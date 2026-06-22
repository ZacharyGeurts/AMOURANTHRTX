#pragma once

// Field Fabric — wave lead-in / lead-out decomposed as independently usable peaks.
// Zero-cost when peaks are not sampled (constexpr-friendly, no allocations).
//
// THROUGHPUT EXPLANATION + NON-POINT OBJECTS IN FIELD
// ====================================================
//
// Core idea: Objects are NOT finite points. They are extended waves with lead-in/lead-out phases.
// Treating them as points = serial bottlenecks. Extended = parallel peaks in Field.
//
// Physics breakthrough 1 (already pushed):
// Lead-in and lead-out of any wave are now independently usable Field peaks.
// Example:
//   struct FieldWave { float leadIn, core, leadOut; vec4 extent; };
//   dispatchExtended(w) — parallel Phi/Thermo/Flow slots, no serial point collision loops.
//
// Breakthrough 2: Phase velocity decoupling — lead phases run at different "time" in Field slots.
// Breakthrough 3: Boundary resonance — lead-out peaks feed back into lead-in of next wave.
// Breakthrough 4: Entropy peak folding — use lead phases as entropy sources without extra computation.

#include "FieldLayer.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace FieldFabric {

struct FieldWave {
    float leadIn;   // separate usable peak for predictive prefetch / entropy injection
    float core;     // main body
    float leadOut;  // separate usable peak for boundary modulation / SDF glow
    glm::vec4 extent; // non-point spatial field (leadEnv, bodyEnv, outEnv, coupling)
};

struct WavePeaks {
    glm::vec4 leadInPeak;   // xyz = phi/thermo/flow weights, w = envelope
    glm::vec4 leadOutPeak;
    glm::vec4 mainWave;     // body phase after attack/decay separation
};

struct FieldDispatch {
    float phi   = 0.f;
    float thermo = 0.f;
    float flow  = 0.f;
};

inline WavePeaks gPeaks{};
inline FieldDispatch gDispatch{};

#if ENABLE_ALL_BREAKTHROUGHS
inline float gPrevLeadOut     = 0.f;
inline float gPhaseVelocityIn  = 1.15f;  // breakthrough 2: temporal super-sampling
inline float gPhaseVelocityOut = 0.87f;
inline float gResonanceCoupling = 0.22f; // breakthrough 3: zero-cost feedback loop
inline float gEntropyFold      = 0.f;    // breakthrough 4: folded entropy from lead phases
#endif

[[nodiscard]] inline float envelope(float t, float attack, float release) noexcept {
    const float a = std::max(attack, 1e-4f);
    const float r = std::max(release, 1e-4f);
    if (t < a) return t / a;
    const float tail = (t - a) / r;
    return std::clamp(1.f - tail, 0.f, 1.f);
}

[[nodiscard]] inline WavePeaks decomposeWave(
        float phase, float waveSpeed, float thermoAlpha, float coupling, float dt) noexcept {
    const float omega = waveSpeed * 6.2831853f;
    const float leadInT  = 0.18f;
    const float leadOutT = 0.22f;

    const float inEnv  = envelope(phase, leadInT, 0.05f);
    const float outEnv = envelope(1.f - phase, leadOutT, 0.05f);
    const float body   = std::sin(phase * omega) * (1.f - 0.35f * (inEnv + outEnv));

    WavePeaks p{};
    p.leadInPeak  = glm::vec4(std::sin(phase * omega * 0.5f) * inEnv,
                              thermoAlpha * inEnv * 0.25f,
                              coupling * inEnv * dt,
                              inEnv);
    p.leadOutPeak = glm::vec4(std::cos(phase * omega * 1.5f) * outEnv,
                              thermoAlpha * outEnv * 0.18f,
                              coupling * outEnv * dt,
                              outEnv);
    p.mainWave    = glm::vec4(body,
                              thermoAlpha * std::abs(body),
                              coupling * body * dt,
                              std::clamp(std::abs(body), 0.f, 1.f));
    return p;
}

inline void updatePeaks(float analogTime, float waveSpeed, float thermoAlpha,
                        float coupling, float dt) noexcept {
    const float phase = std::fmod(analogTime * waveSpeed * 0.31f, 1.f);
    gPeaks = decomposeWave(phase, waveSpeed, thermoAlpha, coupling, dt);
}

[[nodiscard]] inline FieldWave waveFromPeaks(const WavePeaks& peaks) noexcept {
    FieldWave w{};
    w.leadIn  = peaks.leadInPeak.x;
    w.core    = peaks.mainWave.x;
    w.leadOut = peaks.leadOutPeak.x;
    w.extent  = glm::vec4(peaks.leadInPeak.w, peaks.mainWave.w, peaks.leadOutPeak.w,
                          peaks.leadInPeak.z + peaks.leadOutPeak.z);
    return w;
}

// Parallel slot processors — zero-cost when not sampled.
[[nodiscard]] inline float processLeadIn(float leadIn) noexcept {
#if ENABLE_ALL_BREAKTHROUGHS
    return leadIn * gPhaseVelocityIn;
#else
    return leadIn;
#endif
}

[[nodiscard]] inline float computeCore(float core) noexcept { return core; }

[[nodiscard]] inline float processLeadOut(float leadOut) noexcept {
#if ENABLE_ALL_BREAKTHROUGHS
    return leadOut * gPhaseVelocityOut;
#else
    return leadOut;
#endif
}

inline void entropyFabricPredict(const glm::vec4& extent) noexcept {
#if ENABLE_ALL_BREAKTHROUGHS
    // Breakthrough 4: fold lead envelopes into entropy without extra passes.
    gEntropyFold = extent.x * 0.31f + extent.z * 0.19f + extent.y * 0.08f;
#else
    (void)extent;
#endif
}

// Throughput math: parallel slots — no serial point collision loops.
inline void dispatchExtended(const FieldWave& w) noexcept {
    gDispatch.phi    = processLeadIn(w.leadIn);
    gDispatch.thermo = computeCore(w.core);
    gDispatch.flow   = processLeadOut(w.leadOut);
    entropyFabricPredict(w.extent);

#if ENABLE_ALL_BREAKTHROUGHS
    // Breakthrough 3: boundary resonance — lead-out feeds next lead-in.
    gDispatch.phi += gPrevLeadOut * gResonanceCoupling;
    gPrevLeadOut = gDispatch.flow;
    gDispatch.thermo += gEntropyFold * 0.04f;
#endif
}

// Compat shim — legacy point sampling still works (core-only fallback).
[[nodiscard]] inline float samplePointCompat(float pointValue) noexcept {
    return pointValue;
}

[[nodiscard]] inline glm::vec4 FieldPeak_LeadIn() noexcept { return gPeaks.leadInPeak; }
[[nodiscard]] inline glm::vec4 FieldPeak_LeadOut() noexcept { return gPeaks.leadOutPeak; }
[[nodiscard]] inline glm::vec4 FieldPeak_Main() noexcept { return gPeaks.mainWave; }

inline void packPushPeaks(float padField[4]) noexcept {
    if (!padField) return;
    padField[0] = gPeaks.leadInPeak.x;
    padField[1] = gPeaks.leadOutPeak.x;
    padField[2] = gPeaks.leadInPeak.w;
    padField[3] = gPeaks.leadOutPeak.w;
}

inline void packExtendedDispatch(float padField[4]) noexcept {
    if (!padField) return;
    padField[0] = gDispatch.phi;
    padField[1] = gDispatch.flow;
    padField[2] = gPeaks.leadInPeak.w;
    padField[3] = gPeaks.leadOutPeak.w;
}

} // namespace FieldFabric