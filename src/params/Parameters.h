#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

namespace params
{
namespace id
{
    inline constexpr const char* trigger       = "trigger";
    inline constexpr const char* wave          = "wave";
    inline constexpr const char* pitch         = "pitch";
    inline constexpr const char* lfoShape      = "lfoShape";
    inline constexpr const char* lfoRate       = "lfoRate";
    inline constexpr const char* lfoDepth      = "lfoDepth";
    inline constexpr const char* lfoAccel      = "lfoAccel";
    inline constexpr const char* release       = "release";
    inline constexpr const char* sweep         = "sweep";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* delaySync     = "delaySync";
    inline constexpr const char* delayDivision = "delayDivision";
    inline constexpr const char* feedback      = "feedback";
    inline constexpr const char* tapeAmount    = "tapeAmount";
    inline constexpr const char* highCut       = "highCut";
    inline constexpr const char* echoLevel     = "echoLevel";
    inline constexpr const char* echoThrow     = "echoThrow";
    inline constexpr const char* echoKill      = "echoKill";
    inline constexpr const char* echoFreeze    = "echoFreeze";
    inline constexpr const char* output        = "output";
} // namespace id

// Fixed values for what used to be separate controls.
namespace fixed
{
    inline constexpr double attackMs = 2.0;
    inline constexpr float sirenLevel = 0.5f;
    inline constexpr double lowCutHz = 150.0;
    inline constexpr double widthMs = 7.5;     // extra delay on the right channel
    inline constexpr bool lfoRetrigger = true;
    inline constexpr int referenceNote = 60;   // C3 (Ableton naming): plays at the Pitch setting
} // namespace fixed

// The Tape knob (0..1) sets both how slowly the echo time follows the Time knob
// and how much wow and flutter the tape has.
inline double tapeSmoothMs (float amount) { return 20.0 * std::pow (150.0, static_cast<double> (amount)); } // 20 ms .. 3 s
inline double tapeWow (float amount) { return std::pow (static_cast<double> (amount), 1.5); }

inline const juce::StringArray waveNames { "Square", "Saw", "Triangle", "Sine" };
inline const juce::StringArray lfoShapeNames { "Square", "Triangle", "Saw Up", "Saw Down", "Sine", "S&H", "Rotor" };
inline constexpr int rotorShapeIndex = 6;
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

    // Siren
    addChoice (id::wave, "Wave", waveNames, 0);
    addFloat (id::pitch, "Pitch", skewed (30.0f, 3000.0f, 400.0f), 440.0f, hz);

    // LFO (restarts on every press; Rotor keeps its momentum)
    addChoice (id::lfoShape, "LFO Shape", lfoShapeNames, 0);
    addFloat (id::lfoRate, "LFO Rate", skewed (0.05f, 40.0f, 4.0f), 4.0f, hz);
    addFloat (id::lfoDepth, "LFO Depth", NormalisableRange<float> (0.0f, 36.0f), 12.0f, fmt (1, " st"));
    addFloat (id::lfoAccel, "LFO Accel", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, percent);

    // Decay
    addFloat (id::release, "Release", skewed (5.0f, 5000.0f, 400.0f), 250.0f, ms);
    addFloat (id::sweep, "Sweep", NormalisableRange<float> (-48.0f, 48.0f), -12.0f, fmt (1, " st"));

    // Echo
    addBool (id::delaySync, "Echo Sync", true);
    addFloat (id::delayTime, "Echo Time", skewed (10.0f, 2000.0f, 350.0f), 375.0f, ms);
    addChoice (id::delayDivision, "Echo Division", divisionNames, 6);
    addFloat (id::feedback, "Feedback", NormalisableRange<float> (0.0f, 1.2f), 0.6f, percent);
    addFloat (id::tapeAmount, "Tape", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addFloat (id::highCut, "Tone", skewed (300.0f, 18000.0f, 3000.0f), 3500.0f, hz);
    addFloat (id::echoLevel, "Echo Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);

    // Echo performance buttons
    addBool (id::echoThrow, "Echo Throw", false);    // momentary: mute the dry siren, only echoes
    addBool (id::echoKill, "Echo Kill", false);      // momentary: stop feeding the echo
    addBool (id::echoFreeze, "Echo Freeze", false);  // latching: loop the echo as it is

    // Output
    addFloat (id::output, "Volume", NormalisableRange<float> (-36.0f, 6.0f), -9.0f, fmt (1, " dB"));

    return layout;
}
} // namespace params
