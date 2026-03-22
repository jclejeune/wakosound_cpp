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

void VoicePool::setTrackChain(int pad, EffectChain* chain) {
    if (pad >= 0 && pad < MAX_PADS_METER)
        trackChains_[pad] = chain;
}

void VoicePool::setMasterChain(EffectChain* chain) {
    masterChain_ = chain;
}

void VoicePool::mix(float* out, unsigned long frames, float masterVolume) noexcept {
    // Limiter au buffer max pré-alloué
    unsigned long f = std::min(frames, (unsigned long)MAX_FRAMES_PA);

    // ── 1. Vider les buffers par canal ────────────────────────────
    for (auto& cb : chanBufs_)
        std::memset(cb.data(), 0, f * 2 * sizeof(float));
    std::memset(masterBuf_.data(), 0, f * 2 * sizeof(float));

    // ── 2. Distribuer chaque voix dans son canal ──────────────────
    for (auto& v : voices_) {
        int64_t pos = v.position.load(std::memory_order_acquire);
        if (pos < 0) continue;

        const AudioBuffer* buf = v.buffer;
        if (!buf) continue;

        int  pad      = v.padIdx;
        bool stopping = v.stopping.load(std::memory_order_relaxed);
        float env     = 1.0f;
        float fadeStep= stopping ? (1.0f / 16.0f) : 0.0f;

        float* dst = (pad >= 0 && pad < MAX_PADS_METER)
                     ? chanBufs_[pad].data()
                     : masterBuf_.data();   // preview → master direct

        for (unsigned long i = 0; i < f; ++i) {
            if (pos >= buf->frameCount) { pos = -1; break; }
            if (stopping) { env -= fadeStep; if (env <= 0.f) { pos = -1; break; } }

            float gain = v.volume * env;
            dst[i * 2]     += buf->samples[pos * 2]     * gain;
            dst[i * 2 + 1] += buf->samples[pos * 2 + 1] * gain;
            ++pos;
        }
        v.position.store(pos, std::memory_order_release);
    }

    // ── 3. Effets par canal + mesure peak + somme dans master ─────
    float tmpTrack[MAX_PADS_METER] = {};

    for (int p = 0; p < MAX_PADS_METER; ++p) {
        float* cb = chanBufs_[p].data();

        // Appliquer la chain d'effets du pad
        if (trackChains_[p])
            trackChains_[p]->process(cb, static_cast<int>(f));

        // Peak par track
        for (unsigned long i = 0; i < f; ++i) {
            float pk = std::max(std::abs(cb[i*2]), std::abs(cb[i*2+1]));
            if (pk > tmpTrack[p]) tmpTrack[p] = pk;
        }

        // Sommer dans masterBuf_
        for (unsigned long i = 0; i < f * 2; ++i)
            masterBuf_[i] += cb[i];
    }

    // ── 4. Effets master ──────────────────────────────────────────
    if (masterChain_)
        masterChain_->process(masterBuf_.data(), static_cast<int>(f));

    // ── 5. Master volume + peak master + copie dans output ───────
    float tmpL = 0.f, tmpR = 0.f;
    for (unsigned long i = 0; i < f; ++i) {
        masterBuf_[i*2]   *= masterVolume;
        masterBuf_[i*2+1] *= masterVolume;
        if (std::abs(masterBuf_[i*2])   > tmpL) tmpL = std::abs(masterBuf_[i*2]);
        if (std::abs(masterBuf_[i*2+1]) > tmpR) tmpR = std::abs(masterBuf_[i*2+1]);
    }
    std::memcpy(out, masterBuf_.data(), f * 2 * sizeof(float));

    // Silence si frames > MAX_FRAMES_PA (ne devrait pas arriver)
    if (frames > f)
        std::memset(out + f * 2, 0, (frames - f) * 2 * sizeof(float));

    // ── 6. Mettre à jour les peaks (max hold) ────────────────────
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
    peakL_.store(peakL_.load(std::memory_order_relaxed) * factor, std::memory_order_relaxed);
    peakR_.store(peakR_.load(std::memory_order_relaxed) * factor, std::memory_order_relaxed);
}

int VoicePool::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i)
        if (voices_[i].position.load(std::memory_order_relaxed) < 0)
            return i;
    return -1;
}

int VoicePool::stealOldestVoice() {
    int oldest = 0; int64_t maxPos = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        int64_t p = voices_[i].position.load(std::memory_order_relaxed);
        if (p > maxPos) { maxPos = p; oldest = i; }
    }
    return oldest;
}

} // namespace wako::audio