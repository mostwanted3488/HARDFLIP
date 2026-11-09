// HARDFLIP clip curve tests (no JUCE): g++ -std=c++17 -O2 Tests/dsp_test.cpp -o dsp_test && ./dsp_test
#include "../Source/DSP/ClipDsp.h"
#include <cstdio>
#include <cmath>
#include <limits>
#include <vector>

static int fails = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++fails; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

int main()
{
    using namespace hardflip;
    const std::vector<float> threshDb { -24.4f, -18.f, -12.f, -6.f, -3.f, -1.f, -0.3f, 0.0f };

    for (float tdb : threshDb)
    {
        const float t = dbToGain (tdb);
        for (int mode = 0; mode < 2; ++mode)
        {
            const float h = (float) mode;
            float prev = -1.0e9f, maxOut = 0.0f, maxJump = 0.0f, prevY = 0.0f;
            for (int i = -40000; i <= 40000; ++i)
            {
                const float x = (float) i * 0.0001f;   // -4 .. +4 (up to +12 dBFS)
                const float y = clip (x, t, h);
                CHECK (std::isfinite (y), "non-finite output t=%g mode=%d x=%g", t, mode, x);
                if (std::abs (x) <= t) CHECK (std::abs (y - x) < 1e-6f, "not unity below threshold t=%g mode=%d x=%g y=%g", t, mode, x, y);
                CHECK (y >= prev - 1e-7f, "not monotonic t=%g mode=%d x=%g", t, mode, x);
                CHECK (std::abs (clip (-x, t, h) + y) < 1e-6f, "not symmetric t=%g mode=%d x=%g", t, mode, x);
                if (i > -40000) maxJump = std::max (maxJump, std::abs (y - prevY));
                prev = y; prevY = y;
                maxOut = std::max (maxOut, std::abs (y));
            }
            if (mode == 1) CHECK (maxOut <= t + 1e-6f, "hard clip above threshold t=%g max=%g", t, maxOut);
            else           CHECK (maxOut <= 1.0f + 1e-6f, "soft clip above 0 dBFS t=%g max=%g", t, maxOut);
            CHECK (maxJump < 0.00011f, "discontinuity t=%g mode=%d jump=%g", t, mode, maxJump);
            std::printf ("thresh %6.1f dB  %s  peak out %6.2f dBFS  (input up to +12 dBFS)\n",
                         tdb, mode ? "HARD" : "SOFT", 20.0 * std::log10 (maxOut));
        }
        // the click-free crossfade between modes stays between the two curves
        for (float x = 0.0f; x < 4.0f; x += 0.01f)
        {
            const float s = clip (x, t, 0.0f), hc = clip (x, t, 1.0f), m = clip (x, t, 0.5f);
            CHECK (m <= std::max (s, hc) + 1e-6f && m >= std::min (s, hc) - 1e-6f, "crossfade outside curves t=%g x=%g", t, x);
        }
    }

    // garbage in, silence out
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    for (float h : { 0.0f, 1.0f })
    {
        CHECK (clip (nan, 0.5f, h) == 0.0f, "NaN not silenced");
        CHECK (clip (inf, 0.5f, h) == 0.0f, "inf not silenced");
        CHECK (std::isfinite (clip (1.0e30f, 0.5f, h)), "huge input");
        CHECK (std::isfinite (clip (0.7f, 1.0f, h)), "threshold at 0 dB");
        CHECK (std::isfinite (clip (0.7f, 0.0f, h)), "threshold at zero");
    }

    std::printf ("%s (%d failures)\n", fails ? "DSP TESTS FAILED" : "ALL DSP TESTS PASSED", fails);
    return fails ? 1 : 0;
}
