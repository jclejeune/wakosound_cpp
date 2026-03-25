#include "VoicePool.h"

namespace wako::audio {

// ─────────────────────────────
int VoicePool::play(const AudioBuffer* buffer, float volume,
                    bool /*gate*/, int padIdx, int pitchSemitone) {
    if (!buffer || buffer->empty()) return -1;

    int slot = findFreeVoice();
    if (slot < 0) slot = stealVoice();

    Voice& v = voices_[slot];

    v.buffer   = buffer;
    v.position = 0.0f;
    v.volume   = volume;
    v.pitch    = std::pow(2.0f, pitchSemitone / 12.0f);

    v.env      = 0.0f;
    v.envStep  = 1.0f / 64.0f;

    v.active   = true;
    v.stopping = false;

    v.padIdx = padIdx;
    v.id     = nextId_++;

    return v.id;
}

// ─────────────────────────────
void VoicePool::stop(int voiceId) {
    for (auto& v : voices_) {
        if (v.id == voiceId && v.active) {
            v.stopping = true;
            v.envStep  = -1.0f / 64.0f;
        }
    }
}

void VoicePool::stopAll() {
    for (auto& v : voices_) {
        if (v.active) {
            v.stopping = true;
            v.envStep  = -1.0f / 64.0f;
        }
    }
}

// ─────────────────────────────
void VoicePool::setTrackChain(int pad, EffectChain* chain) {
    if (pad >= 0 && pad < MAX_PADS_METER)
        trackChains_[pad] = chain;
}

void VoicePool::setMasterChain(EffectChain* chain) {
    masterChain_ = chain;
}

// ─────────────────────────────
void VoicePool::mix(float* out, unsigned long frames, float masterVolume) noexcept {
    unsigned long f = std::min(frames, (unsigned long)MAX_FRAMES_PA);

    // ── 1. Vider les buffers par track et master ──────────────────
    for (auto& cb : chanBufs_)
        std::memset(cb.data(), 0, f * 2 * sizeof(float));
    std::memset(masterBuf_.data(), 0, f * 2 * sizeof(float));

    // ── 2. Chaque voix → son buffer de track ─────────────────────
    for (auto& v : voices_) {
        if (!v.active || !v.buffer) continue;

        const AudioBuffer* buf = v.buffer;
        const float* s = buf->samples.data();

        float pos   = v.position;
        float pitch = v.pitch;
        int   pad   = v.padIdx;

        // Destination : buffer du pad ou master si pad invalide
        float* dst = (pad >= 0 && pad < MAX_PADS_METER)
                     ? chanBufs_[pad].data()
                     : masterBuf_.data();

        for (unsigned long i = 0; i < f; ++i) {
            int idx = (int)pos;

            if (idx >= buf->frameCount - 1) {
                v.active = false;
                break;
            }

            float frac = pos - idx;

            float l0 = s[idx * 2];
            float r0 = s[idx * 2 + 1];
            float l1 = s[(idx + 1) * 2];
            float r1 = s[(idx + 1) * 2 + 1];

            float outL = l0 + (l1 - l0) * frac;
            float outR = r0 + (r1 - r0) * frac;

            v.env += v.envStep;
            if (v.env <= 0.0f) { v.active = false; break; }
            if (v.env > 1.0f)   v.env = 1.0f;

            float gain = v.volume * v.env;
            dst[i * 2]     += outL * gain;
            dst[i * 2 + 1] += outR * gain;

            pos += pitch;
        }

        v.position = pos;
    }

    // ── 3. Effets par track + peak + somme dans master ────────────
    float tmpTrack[MAX_PADS_METER] = {};

    for (int p = 0; p < MAX_PADS_METER; ++p) {
        float* cb = chanBufs_[p].data();

        // Appliquer la chaîne d'effets du track
        if (trackChains_[p])
            trackChains_[p]->process(cb, static_cast<int>(f));

        // Peak par track
        for (unsigned long i = 0; i < f; ++i) {
            float pk = std::max(std::abs(cb[i*2]), std::abs(cb[i*2+1]));
            if (pk > tmpTrack[p]) tmpTrack[p] = pk;
        }

        // Sommer dans le master
        for (unsigned long i = 0; i < f * 2; ++i)
            masterBuf_[i] += cb[i];
    }

    // ── 4. Effets master ──────────────────────────────────────────
    if (masterChain_)
        masterChain_->process(masterBuf_.data(), static_cast<int>(f));

    // ── 5. Master volume + peak master + copie output ─────────────
    float peakL = 0.f, peakR = 0.f;
    for (unsigned long i = 0; i < f; ++i) {
        masterBuf_[i*2]   *= masterVolume;
        masterBuf_[i*2+1] *= masterVolume;
        if (std::abs(masterBuf_[i*2])   > peakL) peakL = std::abs(masterBuf_[i*2]);
        if (std::abs(masterBuf_[i*2+1]) > peakR) peakR = std::abs(masterBuf_[i*2+1]);
    }
    std::memcpy(out, masterBuf_.data(), f * 2 * sizeof(float));

    if (frames > f)
        std::memset(out + f * 2, 0, (frames - f) * 2 * sizeof(float));

    // ── 6. Peaks (max hold) ───────────────────────────────────────
    for (int p = 0; p < MAX_PADS_METER; ++p) {
        float cur = trackPeaks_[p].load(std::memory_order_relaxed);
        if (tmpTrack[p] > cur)
            trackPeaks_[p].store(tmpTrack[p], std::memory_order_relaxed);
    }
    peakL_.store(peakL, std::memory_order_relaxed);
    peakR_.store(peakR, std::memory_order_relaxed);
}

// ─────────────────────────────
void VoicePool::decayPeaks(float factor) {
    for (auto& p : trackPeaks_) {
        float v = p.load(std::memory_order_relaxed) * factor;
        p.store(v, std::memory_order_relaxed);
    }
    peakL_.store(peakL_.load(std::memory_order_relaxed) * factor, std::memory_order_relaxed);
    peakR_.store(peakR_.load(std::memory_order_relaxed) * factor, std::memory_order_relaxed);
}

// ─────────────────────────────
int VoicePool::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i)
        if (!voices_[i].active)
            return i;
    return -1;
}

int VoicePool::stealVoice() {
    int   best        = 0;
    float maxProgress = -1.f;

    for (int i = 0; i < MAX_VOICES; ++i) {
        const Voice& v = voices_[i];
        if (!v.buffer || v.buffer->frameCount == 0) return i;

        float progress = (v.position * v.pitch) /
                         static_cast<float>(v.buffer->frameCount);

        if (progress > maxProgress) {
            maxProgress = progress;
            best = i;
        }
    }
    return best;
}

} // namespace wako::audio