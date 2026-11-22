#pragma once

#include <algorithm>

namespace siren
{
// Attack / release envelope with a "sweep position" for the release pitch sweep.
// sweepPosition() runs 0 -> 1 over the release, so the pitch falls (or rises) while
// the tone dies away, and returns to 0 when the siren is triggered again.
class Envelope
{
public:
    void reset()
    {
        stage = Stage::Idle;
        level = 0.0;
        sweep = 0.0;
    }

    void noteOn()
    {
        if (stage == Stage::Idle)
            sweep = 0.0;
        stage = Stage::Attack;
    }

    void noteOff()
    {
        if (stage == Stage::Idle || stage == Stage::Release)
            return;
        stage = Stage::Release;
        releaseStart = level;
        releasePos = 0.0;
    }

    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Release; }
    double sweepPosition() const { return sweep; }

    float process (double sampleRate, double attackMs, double releaseMs)
    {
        const double sweepBackStep = 1.0 / (0.005 * sampleRate); // at most 5 ms back to the base pitch

        switch (stage)
        {
            case Stage::Attack:
            {
                const double inc = 1.0 / std::max (1.0, attackMs * 0.001 * sampleRate);
                level += inc;
                sweep = std::max (0.0, sweep - std::max (inc, sweepBackStep));
                if (level >= 1.0)
                {
                    level = 1.0;
                    stage = Stage::Sustain;
                }
                break;
            }
            case Stage::Sustain:
                sweep = std::max (0.0, sweep - sweepBackStep);
                break;
            case Stage::Release:
            {
                releasePos += 1.0 / std::max (1.0, releaseMs * 0.001 * sampleRate);
                if (releasePos >= 1.0)
                {
                    releasePos = 1.0;
                    level = 0.0;
                    stage = Stage::Idle;
                }
                else
                {
                    const double g = 1.0 - releasePos;
                    level = releaseStart * g * g;
                }
                sweep = releasePos;
                break;
            }
            case Stage::Idle:
                level = 0.0;
                break;
        }
        return static_cast<float> (level);
    }

private:
    enum class Stage { Idle, Attack, Sustain, Release };

    Stage stage = Stage::Idle;
    double level = 0.0;
    double sweep = 0.0;
    double releaseStart = 0.0;
    double releasePos = 0.0;
};
} // namespace siren
