#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/Parameters.h"

namespace
{
constexpr double kMaxDelaySeconds = 4.5;

double divisionMs (int index, double bpm)
{
    index = juce::jlimit (0, static_cast<int> (std::size (params::divisionBeats)) - 1, index);
    return params::divisionBeats[index] * 60000.0 / bpm;
}
} // namespace

DubSirenProcessor::DubSirenProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DubSiren", params::createLayout())
{
    heldNotes.reserve (128);
}

bool DubSirenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void DubSirenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voice.prepare (sampleRate);
    delay.prepare (sampleRate, kMaxDelaySeconds);
    const auto n = static_cast<size_t> (juce::jmax (1, samplesPerBlock));
    mono.assign (n, 0.0f);
    wetL.assign (n, 0.0f);
    wetR.assign (n, 0.0f);
    heldNotes.clear();
    triggerHeld = false;
    outputGainSmoothed = -1.0f;
    gateActive = false;
}

siren::SirenParams DubSirenProcessor::readSirenParams (double bpm) const
{
    siren::SirenParams p;
    p.wave = static_cast<siren::Wave> (choice (params::id::wave));

    const bool midiPitch = choice (params::id::pitchMode) == 1;
    p.pitchSemis = (midiPitch && lastNote >= 0) ? static_cast<double> (lastNote - 69)
                                                : siren::hzToSemis (param (params::id::pitch));
    p.glideMs = param (params::id::glide);

    p.lfoShape = static_cast<siren::LfoShape> (choice (params::id::lfoShape));
    p.lfoRateHz = flag (params::id::lfoSync) ? 1000.0 / divisionMs (choice (params::id::lfoDivision), bpm)
                                             : static_cast<double> (param (params::id::lfoRate));
    p.lfoDepthSemis = param (params::id::lfoDepth);

    p.attackMs = param (params::id::attack);
    p.releaseMs = param (params::id::release);
    p.sweepSemis = param (params::id::sweep);
    p.level = 0.5f * param (params::id::sirenLevel);
    return p;
}

siren::TapeDelay::Params DubSirenProcessor::readDelayParams (double bpm) const
{
    siren::TapeDelay::Params p;
    const double maxMs = delay.maxDelayMs() - 30.0; // leave room for width and wow
    p.timeMs = juce::jmin (maxMs, flag (params::id::delaySync) ? divisionMs (choice (params::id::delayDivision), bpm)
                                                               : static_cast<double> (param (params::id::delayTime)));
    p.smoothMs = param (params::id::tape);
    p.feedback = param (params::id::feedback);
    p.wow = param (params::id::wow);
    p.lowCutHz = param (params::id::lowCut);
    p.highCutHz = param (params::id::highCut);
    p.widthMs = 25.0 * param (params::id::width);
    return p;
}

void DubSirenProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        lastNote = note;
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        if (! heldNotes.empty())
            lastNote = heldNotes.back();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
    }
}

void DubSirenProcessor::updateGate()
{
    voice.setGate (triggerHeld || ! heldNotes.empty(), flag (params::id::lfoRetrigger));
}

void DubSirenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (static_cast<size_t> (numSamples) > mono.size())
    {
        mono.resize (static_cast<size_t> (numSamples));
        wetL.resize (static_cast<size_t> (numSamples));
        wetR.resize (static_cast<size_t> (numSamples));
    }

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *hostBpm);

    triggerHeld = flag (params::id::trigger);
    updateGate();

    // Render the voice in segments between MIDI events, so notes land sample-accurately.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos)
        {
            const auto sp = readSirenParams (bpm);
            voice.process (mono.data() + pos, t - pos, sp);
            pos = t;
        }
        handleMidi (meta.getMessage());
        updateGate();
    }
    if (pos < numSamples)
    {
        const auto sp = readSirenParams (bpm);
        voice.process (mono.data() + pos, numSamples - pos, sp);
    }

    delay.process (mono.data(), wetL.data(), wetR.data(), numSamples, readDelayParams (bpm));

    const float echo = param (params::id::echoLevel);
    const float targetGain = juce::Decibels::decibelsToGain (param (params::id::output));
    if (outputGainSmoothed < 0.0f)
        outputGainSmoothed = targetGain;
    const float gainStep = (targetGain - outputGainSmoothed) / static_cast<float> (juce::jmax (1, numSamples));

    const int numChannels = buffer.getNumChannels();
    auto* outL = numChannels > 0 ? buffer.getWritePointer (0) : nullptr;
    auto* outR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        outputGainSmoothed += gainStep;
        const float l = (mono[static_cast<size_t> (i)] + echo * wetL[static_cast<size_t> (i)]) * outputGainSmoothed;
        const float r = (mono[static_cast<size_t> (i)] + echo * wetR[static_cast<size_t> (i)]) * outputGainSmoothed;
        if (outR != nullptr)
        {
            outL[i] = siren::softLimit (l);
            outR[i] = siren::softLimit (r);
        }
        else if (outL != nullptr)
        {
            outL[i] = siren::softLimit (0.5f * (l + r));
        }
    }
    outputGainSmoothed = targetGain;

    gateActive = voice.getGate();
}

juce::AudioProcessorEditor* DubSirenProcessor::createEditor()
{
    return new DubSirenEditor (*this);
}

void DubSirenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DubSirenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // The trigger is momentary: never restore it as held.
    if (auto* trig = apvts.getParameter (params::id::trigger))
        trig->setValueNotifyingHost (0.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DubSirenProcessor();
}
