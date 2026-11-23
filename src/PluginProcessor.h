#pragma once

#include "dsp/Drive.h"
#include "dsp/SirenVoice.h"
#include "dsp/TapeDelay.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

class DubSirenProcessor : public juce::AudioProcessor
{
public:
    DubSirenProcessor();
    ~DubSirenProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // For the UI: true while the siren is triggered (button, automation or MIDI).
    std::atomic<bool> gateActive { false };

private:
    void handleMidi (const juce::MidiMessage& m);
    void updateGate();
    siren::SirenParams readSirenParams() const;
    siren::TapeDelay::Params readDelayParams (double bpm) const;

    float param (const char* pid) const { return apvts.getRawParameterValue (pid)->load(); }
    int choice (const char* pid) const { return juce::roundToInt (param (pid)); }
    bool flag (const char* pid) const { return param (pid) > 0.5f; }

    siren::SirenVoice voice;
    siren::TapeDelay delay;
    std::vector<float> mono, wetL, wetR;

    std::vector<int> heldNotes;   // MIDI notes in press order (last one wins)
    int noteOffset = 0;           // semitones from C3 of the last note played
    bool triggerHeld = false;
    float outputGainSmoothed = -1.0f;
    float dryGain = 1.0f;
    float driveSmoothed = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubSirenProcessor)
};
