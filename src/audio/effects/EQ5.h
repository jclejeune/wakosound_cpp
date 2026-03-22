#pragma once
#include "Biquad.h"
#include <atomic>
#include <array>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// EQ5 — égaliseur 5 bandes paramétrique
//
// Bande 0 : Low Shelf   80 Hz
// Bande 1 : Peaking    250 Hz
// Bande 2 : Peaking   1000 Hz
// Bande 3 : Peaking   4000 Hz
// Bande 4 : High Shelf 12000 Hz
//
// Thread-safety : paramètres atomiques, recalcul dans process() si dirty.
// ──────────────────────────────────────────────────────────────────
class EQ5 {
public:
    static constexpr int BANDS = 5;
    static constexpr float FREQS[BANDS] = {80.f, 250.f, 1000.f, 4000.f, 12000.f};
    static constexpr float QS[BANDS]    = {0.7f, 1.0f,  1.0f,   1.0f,   0.7f};

    EQ5();

    void setSampleRate(int sr);

    // ── Paramètres (UI thread) ────────────────────────────────────
    void  setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool  enabled()    const { return enabled_.load(std::memory_order_relaxed); }

    void  setBandGain(int band, float gainDb); // -12.0 → +12.0
    float getBandGain(int band) const;

    // ── Traitement (RT thread) ────────────────────────────────────
    // Traite un buffer stereo interleaved en place.
    void process(float* stereo, int frames) noexcept;

    void reset();

private:
    void updateCoeffs() noexcept;   // appelé dans RT si dirty

    std::atomic<bool>  enabled_{false};
    std::atomic<bool>  dirty_{true};
    int                sampleRate_ = 44100;

    std::array<std::atomic<float>, BANDS> gains_{};

    // Un filtre L + R par bande
    std::array<Biquad, BANDS> filL_{};
    std::array<Biquad, BANDS> filR_{};
};

} // namespace wako::audio