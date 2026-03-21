#pragma once
#include "AudioCache.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace wako::audio {

static constexpr int MAX_PADS_METER = 9;

struct Voice {
    const AudioBuffer*   buffer   = nullptr;
    std::atomic<int64_t> position {-1};
    float                volume   = 1.0f;
    bool                 gate     = false;
    std::atomic<bool>    stopping {false};
    int                  id       = -1;
    int                  padIdx   = -1;   // pour le metering par track
};

class VoicePool {
public:
    static constexpr int MAX_VOICES = 16;

    // padIdx : index du pad (-1 = inconnu / preview)
    int  play(const AudioBuffer* buffer, float volume = 1.0f,
              bool gate = false, int padIdx = -1);
    void stop(int voiceId);
    void stopAll();

    // masterVolume appliqué dans mix() avant écriture sortie
    void mix(float* outBuffer, unsigned long frameCount,
             float masterVolume = 1.0f);

    // ── Metering ──────────────────────────────────────────────────
    // Peaks mis à jour dans mix(), décroissance naturelle par lecture.
    // Appelé depuis le thread UI (QTimer 30 fps) — atomics garantissent la safety.
    float trackPeak(int pad) const {
        if (pad < 0 || pad >= MAX_PADS_METER) return 0.f;
        return trackPeaks_[pad].load(std::memory_order_relaxed);
    }
    float masterPeakL() const { return peakL_.load(std::memory_order_relaxed); }
    float masterPeakR() const { return peakR_.load(std::memory_order_relaxed); }

    // Decay manuel appelé par le timer UI (évite accumulation infinie)
    void decayPeaks(float factor = 0.85f);

private:
    std::array<Voice, MAX_VOICES> voices_;
    std::atomic<int>              nextId_{0};

    // Peaks — écrits dans mix() (RT), lus dans UI thread
    std::array<std::atomic<float>, MAX_PADS_METER> trackPeaks_{};
    std::atomic<float> peakL_{0}, peakR_{0};

    int findFreeVoice();
    int stealOldestVoice();
};

} // namespace wako::audio