#include "EQ5.h"
#include <algorithm>

namespace wako::audio {

EQ5::EQ5() {
    for (auto& g : gains_) g.store(0.f, std::memory_order_relaxed);
}

void EQ5::setSampleRate(int sr) {
    sampleRate_ = sr;
    dirty_.store(true, std::memory_order_relaxed);
    reset();
}

void EQ5::setBandGain(int band, float gainDb) {
    if (band < 0 || band >= BANDS) return;
    gainDb = std::clamp(gainDb, -12.f, 12.f);
    gains_[band].store(gainDb, std::memory_order_relaxed);
    dirty_.store(true, std::memory_order_relaxed);
}

float EQ5::getBandGain(int band) const {
    if (band < 0 || band >= BANDS) return 0.f;
    return gains_[band].load(std::memory_order_relaxed);
}

void EQ5::reset() {
    for (auto& f : filL_) f.reset();
    for (auto& f : filR_) f.reset();
}

void EQ5::updateCoeffs() noexcept {
    double sr = sampleRate_;
    for (int b = 0; b < BANDS; ++b) {
        float g = gains_[b].load(std::memory_order_relaxed);
        if (b == 0)
            filL_[b].setLowShelf(sr, FREQS[b], g),
            filR_[b].setLowShelf(sr, FREQS[b], g);
        else if (b == BANDS - 1)
            filL_[b].setHighShelf(sr, FREQS[b], g),
            filR_[b].setHighShelf(sr, FREQS[b], g);
        else
            filL_[b].setPeaking(sr, FREQS[b], g, QS[b]),
            filR_[b].setPeaking(sr, FREQS[b], g, QS[b]);
    }
    dirty_.store(false, std::memory_order_relaxed);
}

void EQ5::process(float* stereo, int frames) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;
    if (dirty_.load(std::memory_order_relaxed))    updateCoeffs();

    for (int f = 0; f < frames; ++f) {
        float l = stereo[f * 2];
        float r = stereo[f * 2 + 1];
        for (int b = 0; b < BANDS; ++b) {
            l = filL_[b].process(l);
            r = filR_[b].process(r);
        }
        stereo[f * 2]     = l;
        stereo[f * 2 + 1] = r;
    }
}

} // namespace wako::audio