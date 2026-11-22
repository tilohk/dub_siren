#pragma once

#include <algorithm>
#include <cmath>

namespace siren
{
constexpr double kTwoPi = 6.283185307179586;

enum class Wave { Square, Saw, Triangle, Sine };

// PolyBLEP residual: smooths a unit step discontinuity at phase 0 (t, dt in cycles).
inline double polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// Band-limited oscillator (PolyBLEP square and saw), output roughly in [-1, 1].
class Oscillator
{
public:
    void reset (double startPhase = 0.0) { phase = startPhase; }

    float process (double freqHz, double sampleRate, Wave wave)
    {
        const double dt = std::clamp (freqHz / sampleRate, 0.0, 0.49);
        double out = 0.0;

        switch (wave)
        {
            case Wave::Square:
                out = phase < 0.5 ? 1.0 : -1.0;
                out += polyBlep (phase, dt);
                out -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
                break;
            case Wave::Saw:
                out = 2.0 * phase - 1.0 - polyBlep (phase, dt);
                break;
            case Wave::Triangle:
                // Naive triangle: its harmonics fall at 12 dB/oct, so aliasing stays low.
                out = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
                break;
            case Wave::Sine:
                out = std::sin (kTwoPi * phase);
                break;
        }

        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        return static_cast<float> (out);
    }

private:
    double phase = 0.0;
};
} // namespace siren
