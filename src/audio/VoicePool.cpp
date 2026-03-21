#include "VoicePool.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace wako::audio {

int VoicePool::play(const AudioBuffer* buffer, float volume,
                    bool gate, int padIdx) {
    if (!buffer || buffer->empty()) return -1;

    int slot = findFreeVoice();
    if (slot < 0) slot = stealOldestVoice();

    Voice& v   = voices_[slot];
    v.buffer   = buffer;
    v.volume   = volume;
    v.gate     = gate;
    v.padIdx   = padIdx;
    v.id       = nextId_.fetch_add(1, std::memory_order_relaxed);
    v.stopping.store(false, std::memory_order_release);
    v.position.store(0,     std::memory_order_release);

    return v.id;
}

void VoicePool::stop(int voiceId) {
    for (auto& v : voices_)
        if (v.id == voiceId && v.position.load(std::memory_order_relaxed) >= 0)
            v.stopping.store(true, std::memory_order_release);
}

void VoicePool::stopAll() {
    for (auto& v : voices_)
        v.stopping.store(true, std::memory_order_release);
}

void VoicePool::mix(float* out, unsigned long frames, float masterVolume) {
    std::memset(out, 0, frames * 2 * sizeof(float));

    // Accumulateurs de peak temporaires pour ce callback
    float tmpTrack[MAX_PADS_METER] = {};
    float tmpL = 0.f, tmpR = 0.f;

    for (auto& v : voices_) {
        int64_t pos = v.position.load(std::memory_order_acquire);
        if (pos < 0) continue;

        const AudioBuffer* buf = v.buffer;
        if (!buf) continue;

        bool  stopping = v.stopping.load(std::memory_order_relaxed);
        float env      = 1.0f;
        const float fadeStep = stopping ? (1.0f / 16.0f) : 0.0f;
        int   pad      = v.padIdx;

        for (unsigned long f = 0; f < frames; ++f) {
            if (pos >= buf->frameCount) { pos = -1; break; }
            if (stopping) { env -= fadeStep; if (env <= 0.f) { pos = -1; break; } }

            float gain = v.volume * env;
            float l    = buf->samples[pos * 2]     * gain;
            float r    = buf->samples[pos * 2 + 1] * gain;

            out[f * 2]     += l;
            out[f * 2 + 1] += r;

            // Accumulation peak par track
            if (pad >= 0 && pad < MAX_PADS_METER) {
                float pk = std::max(std::abs(l), std::abs(r));
                if (pk > tmpTrack[pad]) tmpTrack[pad] = pk;
            }
            ++pos;
        }
        v.position.store(pos, std::memory_order_release);
    }

    // Appliquer master volume + calculer peak master
    for (unsigned long f = 0; f < frames; ++f) {
        out[f * 2]     *= masterVolume;
        out[f * 2 + 1] *= masterVolume;
        if (std::abs(out[f*2])   > tmpL) tmpL = std::abs(out[f*2]);
        if (std::abs(out[f*2+1]) > tmpR) tmpR = std::abs(out[f*2+1]);
    }

    // Mettre à jour les atomics (peak hold : on garde le max)
    for (int p = 0; p < MAX_PADS_METER; ++p) {
        float cur = trackPeaks_[p].load(std::memory_order_relaxed);
        if (tmpTrack[p] > cur)
            trackPeaks_[p].store(tmpTrack[p], std::memory_order_relaxed);
    }
    {
        float cl = peakL_.load(std::memory_order_relaxed);
        float cr = peakR_.load(std::memory_order_relaxed);
        if (tmpL > cl) peakL_.store(tmpL, std::memory_order_relaxed);
        if (tmpR > cr) peakR_.store(tmpR, std::memory_order_relaxed);
    }
}

void VoicePool::decayPeaks(float factor) {
    for (auto& p : trackPeaks_) {
        float v = p.load(std::memory_order_relaxed) * factor;
        p.store(v, std::memory_order_relaxed);
    }
    peakL_.store(peakL_.load(std::memory_order_relaxed) * factor,
                 std::memory_order_relaxed);
    peakR_.store(peakR_.load(std::memory_order_relaxed) * factor,
                 std::memory_order_relaxed);
}

int VoicePool::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i)
        if (voices_[i].position.load(std::memory_order_relaxed) < 0)
            return i;
    return -1;
}

int VoicePool::stealOldestVoice() {
    int    oldest = 0;
    int64_t maxPos = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        int64_t p = voices_[i].position.load(std::memory_order_relaxed);
        if (p > maxPos) { maxPos = p; oldest = i; }
    }
    return oldest;
}

} // namespace wako::audio