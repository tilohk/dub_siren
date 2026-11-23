#pragma once

#include <cmath>
#include <cstdint>

namespace siren
{
// Rotor is not a periodic shape: SirenVoice models it as a spinning rotor (see there).
enum class LfoShape { Square, Triangle, SawUp, SawDown, Sine, SampleHold, Rotor };

// Unipolar LFO, output in [0, 1]. A cycle starts at 0 (the base pitch) for every
// shape except SawDown, which starts at the top and falls: the classic "laser zap".
class Lfo
{
public:
    void reset()
    {
        phase = 0.0;
        held = nextRandom();
    }

    float process (double rateHz, double sampleRate, LfoShape shape)
    {
        const float value = valueAt (phase, shape);
        phase += rateHz / sampleRate;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = nextRandom();
        }
        return value;
    }

    static float valueAtPhase (double p, LfoShape shape, float sampleHoldValue = 0.0f)
    {
        switch (shape)
        {
            case LfoShape::Square:     return p < 0.5 ? 0.0f : 1.0f;
            case LfoShape::Triangle:   return static_cast<float> (p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p);
            case LfoShape::SawUp:      return static_cast<float> (p);
            case LfoShape::SawDown:    return static_cast<float> (1.0 - p);
            case LfoShape::Sine:       return static_cast<float> (0.5 - 0.5 * std::cos (6.283185307179586 * p));
            case LfoShape::SampleHold: return sampleHoldValue;
            case LfoShape::Rotor:      return 0.0f;
        }
        return 0.0f;
    }

private:
    float valueAt (double p, LfoShape shape) const { return valueAtPhase (p, shape, held); }

    float nextRandom()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return static_cast<float> (rng >> 8) / static_cast<float> (1u << 24);
    }

    double phase = 0.0;
    float held = 0.0f;
    uint32_t rng = 0x9E3779B9u;
};
} // namespace siren
