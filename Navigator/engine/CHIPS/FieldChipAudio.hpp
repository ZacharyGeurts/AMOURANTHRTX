#pragma once

// Shared audio output style — Classic (raw/harsh) vs Modern (bandlimited + soft clip).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldChips {

enum class AudioStyle : std::uint8_t {
    Classic = 0, // raw staircase waves, nonlinear mix
    Modern = 1,  // bandlimited + gentle low-pass, less chippy
};

constexpr AudioStyle Authentic = AudioStyle::Classic; // backward compat alias

namespace AudioDetail {

inline float polyBlep(float t, float dt) noexcept {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    }
    if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

inline float squareClassic(bool high, float amp) noexcept {
    return high ? amp : 0.f;
}

inline float squareModern(bool high, float amp, float& phase, float dt) noexcept {
    float y = high ? amp : 0.f;
    y += polyBlep(phase, dt);
    phase += 1.f;
    if (phase >= 1.f) phase -= 1.f;
    return y;
}

inline float applyModernFilter(float out, float& lpState, float kLp = 0.12f,
                               float tanhGain = 1.4f) noexcept {
    lpState += kLp * (out - lpState);
    return std::tanh(lpState * tanhGain);
}

inline float finalizeSample(float out, AudioStyle style, float& lpState,
                            float kLp = 0.12f, float tanhGain = 1.4f) noexcept {
    if (style == AudioStyle::Modern)
        out = applyModernFilter(out, lpState, kLp, tanhGain);
    return std::clamp(out, -1.f, 1.f);
}

} // namespace AudioDetail

} // namespace FieldChips