#pragma once
#include "AudioCache.h"
#include "EffectChain.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace wako::audio {

static constexpr int MAX_PADS_METER = 9;
static constexpr int MAX_FRAMES_PA  = 4096;

struct Voice {
    const AudioBuffer* buffer = nullptr;

    float position = 0.0f;
    float volume   = 1.0f;
    float pitch    = 1.0f;

    float env      = 0.0f;
    float envStep  = 0.0f;

    bool  active   = false;
    bool  stopping = false;

    int   id       = -1;
    int   padIdx   = -1;
};

class VoicePool {
public:
    static constexpr int MAX_VOICES = 256;

    void setSampleRate(int sr) { sampleRate_ = sr; }

    int  play(const AudioBuffer* buffer, float volume = 1.0f,
              bool gate = false, int padIdx = -1, int pitch = 0);

    void stop(int voiceId);
    void stopAll();

    void setTrackChain(int pad, EffectChain* chain);
    void setMasterChain(EffectChain* chain);

    void mix(float* out, unsigned long frames, float masterVolume = 1.0f) noexcept;

    // ── Metering ─────────────────────────────
    float trackPeak(int pad) const {
        if (pad < 0 || pad >= MAX_PADS_METER) return 0.f;
        return trackPeaks_[pad].load(std::memory_order_relaxed);
    }

    float masterPeakL() const { return peakL_.load(std::memory_order_relaxed); }
    float masterPeakR() const { return peakR_.load(std::memory_order_relaxed); }

    void decayPeaks(float factor = 0.85f);

private:
    std::array<Voice, MAX_VOICES> voices_{};

    int nextId_     = 0;
    int sampleRate_ = 44100;

    std::array<EffectChain*, MAX_PADS_METER> trackChains_{};
    EffectChain* masterChain_ = nullptr;

    std::array<std::atomic<float>, MAX_PADS_METER> trackPeaks_{};
    std::atomic<float> peakL_{0};
    std::atomic<float> peakR_{0};

    int findFreeVoice();
    int stealVoice();
};

} // namespace wako::audio