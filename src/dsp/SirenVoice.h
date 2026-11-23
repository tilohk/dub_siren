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
    double lfoRateHz = 4.0;        // LFO speed; for Rotor: 1 / spin-up time
    double lfoDepthSemis = 12.0;
    double lfoAccel = 0.0;         // -1 .. 1: LFO slows down / speeds up while held
    double attackMs = 2.0;
    double releaseMs = 300.0;
    double sweepSemis = -12.0;     // pitch change over the release
    float level = 0.5f;
};

inline double semisToHz (double semis) { return 440.0 * std::exp2 (semis / 12.0); }
inline double hzToSemis (double hz) { return 12.0 * std::log2 (hz / 440.0); }

// One monophonic siren: oscillator, pitch LFO (or rotor), AR envelope with release pitch sweep.
class SirenVoice
{
public:
    // Accel at full scale changes the LFO rate by this many octaves per second of holding.
    static constexpr double kAccelOctavesPerSecond = 1.5;
    static constexpr double kAccelMaxOctaves = 4.0;

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
        rotorSpeed = 0.0;
        accelOctaves = 0.0;
    }

    // retriggerLfo: restart the LFO cycle on every press (otherwise it free-runs).
    // The rotor never restarts: it keeps its momentum between presses.
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
            {
                lfo.reset();
                accelOctaves = 0.0;
            }
            env.noteOn();
        }
        else
        {
            env.noteOff();
        }
    }

    bool getGate() const { return gate; }
    bool isSounding() const { return env.isActive(); }
    double getRotorSpeed() const { return rotorSpeed; }

    void process (float* out, int numSamples, const SirenParams& p)
    {
        const double glideCoef = p.glideMs <= 0.5 ? 0.0 : std::exp (-1.0 / (p.glideMs * 0.001 * sampleRate));
        const float levelCoef = static_cast<float> (std::exp (-1.0 / (0.01 * sampleRate)));
        const double maxHz = sampleRate * 0.45;
        const bool rotor = p.lfoShape == LfoShape::Rotor;

        // Rotor: spins up towards full speed while held (time constant 1 / rate) and coasts
        // down after release (a third of the release time, so it has mostly stopped when the
        // tone fades out). Its speed maps linearly to frequency, like a real siren's rotor.
        const double spinUpSeconds = 1.0 / std::max (0.01, p.lfoRateHz);
        const double coastSeconds = std::max (0.01, p.releaseMs * 0.001 / 3.0);
        const double spinUpCoef = 1.0 - std::exp (-1.0 / (spinUpSeconds * sampleRate));
        const double coastCoef = 1.0 - std::exp (-1.0 / (coastSeconds * sampleRate));
        const double rotorTopRatio = std::exp2 (p.lfoDepthSemis / 12.0) - 1.0;

        const double accelStep = p.lfoAccel * kAccelOctavesPerSecond / sampleRate;

        if (snapPitch)
        {
            currentSemis = p.pitchSemis;
            snapPitch = false;
        }
        if (levelSmoothed < 0.0f)
            levelSmoothed = p.level;

        for (int i = 0; i < numSamples; ++i)
        {
            double modSemis;
            if (rotor)
            {
                rotorSpeed += gate ? spinUpCoef * (1.0 - rotorSpeed) : -coastCoef * rotorSpeed;
                modSemis = 12.0 * std::log2 (1.0 + rotorSpeed * rotorTopRatio);
            }
            else
            {
                rotorSpeed -= coastCoef * rotorSpeed;
                if (gate)
                    accelOctaves = std::clamp (accelOctaves + accelStep, -kAccelMaxOctaves, kAccelMaxOctaves);
                const double rate = std::clamp (p.lfoRateHz * std::exp2 (accelOctaves), 0.01, 80.0);
                modSemis = lfo.process (rate, sampleRate, p.lfoShape) * p.lfoDepthSemis;
            }

            currentSemis = p.pitchSemis + glideCoef * (currentSemis - p.pitchSemis);
            levelSmoothed = p.level + levelCoef * (levelSmoothed - p.level);

            if (! env.isActive())
            {
                out[i] = 0.0f;
                continue;
            }

            const float amp = env.process (sampleRate, p.attackMs, p.releaseMs);
            const double semis = currentSemis + modSemis + p.sweepSemis * env.sweepPosition();
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
    double rotorSpeed = 0.0;     // 0 = stopped, 1 = full speed
    double accelOctaves = 0.0;   // current LFO rate offset from Accel
};
} // namespace siren
