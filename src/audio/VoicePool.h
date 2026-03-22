#pragma once
#include "AudioCache.h"
#include "EffectChain.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace wako::audio {

static constexpr int MAX_PADS_METER = 9;
static constexpr int MAX_FRAMES_PA  = 4096; // buffer max PortAudio

struct Voice {
    const AudioBuffer*   buffer   = nullptr;
    std::atomic<int64_t> position {-1};
    float                volume   = 1.0f;
    bool                 gate     = false;
    std::atomic<bool>    stopping {false};
    int                  id       = -1;
    int                  padIdx   = -1;
};

class VoicePool {
public:
    static constexpr int MAX_VOICES = 16;

    int  play(const AudioBuffer* buffer, float volume = 1.0f,
              bool gate = false, int padIdx = -1);
    void stop(int voiceId);
    void stopAll();

    // ── Effect chains (pointeurs, VoicePool ne possède pas) ───────
    // Player appelle setChain() avant de démarrer le stream.
    void setTrackChain(int pad, EffectChain* chain);
    void setMasterChain(EffectChain* chain);

    // ── Mix principal (RT callback) ───────────────────────────────
    // Mix par pad → applique effect chain par pad →
    // somme → applique master chain → applique masterVolume → output.
    void mix(float* out, unsigned long frames, float masterVolume = 1.0f) noexcept;

    // ── Metering ──────────────────────────────────────────────────
    float trackPeak(int pad) const {
        if (pad < 0 || pad >= MAX_PADS_METER) return 0.f;
        return trackPeaks_[pad].load(std::memory_order_relaxed);
    }
    float masterPeakL() const { return peakL_.load(std::memory_order_relaxed); }
    float masterPeakR() const { return peakR_.load(std::memory_order_relaxed); }
    void  decayPeaks(float factor = 0.85f);

private:
    std::array<Voice, MAX_VOICES> voices_;
    std::atomic<int>              nextId_{0};

    // Per-channel intermediate buffers (pre-allocated, zéro alloc RT)
    // [pad][frame*2] stereo interleaved
    std::array<std::array<float, MAX_FRAMES_PA * 2>, MAX_PADS_METER> chanBufs_{};
    std::array<float, MAX_FRAMES_PA * 2>                              masterBuf_{};

    // Effect chains (non-owning)
    std::array<EffectChain*, MAX_PADS_METER> trackChains_{};
    EffectChain*                              masterChain_ = nullptr;

    // Peaks
    std::array<std::atomic<float>, MAX_PADS_METER> trackPeaks_{};
    std::atomic<float> peakL_{0}, peakR_{0};

    int findFreeVoice();
    int stealOldestVoice();
};

} // namespace wako::audio