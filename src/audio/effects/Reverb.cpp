#include "Reverb.h"
#include <algorithm>
#include <cmath>

namespace wako::audio {

Reverb::Reverb() {
    for (int i = 0; i < NUM_COMBS; ++i) {
        combL_[i].init(COMB_SIZES[i]);
        combR_[i].init(COMB_SIZES[i] + STEREO_SPREAD);
    }
    for (int i = 0; i < NUM_ALLPASSES; ++i) {
        apL_[i].init(ALLPASS_SIZES[i]);
        apR_[i].init(ALLPASS_SIZES[i] + STEREO_SPREAD);
    }
}

void Reverb::setSampleRate(int sr) {
    sampleRate_ = sr;
    float ratio = static_cast<float>(sr) / 44100.f;
    for (int i = 0; i < NUM_COMBS; ++i) {
        int szL = static_cast<int>(COMB_SIZES[i] * ratio);
        int szR = szL + STEREO_SPREAD;
        combL_[i].init(szL);
        combR_[i].init(szR);
    }
    for (int i = 0; i < NUM_ALLPASSES; ++i) {
        int szL = static_cast<int>(ALLPASS_SIZES[i] * ratio);
        int szR = szL + STEREO_SPREAD;
        apL_[i].init(szL);
        apR_[i].init(szR);
    }
}

void Reverb::setRoomSize(float r) {
    roomSize_.store(std::clamp(r, 0.f, 1.f), std::memory_order_relaxed);
}
void Reverb::setDamping(float d) {
    damping_.store(std::clamp(d, 0.f, 1.f), std::memory_order_relaxed);
}
void Reverb::setWet(float w) {
    wet_.store(std::clamp(w, 0.f, 1.f), std::memory_order_relaxed);
}

void Reverb::reset() {
    for (auto& c : combL_) c.filterstore = 0.f, std::fill(c.buf.begin(), c.buf.end(), 0.f);
    for (auto& c : combR_) c.filterstore = 0.f, std::fill(c.buf.begin(), c.buf.end(), 0.f);
    for (auto& a : apL_)   std::fill(a.buf.begin(), a.buf.end(), 0.f);
    for (auto& a : apR_)   std::fill(a.buf.begin(), a.buf.end(), 0.f);
}

void Reverb::process(float* stereo, int frames) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    float room = 0.7f + roomSize_.load(std::memory_order_relaxed) * 0.28f; // 0.7–0.98
    float damp = damping_.load(std::memory_order_relaxed) * 0.4f;
    float wet  = wet_.load(std::memory_order_relaxed);
    float dry  = 1.f - wet;

    for (int f = 0; f < frames; ++f) {
        float inL = stereo[f * 2];
        float inR = stereo[f * 2 + 1];
        float mono = (inL + inR) * 0.015f;  // atténuation entrée

        float outL = 0.f, outR = 0.f;

        // Combs parallèles
        for (int i = 0; i < NUM_COMBS; ++i) {
            outL += combL_[i].process(mono, room, damp);
            outR += combR_[i].process(mono, room, damp);
        }

        // Allpass en série
        for (int i = 0; i < NUM_ALLPASSES; ++i) {
            outL = apL_[i].process(outL);
            outR = apR_[i].process(outR);
        }

        stereo[f * 2]     = dry * inL + wet * outL;
        stereo[f * 2 + 1] = dry * inR + wet * outR;
    }
}

} // namespace wako::audio