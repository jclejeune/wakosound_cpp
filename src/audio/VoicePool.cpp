#include "VoicePool.h"
#include <cstring>
#include <limits>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// play() — thread principal
// ──────────────────────────────────────────────────────────────────
int VoicePool::play(const AudioBuffer* buffer, float volume, bool gate) {
    if (!buffer || buffer->empty()) return -1;

    int slot = findFreeVoice();
    if (slot < 0) slot = stealOldestVoice();

    Voice& v = voices_[slot];
    v.buffer   = buffer;
    v.volume   = volume;
    v.gate     = gate;
    v.id       = nextId_.fetch_add(1, std::memory_order_relaxed);
    v.stopping.store(false, std::memory_order_release);
    // position en dernier : le callback détecte l'activation via position ≥ 0
    v.position.store(0, std::memory_order_release);

    return v.id;
}

// ──────────────────────────────────────────────────────────────────
// stop() — thread principal
// ──────────────────────────────────────────────────────────────────
void VoicePool::stop(int voiceId) {
    for (auto& v : voices_) {
        if (v.id == voiceId && v.position.load(std::memory_order_relaxed) >= 0)
            v.stopping.store(true, std::memory_order_release);
    }
}

void VoicePool::stopAll() {
    for (auto& v : voices_)
        v.stopping.store(true, std::memory_order_release);
}

// ──────────────────────────────────────────────────────────────────
// mix() — callback PortAudio (RT thread, zéro allocation)
// ──────────────────────────────────────────────────────────────────
void VoicePool::mix(float* out, unsigned long frames) {
    std::memset(out, 0, frames * 2 * sizeof(float));

    for (auto& v : voices_) {
        int64_t pos = v.position.load(std::memory_order_acquire);
        if (pos < 0) continue;

        const AudioBuffer* buf = v.buffer;
        if (!buf) continue;

        // Fade-out rapide si stopping (16 frames)
        bool stopping = v.stopping.load(std::memory_order_relaxed);
        float env = 1.0f;
        const float fadeStep = stopping ? (1.0f / 16.0f) : 0.0f;

        for (unsigned long f = 0; f < frames; ++f) {
            if (pos >= buf->frameCount) {
                pos = -1;
                break;
            }

            if (stopping) {
                env -= fadeStep;
                if (env <= 0.0f) { pos = -1; break; }
            }

            float gain = v.volume * env;
            out[f * 2]     += buf->samples[pos * 2]     * gain;
            out[f * 2 + 1] += buf->samples[pos * 2 + 1] * gain;
            ++pos;
        }

        v.position.store(pos, std::memory_order_release);
    }
}

// ──────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────
int VoicePool::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i)
        if (voices_[i].position.load(std::memory_order_relaxed) < 0)
            return i;
    return -1;
}

int VoicePool::stealOldestVoice() {
    // Vole la voix dont la position est la plus avancée (la plus ancienne)
    int    oldest   = 0;
    int64_t maxPos  = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        int64_t p = voices_[i].position.load(std::memory_order_relaxed);
        if (p > maxPos) { maxPos = p; oldest = i; }
    }
    return oldest;
}

} // namespace wako::audio
