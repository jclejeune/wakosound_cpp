#include "Saturator.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace wako::audio {

void Saturator::setDrive(float d) {
    drive_.store(std::clamp(d, 0.f, 1.f), std::memory_order_relaxed);
}
void Saturator::setMix(float m) {
    mix_.store(std::clamp(m, 0.f, 1.f), std::memory_order_relaxed);
}

// ──────────────────────────────────────────────────────────────────
// Algorithmes de saturation (pas de makeup ici — appliqué après)
// ──────────────────────────────────────────────────────────────────

static inline float tubeSat(float x, float pg) noexcept {
    float driven = x * pg;
    return driven >= 0.f
        ? (2.f / 3.14159265f) * std::atan(driven * 1.5f)
        : std::tanh(driven * 1.2f);
}

static inline float transistorSat(float x, float pg) noexcept {
    float driven = x * pg;
    constexpr float THRESH_POS =  0.8f;
    constexpr float THRESH_NEG = -0.3f;
    if (driven > THRESH_POS) {
        float over = driven - THRESH_POS;
        return THRESH_POS + over / (1.f + over * over * 3.f);
    }
    if (driven < THRESH_NEG) {
        float over = driven - THRESH_NEG;
        return THRESH_NEG + over / (1.f + std::abs(over) * 6.f);
    }
    return driven;
}

static inline float fuzzSat(float x, float pg) noexcept {
    float driven   = x * pg;
    constexpr float RECT = 0.4f;
    float rectified = driven * (1.f - RECT) + std::abs(driven) * RECT;
    return std::clamp(rectified, -1.f, 1.f);
}

// ──────────────────────────────────────────────────────────────────
// process — compensation RMS, C++20 strict (pas de VLA)
// ──────────────────────────────────────────────────────────────────
void Saturator::process(float* stereo, int frames) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    float drive = drive_.load(std::memory_order_relaxed);
    float wet   = mix_.load(std::memory_order_relaxed);
    float dry   = 1.f - wet;
    Mode  mode  = static_cast<Mode>(mode_.load(std::memory_order_relaxed));

    float maxGain = (mode == Mode::Tube)       ? 4.f
                  : (mode == Mode::Transistor) ? 20.f
                  :                              40.f;
    float pregain = 1.f + drive * drive * (maxGain - 1.f);

    // ── RMS entrée ────────────────────────────────────────────────
    float sumIn = 0.f;
    for (int i = 0; i < frames * 2; ++i)
        sumIn += stereo[i] * stereo[i];
    float rmsIn = std::sqrt(sumIn / static_cast<float>(frames * 2));

    // ── Saturation dans un buffer alloué sur le heap ──────────────
    // std::vector au lieu de VLA — C++20 strict, portable (Swift, WASM…)
    std::vector<float> saturated(static_cast<std::size_t>(frames * 2));

    for (int f = 0; f < frames; ++f) {
        float inL = stereo[f * 2];
        float inR = stereo[f * 2 + 1];
        switch (mode) {
            case Mode::Tube:
                saturated[f * 2]     = tubeSat(inL, pregain);
                saturated[f * 2 + 1] = tubeSat(inR, pregain);
                break;
            case Mode::Transistor:
                saturated[f * 2]     = transistorSat(inL, pregain);
                saturated[f * 2 + 1] = transistorSat(inR, pregain);
                break;
            case Mode::Fuzz:
                saturated[f * 2]     = fuzzSat(inL, pregain);
                saturated[f * 2 + 1] = fuzzSat(inR, pregain);
                break;
        }
    }

    // ── RMS sortie ────────────────────────────────────────────────
    float sumOut = 0.f;
    for (int i = 0; i < frames * 2; ++i)
        sumOut += saturated[i] * saturated[i];
    float rmsOut = std::sqrt(sumOut / static_cast<float>(frames * 2));

    // ── Makeup gain RMS ───────────────────────────────────────────
    // Ne compense pas si le signal est trop faible (silence/bruit de fond)
    float makeup = 1.f;
    if (rmsOut > 0.001f && rmsIn > 0.001f) {
        makeup = std::clamp(rmsIn / rmsOut, 0.5f, 3.0f);
    }

    // ── Mix wet/dry + makeup ──────────────────────────────────────
    for (int f = 0; f < frames; ++f) {
        stereo[f * 2]     = dry * stereo[f * 2]     + wet * saturated[f * 2]     * makeup;
        stereo[f * 2 + 1] = dry * stereo[f * 2 + 1] + wet * saturated[f * 2 + 1] * makeup;
    }
}

} // namespace wako::audio