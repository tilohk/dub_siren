#pragma once

#include <cmath>

namespace siren
{
// Output drive, like pushing a channel on an old mixer: tanh saturation with up to
// +24 dB into it. The level is matched at an amplitude of 0.5, so turning it up mostly
// adds grit and density rather than volume. Amount 0 leaves the signal untouched.
struct DriveShape
{
    float gain = 1.0f;
    float makeup = 1.0f;
    float amount = 0.0f;

    static DriveShape forAmount (float a)
    {
        DriveShape d;
        d.amount = a;
        d.gain = std::pow (10.0f, a * 24.0f / 20.0f);
        d.makeup = 0.5f / std::tanh (0.5f * d.gain);
        return d;
    }

    float process (float x) const
    {
        if (amount <= 0.0f)
            return x;
        const float driven = std::tanh (gain * x) * makeup;
        return x + amount * (driven - x);
    }
};
} // namespace siren
