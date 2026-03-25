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

    std::memset(out, 0, f * 2 * sizeof(float));

    float peakL = 0.0f;
    float peakR = 0.0f;

    float tmpTrack[MAX_PADS_METER] = {};

    for (auto& v : voices_) {
        if (!v.active || !v.buffer) continue;

        const AudioBuffer* buf = v.buffer;
        const float* s = buf->samples.data();

        float pos   = v.position;
        float pitch = v.pitch;

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

            // enveloppe
            v.env += v.envStep;
            if (v.env <= 0.0f) {
                v.active = false;
                break;
            }
            if (v.env > 1.0f) v.env = 1.0f;

            float gain = v.volume * v.env;

            float finalL = outL * gain;
            float finalR = outR * gain;

            out[i * 2]     += finalL;
            out[i * 2 + 1] += finalR;

            // peaks track
            if (v.padIdx >= 0 && v.padIdx < MAX_PADS_METER) {
                float pk = std::max(std::abs(finalL), std::abs(finalR));
                if (pk > tmpTrack[v.padIdx])
                    tmpTrack[v.padIdx] = pk;
            }

            // peaks master
            if (std::abs(finalL) > peakL) peakL = std::abs(finalL);
            if (std::abs(finalR) > peakR) peakR = std::abs(finalR);

            pos += pitch;
        }

        v.position = pos;
    }

    // effets master
    if (masterChain_)
        masterChain_->process(out, (int)f);

    // volume master
    for (unsigned long i = 0; i < f * 2; ++i)
        out[i] *= masterVolume;

    // store peaks
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
    int best = 0;
    float lowestEnergy = 1e9f;

    for (int i = 0; i < MAX_VOICES; ++i) {
        float energy = voices_[i].volume * voices_[i].env;
        if (energy < lowestEnergy) {
            lowestEnergy = energy;
            best = i;
        }
    }
    return best;
}

} // namespace wako::audio