#pragma once

#include "Envelope.h"
#include "Lfo.h"
#include "Oscillator.h"

#include <algorithm>
#include <cmath>

namespace siren
{
struct SirenParams
{
    Wave wave = Wave::Square;
    double pitchSemis = 0.0;       // target pitch in semitones relative to A4 (440 Hz)
    double glideMs = 0.0;
    LfoShape lfoShape = LfoShape::Square;
    double lfoRateHz = 4.0;
    double lfoDepthSemis = 12.0;
    double attackMs = 2.0;
    double releaseMs = 300.0;
    double sweepSemis = -12.0;     // pitch change over the release
    float level = 0.5f;
};

inline double semisToHz (double semis) { return 440.0 * std::exp2 (semis / 12.0); }
inline double hzToSemis (double hz) { return 12.0 * std::log2 (hz / 440.0); }

// One monophonic siren: oscillator, pitch LFO, AR envelope with release pitch sweep.
class SirenVoice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        osc.reset();
        lfo.reset();
        env.reset();
        gate = false;
        snapPitch = true;
        levelSmoothed = -1.0f;
    }

    // retriggerLfo: restart the LFO cycle on every press (otherwise it free-runs).
    void setGate (bool on, bool retriggerLfo)
    {
        if (on == gate)
            return;
        gate = on;
        if (on)
        {
            if (! env.isActive())
            {
                osc.reset();
                snapPitch = true;
            }
            if (retriggerLfo || ! env.isActive())
                lfo.reset();
            env.noteOn();
        }
        else
        {
            env.noteOff();
        }
    }

    bool getGate() const { return gate; }
    bool isSounding() const { return env.isActive(); }

    void process (float* out, int numSamples, const SirenParams& p)
    {
        const double glideCoef = p.glideMs <= 0.5 ? 0.0 : std::exp (-1.0 / (p.glideMs * 0.001 * sampleRate));
        const float levelCoef = static_cast<float> (std::exp (-1.0 / (0.01 * sampleRate)));
        const double maxHz = sampleRate * 0.45;

        if (snapPitch)
        {
            currentSemis = p.pitchSemis;
            snapPitch = false;
        }
        if (levelSmoothed < 0.0f)
            levelSmoothed = p.level;

        for (int i = 0; i < numSamples; ++i)
        {
            const float lfoValue = lfo.process (p.lfoRateHz, sampleRate, p.lfoShape);
            currentSemis = p.pitchSemis + glideCoef * (currentSemis - p.pitchSemis);
            levelSmoothed = p.level + levelCoef * (levelSmoothed - p.level);

            if (! env.isActive())
            {
                out[i] = 0.0f;
                continue;
            }

            const float amp = env.process (sampleRate, p.attackMs, p.releaseMs);
            const double semis = currentSemis + lfoValue * p.lfoDepthSemis + p.sweepSemis * env.sweepPosition();
            const double hz = std::clamp (semisToHz (semis), 5.0, maxHz);
            out[i] = osc.process (hz, sampleRate, p.wave) * amp * levelSmoothed;
        }
    }

private:
    double sampleRate = 44100.0;
    Oscillator osc;
    Lfo lfo;
    Envelope env;
    bool gate = false;
    bool snapPitch = true;
    double currentSemis = 0.0;
    float levelSmoothed = -1.0f;
};
} // namespace siren
