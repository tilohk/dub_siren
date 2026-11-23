#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace siren
{
// Stereo tape-style echo.
// - The delay time follows its target slowly ("tape" smoothing), so turning the time
//   knob bends the pitch of the repeats like a tape echo.
// - Low-cut and high-cut filters plus tanh saturation sit inside the feedback loop:
//   feedback above 100% self-oscillates but stays bounded.
// - Wow and flutter modulate the read position.
// - Kill closes the input (the tail rings out); Freeze loops the current buffer forever
//   (no input, unity feedback, no filtering) until it is released.
class TapeDelay
{
public:
    struct Params
    {
        double timeMs = 375.0;
        double smoothMs = 300.0;    // how slowly the delay time follows the knob
        double feedback = 0.6;      // 0 .. 1.2
        double lowCutHz = 120.0;
        double highCutHz = 3500.0;
        double wow = 0.2;           // 0 .. 1
        double widthMs = 0.0;       // extra delay time on the right channel
        bool kill = false;          // stop feeding the echo
        bool freeze = false;        // hold and loop what is in the echo now
    };

    void prepare (double sr, double maxSeconds)
    {
        sampleRate = sr;
        size = static_cast<int> (maxSeconds * sr) + 8;
        for (auto& b : buffer)
            b.assign (static_cast<size_t> (size), 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buffer)
            std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
        currentDelay = { -1.0, -1.0 };
        lpState = { 0.0f, 0.0f };
        hpState = { 0.0f, 0.0f };
        wowPhase = 0.0;
        flutterPhase = 0.0;
        inputGain = 1.0f;
        freezeAmount = 0.0f;
    }

    double maxDelayMs() const { return (size - 8) * 1000.0 / sampleRate; }

    // Reads `in` (mono) and writes the wet echo signal to wetL / wetR.
    void process (const float* in, float* wetL, float* wetR, int numSamples, const Params& p)
    {
        const double smoothCoef = std::exp (-1.0 / (std::max (1.0, p.smoothMs) * 0.001 * sampleRate));
        const float lpA = onePoleCoef (std::min (p.highCutHz, sampleRate * 0.45));
        const float hpA = onePoleCoef (p.lowCutHz);
        const float fb = static_cast<float> (p.feedback);
        const double wowDepth = p.wow * 0.0025 * sampleRate;      // up to 2.5 ms of slow wow
        const double flutterDepth = p.wow * 0.0003 * sampleRate;  // and 0.3 ms of flutter
        const double wowInc = 0.7 / sampleRate;
        const double flutterInc = 9.0 / sampleRate;
        const double maxDelay = static_cast<double> (size - 4);
        const float switchCoef = static_cast<float> (1.0 - std::exp (-1.0 / (0.01 * sampleRate))); // 10 ms fades
        const float inputTarget = (p.kill || p.freeze) ? 0.0f : 1.0f;
        const float freezeTarget = p.freeze ? 1.0f : 0.0f;

        std::array<double, 2> target {
            p.timeMs * 0.001 * sampleRate,
            (p.timeMs + p.widthMs) * 0.001 * sampleRate
        };

        float* wet[2] = { wetL, wetR };

        for (int i = 0; i < numSamples; ++i)
        {
            wowPhase += wowInc;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            flutterPhase += flutterInc;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            inputGain += switchCoef * (inputTarget - inputGain);
            freezeAmount += switchCoef * (freezeTarget - freezeAmount);
            const float input = in[i] * inputGain;
            const double modScale = 1.0 - freezeAmount; // a frozen loop stays steady

            for (size_t ch = 0; ch < 2; ++ch)
            {
                const double t = std::clamp (target[ch], 2.0, maxDelay);
                if (currentDelay[ch] < 0.0)
                    currentDelay[ch] = t;
                currentDelay[ch] = t + smoothCoef * (currentDelay[ch] - t);

                const double chOffset = static_cast<double> (ch) * 0.25; // decorrelate the channels a little
                const double mod = modScale * wowDepth * std::sin (kTwoPiD * (wowPhase + chOffset))
                                 + modScale * flutterDepth * std::sin (kTwoPiD * (flutterPhase + chOffset));
                const double d = std::clamp (currentDelay[ch] + mod, 2.0, maxDelay);

                const float raw = readHermite (ch, d);
                float y = raw;

                lpState[ch] += lpA * (y - lpState[ch]);
                y = lpState[ch];
                hpState[ch] += hpA * (y - hpState[ch]);
                y -= hpState[ch];

                const float normal = std::tanh (input + fb * y);
                buffer[ch][static_cast<size_t> (writePos)] = normal + freezeAmount * (raw - normal);
                wet[ch][i] = y;
            }

            if (++writePos >= size)
                writePos = 0;
        }
    }

private:
    static constexpr double kTwoPiD = 6.283185307179586;

    float onePoleCoef (double hz) const
    {
        return static_cast<float> (1.0 - std::exp (-kTwoPiD * hz / sampleRate));
    }

    float sampleAt (size_t ch, int index) const
    {
        while (index < 0)
            index += size;
        while (index >= size)
            index -= size;
        return buffer[ch][static_cast<size_t> (index)];
    }

    // 4-point Hermite interpolation, `delay` samples behind the write head.
    float readHermite (size_t ch, double delay) const
    {
        const double readPos = static_cast<double> (writePos) - delay;
        const double fl = std::floor (readPos);
        const int i1 = static_cast<int> (fl);
        const float f = static_cast<float> (readPos - fl);

        const float xm1 = sampleAt (ch, i1 - 1);
        const float x0 = sampleAt (ch, i1);
        const float x1 = sampleAt (ch, i1 + 1);
        const float x2 = sampleAt (ch, i1 + 2);

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    double sampleRate = 44100.0;
    int size = 0;
    int writePos = 0;
    std::array<std::vector<float>, 2> buffer;
    std::array<double, 2> currentDelay { -1.0, -1.0 };
    std::array<float, 2> lpState { 0.0f, 0.0f };
    std::array<float, 2> hpState { 0.0f, 0.0f };
    double wowPhase = 0.0;
    double flutterPhase = 0.0;
    float inputGain = 1.0f;
    float freezeAmount = 0.0f;
};

// Gentle output safety: linear up to 0.8, then a tanh knee that never exceeds 1.
inline float softLimit (float x)
{
    const float a = std::fabs (x);
    if (a <= 0.8f)
        return x;
    return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
}
} // namespace siren
