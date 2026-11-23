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

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double> (v[i]) * v[i];
    return std::sqrt (sum / static_cast<double> (to - from));
}

// Counts how often the period between rising zero crossings jumps by more than 30 %
// (the pitch steps of a square LFO).
static int countPitchSteps (const std::vector<float>& v, size_t from, size_t to)
{
    int steps = 0;
    size_t last = 0, lastPeriod = 0;
    for (size_t i = from + 1; i < to; ++i)
    {
        if (! (v[i - 1] < 0.0f && v[i] >= 0.0f))
            continue;
        if (last != 0)
        {
            const size_t period = i - last;
            if (lastPeriod != 0 && (period > lastPeriod * 13 / 10 || period * 13 / 10 < lastPeriod))
                ++steps;
            lastPeriod = period;
        }
        last = i;
    }
    return steps;
}

static void testRotor()
{
    using namespace siren;
    SirenVoice voice;
    voice.prepare (sr);

    SirenParams p;
    p.lfoShape = LfoShape::Rotor;
    p.lfoRateHz = 2.0;          // spin-up time constant 0.5 s
    p.lfoDepthSemis = 12.0;     // full speed = one octave up
    p.sweepSemis = 0.0;
    p.releaseMs = 3000.0;       // coasts down over about 1 s

    std::vector<float> out (3 * 48000);
    voice.setGate (true, true);
    voice.process (out.data(), 3 * 48000, p);
    const int start = countRisingZeroCrossings (out, 0, 2400);         // first 50 ms
    const int top = countRisingZeroCrossings (out, 120000, 144000);    // 2.5 .. 3 s
    CHECK (start < 26 && top > 210, "rotor spins up from the base pitch towards +1 octave");

    voice.setGate (false, true);
    voice.process (out.data(), 2 * 48000, p);
    const int justReleased = countRisingZeroCrossings (out, 0, 4800);       // first 0.1 s
    const int coasting = countRisingZeroCrossings (out, 91200, 96000);      // 1.9 .. 2 s after release
    CHECK (voice.getRotorSpeed() < 0.2 && coasting < justReleased * 7 / 10, "rotor coasts down after release");

    // Tapping keeps the momentum: a second press continues from the current speed.
    voice.setGate (true, true);
    const double before = voice.getRotorSpeed();
    voice.process (out.data(), 480, p);
    CHECK (before > 0.05 && voice.getRotorSpeed() > before, "rotor keeps its momentum between presses");
}

static void testAccel()
{
    using namespace siren;
    SirenParams p;
    p.lfoShape = LfoShape::Square;
    p.lfoRateHz = 2.0;
    p.lfoDepthSemis = 12.0;
    p.sweepSemis = 0.0;

    auto stepsPerSecond = [&] (double accel) {
        SirenVoice voice;
        voice.prepare (sr);
        SirenParams q = p;
        q.lfoAccel = accel;
        std::vector<float> out (4 * 48000);
        voice.setGate (true, true);
        voice.process (out.data(), 4 * 48000, q);
        return std::make_pair (countPitchSteps (out, 0, 48000), countPitchSteps (out, 3 * 48000, 4 * 48000));
    };

    const auto [steady0, steady3] = stepsPerSecond (0.0);
    CHECK (std::abs (steady0 - steady3) <= 2, "Accel at 0 keeps the LFO rate steady");
    const auto [up0, up3] = stepsPerSecond (1.0);
    CHECK (up3 > 3 * up0, "positive Accel speeds the LFO up while held");
    const auto [down0, down3] = stepsPerSecond (-1.0);
    CHECK (down3 < down0, "negative Accel slows the LFO down while held");
}

static void testKillAndFreeze()
{
    using namespace siren;
    TapeDelay::Params p;
    p.timeMs = 200.0;
    p.feedback = 0.5;
    p.wow = 0.5;

    std::vector<float> noise (48000), l (48000), r (48000);
    uint32_t seed = 7;
    auto fillNoise = [&] {
        for (auto& s : noise)
        {
            seed = seed * 1664525u + 1013904223u;
            s = static_cast<float> (seed >> 8) / 16777216.0f - 0.5f;
        }
    };

    // Kill: input keeps playing, but the echo stops being fed and dies away.
    TapeDelay kill;
    kill.prepare (sr, 4.0);
    fillNoise();
    kill.process (noise.data(), l.data(), r.data(), 48000, p);
    const double running = rms (l, 24000, 48000);
    p.kill = true;
    for (int s = 0; s < 3; ++s)
    {
        fillNoise();
        kill.process (noise.data(), l.data(), r.data(), 48000, p);
    }
    CHECK (running > 0.05 && rms (l, 0, 48000) < running * 0.01, "Kill stops feeding the echo");

    // Freeze: the loop keeps its level for a long time and ignores new input.
    TapeDelay frz;
    frz.prepare (sr, 4.0);
    p.kill = false;
    fillNoise();
    frz.process (noise.data(), l.data(), r.data(), 48000, p);
    p.freeze = true;
    frz.process (noise.data(), l.data(), r.data(), 48000, p);
    const double frozenStart = rms (l, 24000, 48000);
    std::vector<float> silence (48000, 0.0f), loud (48000, 0.9f);
    for (int s = 0; s < 9; ++s)
        frz.process (s % 2 == 0 ? silence.data() : loud.data(), l.data(), r.data(), 48000, p);
    const double frozenEnd = rms (l, 0, 48000);
    CHECK (frozenStart > 0.05 && frozenEnd > frozenStart * 0.6 && frozenEnd < frozenStart * 1.4,
           "Freeze holds the loop for 10 s and ignores new input");

    p.freeze = false;
    p.feedback = 0.0;
    for (int s = 0; s < 2; ++s)
        frz.process (silence.data(), l.data(), r.data(), 48000, p);
    CHECK (rms (l, 0, 48000) < 1.0e-4, "releasing Freeze lets the echo die away again");
}

int main()
{
    testOscillator();
    testLfo();
    testVoice();
    testDelay();
    testRotor();
    testAccel();
    testKillAndFreeze();
    std::printf (failures == 0 ? "\nAll DSP tests passed\n" : "\n%d DSP test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
