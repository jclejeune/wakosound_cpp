#pragma once
#include "VoicePool.h"
#include "EffectChain.h"
#include <portaudio.h>
#include <string>
#include <atomic>
#include <algorithm>
#include <array>
#include <vector>

namespace wako::audio {

class Player {
public:
    static Player& instance();
    ~Player();

    bool init(int sampleRate = 44100, int framesPerBuffer = 256);
    void shutdown();

    int  play(const std::string& filePath,
              float volume = 1.0f, int pitch = 0,
              bool gate = false, int padIdx = -1);

    void stop(int voiceId);
    void stopAll();

    bool isRunning() const { return stream_ != nullptr; }

    // ── Master volume ─────────────────────────────
    void setMasterVolume(float v) {
        masterVolume_.store(std::clamp(v, 0.f, 1.f), std::memory_order_relaxed);
    }

    float masterVolume() const {
        return masterVolume_.load(std::memory_order_relaxed);
    }

    // ── Effect chains ─────────────────────────────
    static constexpr int MASTER_CHAIN = 9;

    EffectChain& chain(int idx)      { return chains_[std::clamp(idx, 0, MASTER_CHAIN)]; }
    EffectChain& trackChain(int pad) { return chains_[std::clamp(pad, 0, 8)]; }
    EffectChain& masterChain()       { return chains_[MASTER_CHAIN]; }

    // ── Recording ─────────────────────────────────
    void startRecording(int totalFrames);
    bool stopRecording(const std::string& outputPath);
    bool isRecording() const { return recording_.load(std::memory_order_relaxed); }

    // ── Metering ──────────────────────────────────
    float trackPeak(int pad)  const { return voicePool_.trackPeak(pad); }
    float masterPeakL()       const { return voicePool_.masterPeakL(); }
    float masterPeakR()       const { return voicePool_.masterPeakR(); }
    void  decayPeaks(float f = 0.85f) { voicePool_.decayPeaks(f); }

private:
    Player() = default;
    Player(const Player&) = delete;

    static int paCallback(const void*, void* out, unsigned long frames,
                          const PaStreamCallbackTimeInfo*,
                          PaStreamCallbackFlags, void* userData);

    PaStream*  stream_      = nullptr;
    VoicePool  voicePool_;
    int        sampleRate_  = 44100;

    std::atomic<float> masterVolume_{1.0f};

    std::array<EffectChain, MASTER_CHAIN + 1> chains_;

    // ── Recording ─────────────────────────────────
    std::atomic<bool>   recording_{false};
    std::vector<float>  recordBuf_;
    std::atomic<int>    recordPos_{0};
    int                 recordTotal_{0};
};

} // namespace wako::audio