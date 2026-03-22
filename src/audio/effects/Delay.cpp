#include "Delay.h"
#include <algorithm>
#include <cmath>

namespace wako::audio {

Delay::Delay() {
    setSampleRate(44100);
}

void Delay::setSampleRate(int sr) {
    sampleRate_ = sr;
    maxDelay_   = static_cast<int>(MAX_DELAY_S * sr) + 1;
    bufL_.assign(maxDelay_, 0.f);
    bufR_.assign(maxDelay_, 0.f);
    writePos_ = 0;
}

void Delay::setTimeMs(float ms) {
    timeMs_.store(std::clamp(ms, 0.f, MAX_DELAY_S * 1000.f),
                  std::memory_order_relaxed);
}
void Delay::setFeedback(float f) {
    feedback_.store(std::clamp(f, 0.f, 0.95f), std::memory_order_relaxed);
}
void Delay::setMix(float m) {
    mix_.store(std::clamp(m, 0.f, 1.f), std::memory_order_relaxed);
}

void Delay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.f);
    std::fill(bufR_.begin(), bufR_.end(), 0.f);
    writePos_ = 0;
}

void Delay::process(float* stereo, int frames) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    float ms       = timeMs_.load(std::memory_order_relaxed);
    float feedback = feedback_.load(std::memory_order_relaxed);
    float wet      = mix_.load(std::memory_order_relaxed);
    float dry      = 1.f - wet;

    int delaySamples = static_cast<int>(ms * 0.001f * sampleRate_);
    delaySamples = std::clamp(delaySamples, 1, maxDelay_ - 1);

    for (int f = 0; f < frames; ++f) {
        int readPos = writePos_ - delaySamples;
        if (readPos < 0) readPos += maxDelay_;

        float dL = bufL_[readPos];
        float dR = bufR_[readPos];

        float inL = stereo[f * 2];
        float inR = stereo[f * 2 + 1];

        bufL_[writePos_] = inL + dL * feedback;
        bufR_[writePos_] = inR + dR * feedback;

        stereo[f * 2]     = dry * inL + wet * dL;
        stereo[f * 2 + 1] = dry * inR + wet * dR;

        if (++writePos_ >= maxDelay_) writePos_ = 0;
    }
}

} // namespace wako::audio