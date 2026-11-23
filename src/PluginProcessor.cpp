#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/Parameters.h"
#include "params/Presets.h"

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

siren::SirenParams DubSirenProcessor::readSirenParams() const
{
    siren::SirenParams p;
    p.wave = static_cast<siren::Wave> (choice (params::id::wave));

    // The Pitch knob sets the pitch of C3 (and of the trigger button); other notes transpose it.
    p.pitchSemis = siren::hzToSemis (param (params::id::pitch)) + noteOffset;
    p.glideMs = 0.0;

    p.lfoShape = static_cast<siren::LfoShape> (choice (params::id::lfoShape));
    p.lfoRateHz = param (params::id::lfoRate);
    p.lfoDepthSemis = param (params::id::lfoDepth);
    p.lfoAccel = param (params::id::lfoAccel);

    p.attackMs = params::fixed::attackMs;
    p.releaseMs = param (params::id::release);
    p.sweepSemis = param (params::id::sweep);
    p.level = params::fixed::sirenLevel;
    return p;
}

siren::TapeDelay::Params DubSirenProcessor::readDelayParams (double bpm) const
{
    siren::TapeDelay::Params p;
    const double maxMs = delay.maxDelayMs() - 30.0; // leave room for width and wow
    p.timeMs = juce::jmin (maxMs, flag (params::id::delaySync) ? divisionMs (choice (params::id::delayDivision), bpm)
                                                               : static_cast<double> (param (params::id::delayTime)));
    const float tape = param (params::id::tapeAmount);
    p.smoothMs = params::tapeSmoothMs (tape);
    p.wow = params::tapeWow (tape);
    p.feedback = param (params::id::feedback);
    p.lowCutHz = params::fixed::lowCutHz;
    p.highCutHz = param (params::id::highCut);
    p.widthMs = params::fixed::widthMs;
    p.kill = flag (params::id::echoKill);
    p.freeze = flag (params::id::echoFreeze);
    return p;
}

void DubSirenProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        noteOffset = note - params::fixed::referenceNote;
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        if (! heldNotes.empty())
            noteOffset = heldNotes.back() - params::fixed::referenceNote;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
    }
}

void DubSirenProcessor::updateGate()
{
    voice.setGate (triggerHeld || ! heldNotes.empty(), params::fixed::lfoRetrigger);
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

    // A press of the trigger button on its own plays the Pitch setting itself.
    const bool triggerNow = flag (params::id::trigger);
    if (triggerNow && ! triggerHeld && heldNotes.empty())
        noteOffset = 0;
    triggerHeld = triggerNow;
    updateGate();

    // Render the voice in segments between MIDI events, so notes land sample-accurately.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos)
        {
            const auto sp = readSirenParams();
            voice.process (mono.data() + pos, t - pos, sp);
            pos = t;
        }
        handleMidi (meta.getMessage());
        updateGate();
    }
    if (pos < numSamples)
    {
        const auto sp = readSirenParams();
        voice.process (mono.data() + pos, numSamples - pos, sp);
    }

    delay.process (mono.data(), wetL.data(), wetR.data(), numSamples, readDelayParams (bpm));

    const float echo = param (params::id::echoLevel);
    // Throw mutes the dry siren (with a short fade) so only the echoes are heard.
    const float dryTarget = flag (params::id::echoThrow) ? 0.0f : 1.0f;
    const float dryCoef = 1.0f - std::exp (-1.0f / (0.005f * static_cast<float> (getSampleRate())));
    const float targetGain = juce::Decibels::decibelsToGain (param (params::id::output));
    if (outputGainSmoothed < 0.0f)
        outputGainSmoothed = targetGain;
    const float gainStep = (targetGain - outputGainSmoothed) / static_cast<float> (juce::jmax (1, numSamples));

    // Drive follows its knob over the block (no zipper noise).
    const float targetDrive = param (params::id::drive);
    if (driveSmoothed < 0.0f)
        driveSmoothed = targetDrive;
    const float driveStep = (targetDrive - driveSmoothed) / static_cast<float> (juce::jmax (1, numSamples));

    const int numChannels = buffer.getNumChannels();
    auto* outL = numChannels > 0 ? buffer.getWritePointer (0) : nullptr;
    auto* outR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        outputGainSmoothed += gainStep;
        dryGain += dryCoef * (dryTarget - dryGain);
        const float dry = dryGain * mono[static_cast<size_t> (i)];
        driveSmoothed += driveStep;
        const auto shape = siren::DriveShape::forAmount (driveSmoothed);
        const float l = shape.process (dry + echo * wetL[static_cast<size_t> (i)]) * outputGainSmoothed;
        const float r = shape.process (dry + echo * wetR[static_cast<size_t> (i)]) * outputGainSmoothed;
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
    driveSmoothed = targetDrive;

    gateActive = voice.getGate();
}

juce::AudioProcessorEditor* DubSirenProcessor::createEditor()
{
    return new DubSirenEditor (*this);
}

int DubSirenProcessor::getNumPrograms()
{
    return params::numPresets;
}

const juce::String DubSirenProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, params::numPresets) ? params::presets[index].name : "";
}

void DubSirenProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, params::numPresets))
        return;
    const auto& preset = params::presets[index];
    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || params::presetSkips (ranged->getParameterID()))
            continue;
        float value = ranged->getDefaultValue();
        for (const auto& [pid, real] : preset.values)
            if (ranged->getParameterID() == pid)
                value = ranged->convertTo0to1 (real);
        ranged->setValueNotifyingHost (value);
    }
    currentPreset = index;
}

void DubSirenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DubSirenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPreset = juce::jlimit (0, params::numPresets - 1, static_cast<int> (apvts.state.getProperty ("preset", 0)));
    }

    // Performance buttons are momentary (or a live state): never restore them as held.
    for (auto* pid : { params::id::trigger, params::id::echoThrow, params::id::echoKill, params::id::echoFreeze })
        if (auto* button = apvts.getParameter (pid))
            button->setValueNotifyingHost (0.0f);

    // Tell the host every value again: a parameter whose stored value did not change (a bool that
    // was set to 0.7 and is restored as 1, say) sends no notification, and the host's copy would stay stale.
    for (auto* p : getParameters())
        p->sendValueChangedMessageToListeners (p->getValue());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DubSirenProcessor();
}
