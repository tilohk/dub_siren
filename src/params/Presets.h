#pragma once

#include "Parameters.h"

#include <initializer_list>
#include <utility>

namespace params
{
// A factory preset: a name and the values it changes. Everything else goes back to its
// default when it loads; Volume and the performance buttons are left alone.
struct Preset
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;   // real (unnormalised) values
};

// Choice values are indices: wave Square/Saw/Triangle/Sine, LFO shape Square/Triangle/
// Saw Up/Saw Down/Sine/S&H/Rotor, division as in divisionNames (5 = 1/8, 6 = 1/8D, 8 = 1/4, 9 = 1/4D).
inline const Preset presets[] {
    { "Classic Siren", {} },
    { "Rising Alarm",
      { { id::wave, 1.0f }, { id::pitch, 300.0f }, { id::lfoShape, 2.0f }, { id::lfoRate, 1.5f },
        { id::release, 600.0f }, { id::sweep, -24.0f }, { id::delayDivision, 8.0f } } },
    { "Laser Zap",
      { { id::pitch, 1200.0f }, { id::lfoShape, 3.0f }, { id::lfoRate, 12.0f }, { id::lfoDepth, 24.0f },
        { id::release, 120.0f }, { id::sweep, -36.0f }, { id::delayDivision, 5.0f }, { id::feedback, 0.5f },
        { id::highCut, 5000.0f } } },
    { "Rotor Wind-up",
      { { id::wave, 2.0f }, { id::pitch, 500.0f }, { id::lfoShape, 6.0f }, { id::lfoRate, 2.0f },
        { id::lfoDepth, 18.0f }, { id::release, 800.0f }, { id::delayDivision, 9.0f }, { id::feedback, 0.7f },
        { id::tapeAmount, 0.7f } } },
    { "Space Wobble",
      { { id::wave, 3.0f }, { id::pitch, 220.0f }, { id::lfoShape, 4.0f }, { id::lfoRate, 0.8f },
        { id::lfoDepth, 7.0f }, { id::lfoAccel, 0.4f }, { id::release, 1500.0f }, { id::sweep, 0.0f },
        { id::feedback, 0.85f }, { id::tapeAmount, 0.8f }, { id::highCut, 2200.0f }, { id::echoLevel, 0.6f },
        { id::drive, 0.3f } } },
};
inline constexpr int numPresets = static_cast<int> (std::size (presets));

// Parameters a preset never touches.
inline bool presetSkips (const juce::String& pid)
{
    for (auto* skip : { id::trigger, id::echoThrow, id::echoKill, id::echoFreeze, id::output })
        if (pid == skip)
            return true;
    return false;
}
} // namespace params
