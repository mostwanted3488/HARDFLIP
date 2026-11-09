#pragma once
// HARDFLIP clip curves. Pure C++ (no JUCE) so they can be unit-tested on their own.
//
// All values are linear amplitudes. `t` is the threshold (THRESH knob, 0 < t <= 1).
//  - soft: unity below t, then a tanh knee that bends into a ceiling of 1.0 (0 dBFS)
//  - hard: unity below t, flat at t above it (brick wall)
// The same formulas are used by the web UI to draw the curve (UI/index.html, clipF()).
#include <algorithm>
#include <cmath>

namespace hardflip
{
    inline float softClip (float x, float t) noexcept
    {
        const float a = std::abs (x);
        if (! (a > t)) return x;                       // also passes 0 and keeps the sign of tiny values
        const float k = 1.0f - t;
        if (k < 1.0e-4f) return std::copysign (t, x);  // threshold at 0 dB: the knee has no room left
        return std::copysign (t + k * std::tanh ((a - t) / k), x);
    }

    inline float hardClip (float x, float t) noexcept
    {
        return std::clamp (x, -t, t);
    }

    // hardAmount: 0 = soft, 1 = hard (values in between are only used for a click-free mode switch)
    inline float clip (float x, float t, float hardAmount) noexcept
    {
        if (! std::isfinite (x)) return 0.0f;
        t = std::clamp (t, 1.0e-4f, 1.0f);
        if (hardAmount <= 0.0f) return softClip (x, t);
        if (hardAmount >= 1.0f) return hardClip (x, t);
        return softClip (x, t) * (1.0f - hardAmount) + hardClip (x, t) * hardAmount;
    }

    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
}
