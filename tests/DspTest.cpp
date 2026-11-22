// Offline DSP checks for the siren voice and the tape delay (no JUCE needed).
#include "dsp/SirenVoice.h"
#include "dsp/TapeDelay.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;

#define CHECK(cond, msg)                                     \
    do {                                                     \
        if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } \
        else          { std::printf ("ok:   %s\n", msg); }  \
    } while (0)

static constexpr double sr = 48000.0;

static float peak (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
{
    float m = 0.0f;
    for (size_t i = from; i < std::min (to, v.size()); ++i)
        m = std::max (m, std::fabs (v[i]));
    return m;
}

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}

static int countRisingZeroCrossings (const std::vector<float>& v, size_t from, size_t to)
{
    int n = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
            ++n;
    return n;
}

static void testOscillator()
{
    using namespace siren;
    for (Wave w : { Wave::Square, Wave::Saw, Wave::Triangle, Wave::Sine })
        for (double hz : { 20.0, 440.0, 5000.0, 20000.0 })
        {
            Oscillator osc;
            std::vector<float> out (48000);
            for (auto& s : out)
                s = osc.process (hz, sr, w);
            if (! allFinite (out) || peak (out) > 1.3f)
            {
                std::printf ("FAIL: oscillator wave %d at %.0f Hz (peak %.3f)\n", static_cast<int> (w), hz, peak (out));
                ++failures;
                return;
            }
        }
    std::printf ("ok:   oscillators finite and bounded\n");

    Oscillator osc;
    std::vector<float> out (48000);
    for (auto& s : out)
        s = osc.process (440.0, sr, Wave::Square);
    const int crossings = countRisingZeroCrossings (out, 0, out.size());
    CHECK (std::abs (crossings - 440) <= 1, "square at 440 Hz has 440 cycles per second");
}

static void testLfo()
{
    using namespace siren;
    bool ok = true;
    for (LfoShape s : { LfoShape::Square, LfoShape::Triangle, LfoShape::SawUp, LfoShape::SawDown, LfoShape::Sine, LfoShape::SampleHold })
    {
        Lfo lfo;
        lfo.reset();
        for (int i = 0; i < 48000; ++i)
        {
            const float v = lfo.process (7.3, sr, s);
            ok = ok && v >= 0.0f && v <= 1.0f;
        }
    }
    CHECK (ok, "LFO shapes stay in [0, 1]");
}

static void testVoice()
{
    using namespace siren;
    SirenVoice voice;
    voice.prepare (sr);

    SirenParams p;
    p.lfoDepthSemis = 0.0;
    p.sweepSemis = 0.0;
    p.releaseMs = 100.0;
    p.pitchSemis = 0.0; // 440 Hz

    std::vector<float> out (48000);
    voice.process (out.data(), 4800, p);
    CHECK (peak (out, 0, 4800) == 0.0f, "silent before trigger");

    voice.setGate (true, true);
    voice.process (out.data(), 48000, p);
    CHECK (peak (out, 4800) > 0.3f, "sounds while triggered");
    CHECK (std::abs (countRisingZeroCrossings (out, 0, out.size()) - 440) <= 2, "fixed pitch plays 440 Hz");

    voice.setGate (false, true);
    voice.process (out.data(), 48000, p);
    CHECK (! voice.isSounding(), "voice goes idle after the release");
    CHECK (peak (out, 9600) == 0.0f, "silent after the release");

    // LFO square with 12 semitones depth: alternates 440 / 880 Hz.
    SirenVoice v2;
    v2.prepare (sr);
    SirenParams q = p;
    q.lfoShape = LfoShape::Square;
    q.lfoRateHz = 1.0;
    q.lfoDepthSemis = 12.0;
    v2.setGate (true, true);
    v2.process (out.data(), 48000, q);
    const int lowHalf = countRisingZeroCrossings (out, 0, 24000);
    const int highHalf = countRisingZeroCrossings (out, 24000, 48000);
    CHECK (std::abs (lowHalf - 220) <= 2 && std::abs (highHalf - 440) <= 2, "square LFO alternates base and +1 octave");

    // Release sweep: -12 st over a 1 s release lowers the pitch.
    SirenVoice v3;
    v3.prepare (sr);
    SirenParams r = p;
    r.releaseMs = 1000.0;
    r.sweepSemis = -12.0;
    v3.setGate (true, true);
    v3.process (out.data(), 4800, r);
    v3.setGate (false, true);
    v3.process (out.data(), 48000, r);
    const int firstTenth = countRisingZeroCrossings (out, 0, 4800);
    const int lateTenth = countRisingZeroCrossings (out, 33600, 38400);
    CHECK (lateTenth < firstTenth - 10, "release sweep drops the pitch");
}

static void testDelay()
{
    using namespace siren;
    TapeDelay d;
    d.prepare (sr, 4.0);

    TapeDelay::Params p;
    p.timeMs = 100.0;
    p.feedback = 0.0;
    p.wow = 0.0;
    p.lowCutHz = 5.0;
    p.highCutHz = 20000.0;

    std::vector<float> in (9600, 0.0f), l (9600), r (9600);
    in[0] = 0.5f;
    d.process (in.data(), l.data(), r.data(), 9600, p);
    size_t peakIdx = 0;
    for (size_t i = 0; i < l.size(); ++i)
        if (std::fabs (l[i]) > std::fabs (l[peakIdx]))
            peakIdx = i;
    CHECK (peakIdx >= 4799 && peakIdx <= 4802, "echo arrives after 100 ms");

    // Feedback above 100 % with noise input for 20 s: finite and bounded.
    TapeDelay d2;
    d2.prepare (sr, 4.0);
    TapeDelay::Params q;
    q.timeMs = 50.0;
    q.feedback = 1.2;
    q.wow = 1.0;
    std::vector<float> noise (48000), wl (48000), wr (48000);
    uint32_t seed = 1;
    bool ok = true;
    float maxOut = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        for (auto& s : noise)
        {
            seed = seed * 1664525u + 1013904223u;
            s = (static_cast<float> (seed >> 8) / 16777216.0f - 0.5f) * 1.0f;
        }
        q.timeMs = 50.0 + 400.0 * (block % 3); // jump the time around while running
        d2.process (noise.data(), wl.data(), wr.data(), 48000, q);
        ok = ok && allFinite (wl) && allFinite (wr);
        maxOut = std::max ({ maxOut, peak (wl), peak (wr) });
    }
    CHECK (ok && maxOut < 2.0f, "120% feedback stays finite and bounded");

    CHECK (softLimit (10.0f) <= 1.0f && softLimit (0.5f) == 0.5f, "soft limiter is transparent below 0.8 and caps at 1");
}

int main()
{
    testOscillator();
    testLfo();
    testVoice();
    testDelay();
    std::printf (failures == 0 ? "\nAll DSP tests passed\n" : "\n%d DSP test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
