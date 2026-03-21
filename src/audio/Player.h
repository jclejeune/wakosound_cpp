#pragma once
#include "VoicePool.h"
#include <portaudio.h>
#include <string>
#include <atomic>
#include <algorithm> 

namespace wako::audio {

class Player {
public:
    static Player& instance();
    ~Player();

    bool init(int sampleRate = 44100, int framesPerBuffer = 256);
    void shutdown();

    // padIdx : index du pad pour le metering (-1 = preview)
    int  play(const std::string& filePath,
              float volume  = 1.0f,
              int   pitch   = 0,
              bool  gate    = false,
              int   padIdx  = -1);

    void stop(int voiceId);
    void stopAll();

    bool isRunning() const { return stream_ != nullptr; }

    // ── Master volume ─────────────────────────────────────────────
    void  setMasterVolume(float v) {
        masterVolume_.store(std::clamp(v, 0.f, 1.f),
                            std::memory_order_relaxed);
    }
    float masterVolume() const {
        return masterVolume_.load(std::memory_order_relaxed);
    }

    // ── Metering (thread-safe, appelé depuis UI) ──────────────────
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

    PaStream*          stream_       = nullptr;
    VoicePool          voicePool_;
    int                sampleRate_   = 44100;
    std::atomic<float> masterVolume_ {1.0f};
};

} // namespace wako::audio