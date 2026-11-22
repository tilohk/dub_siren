#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
namespace id
{
    inline constexpr const char* trigger      = "trigger";
    inline constexpr const char* wave         = "wave";
    inline constexpr const char* pitchMode    = "pitchMode";
    inline constexpr const char* pitch        = "pitch";
    inline constexpr const char* glide        = "glide";
    inline constexpr const char* lfoShape     = "lfoShape";
    inline constexpr const char* lfoRate      = "lfoRate";
    inline constexpr const char* lfoSync      = "lfoSync";
    inline constexpr const char* lfoDivision  = "lfoDivision";
    inline constexpr const char* lfoDepth     = "lfoDepth";
    inline constexpr const char* lfoRetrigger = "lfoRetrigger";
    inline constexpr const char* attack       = "attack";
    inline constexpr const char* release      = "release";
    inline constexpr const char* sweep        = "sweep";
    inline constexpr const char* delayTime    = "delayTime";
    inline constexpr const char* delaySync    = "delaySync";
    inline constexpr const char* delayDivision= "delayDivision";
    inline constexpr const char* feedback     = "feedback";
    inline constexpr const char* tape         = "tape";
    inline constexpr const char* wow          = "wow";
    inline constexpr const char* lowCut       = "lowCut";
    inline constexpr const char* highCut      = "highCut";
    inline constexpr const char* width        = "width";
    inline constexpr const char* echoLevel    = "echoLevel";
    inline constexpr const char* sirenLevel   = "sirenLevel";
    inline constexpr const char* output       = "output";
} // namespace id

inline const juce::StringArray waveNames { "Square", "Saw", "Triangle", "Sine" };
inline const juce::StringArray pitchModeNames { "Fixed", "MIDI" };
inline const juce::StringArray lfoShapeNames { "Square", "Triangle", "Saw Up", "Saw Down", "Sine", "S&H" };
inline const juce::StringArray divisionNames { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                               "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1" };
// Length of each division in quarter-note beats.
inline constexpr double divisionBeats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                                          2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0 };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto skewed = [] (float lo, float hi, float centre) {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto fmt = [] (int decimals, const char* unit) {
        return AudioParameterFloatAttributes().withStringFromValueFunction (
            [decimals, unit] (float v, int) { return String (v, decimals) + unit; });
    };
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, v < 10.0f ? 2 : 0) + " Hz";
    });
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (v, v < 10.0f ? 1 : 0) + " ms";
    });
    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return String (roundToInt (v * 100.0f)) + " %";
    });

    auto addFloat = [&] (const char* pid, const char* name, NormalisableRange<float> range, float def,
                         AudioParameterFloatAttributes attrs) {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def, attrs));
    };
    auto addChoice = [&] (const char* pid, const char* name, const StringArray& choices, int def) {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, choices, def));
    };
    auto addBool = [&] (const char* pid, const char* name, bool def) {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
    };

    addBool (id::trigger, "Trigger", false);

    // Source
    addChoice (id::wave, "Wave", waveNames, 0);
    addChoice (id::pitchMode, "Pitch Mode", pitchModeNames, 0);
    addFloat (id::pitch, "Pitch", skewed (30.0f, 3000.0f, 400.0f), 440.0f, hz);
    addFloat (id::glide, "Glide", skewed (0.0f, 2000.0f, 150.0f), 0.0f, ms);

    // LFO
    addChoice (id::lfoShape, "LFO Shape", lfoShapeNames, 0);
    addFloat (id::lfoRate, "LFO Rate", skewed (0.05f, 40.0f, 4.0f), 4.0f, hz);
    addBool (id::lfoSync, "LFO Sync", false);
    addChoice (id::lfoDivision, "LFO Division", divisionNames, 5);
    addFloat (id::lfoDepth, "LFO Depth", NormalisableRange<float> (0.0f, 36.0f), 12.0f, fmt (1, " st"));
    addBool (id::lfoRetrigger, "LFO Retrigger", true);

    // Envelope
    addFloat (id::attack, "Attack", skewed (0.5f, 1000.0f, 50.0f), 2.0f, ms);
    addFloat (id::release, "Release", skewed (5.0f, 5000.0f, 400.0f), 250.0f, ms);
    addFloat (id::sweep, "Sweep", NormalisableRange<float> (-48.0f, 48.0f), -12.0f, fmt (1, " st"));

    // Echo
    addFloat (id::delayTime, "Echo Time", skewed (10.0f, 2000.0f, 350.0f), 375.0f, ms);
    addBool (id::delaySync, "Echo Sync", true);
    addChoice (id::delayDivision, "Echo Division", divisionNames, 6);
    addFloat (id::feedback, "Feedback", NormalisableRange<float> (0.0f, 1.2f), 0.6f, percent);
    addFloat (id::tape, "Tape", skewed (5.0f, 3000.0f, 300.0f), 300.0f, ms);
    addFloat (id::wow, "Wow", NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent);
    addFloat (id::lowCut, "Low Cut", skewed (20.0f, 2000.0f, 200.0f), 120.0f, hz);
    addFloat (id::highCut, "Tone", skewed (300.0f, 18000.0f, 3000.0f), 3500.0f, hz);
    addFloat (id::width, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent);
    addFloat (id::echoLevel, "Echo Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);

    // Output
    addFloat (id::sirenLevel, "Siren Level", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent);
    addFloat (id::output, "Output", NormalisableRange<float> (-36.0f, 6.0f), -9.0f, fmt (1, " dB"));

    return layout;
}
} // namespace params
