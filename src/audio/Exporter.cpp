#include "Exporter.h"
#include "AudioCache.h"
#include <sndfile.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// render
// ──────────────────────────────────────────────────────────────────
ExportResult Exporter::render(
    const seq::Pattern&           pattern,
    const model::KitManager&      kitManager,
    std::array<EffectChain*, 10>& chains,
    float                         masterVolume,
    int                           loops,
    const std::string&            outputPath,
    int                           sampleRate,
    std::function<void(float)>    progressCb)
{
    ExportResult result;

    int bpm          = pattern.bpm;
    int stepLenMs    = seq::Pattern::stepIntervalMs(bpm);
    int framesPerStep= static_cast<int>(
                           static_cast<double>(sampleRate) * stepLenMs / 1000.0);

    int patLen       = pattern.patternLength;
    int totalSteps   = patLen * loops;

    std::vector<float> outBuffer;
    outBuffer.reserve(static_cast<size_t>(totalSteps) * framesPerStep * 2);

    seq::Pattern sim = pattern;
    sim.trackSteps.fill(0);

    float globalPeak = 0.f;

    for (int i = 0; i < 10; ++i)
        if (chains[i]) chains[i]->reset();

    for (int step = 0; step < totalSteps; ++step) {
        seq::TrackSteps currentSteps = sim.trackSteps;
        auto activePads = sim.advance();

        std::vector<float> stepBuf(static_cast<size_t>(framesPerStep * 2), 0.f);

        float stepPeak = 0.f;
        mixStep(activePads, currentSteps, kitManager, pattern,
                chains, masterVolume,
                stepBuf, framesPerStep, stepPeak);

        globalPeak = std::max(globalPeak, stepPeak);
        outBuffer.insert(outBuffer.end(), stepBuf.begin(), stepBuf.end());

        if (progressCb)
            progressCb(static_cast<float>(step + 1) / static_cast<float>(totalSteps));
    }

    result.peakLevel = globalPeak;
    result.clipping  = (globalPeak > 1.0f);

    SF_INFO info{};
    info.samplerate = sampleRate;
    info.channels   = 2;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* sf = sf_open(outputPath.c_str(), SFM_WRITE, &info);
    if (!sf) {
        result.success = false;
        result.error   = sf_strerror(nullptr);
        std::cerr << "[Exporter] Cannot open output: " << result.error << "\n";
        return result;
    }

    if (result.clipping && globalPeak > 0.f) {
        float norm = 1.0f / globalPeak;
        for (auto& s : outBuffer) s *= norm;
    }

    sf_count_t written = sf_writef_float(sf, outBuffer.data(),
                                         static_cast<sf_count_t>(outBuffer.size() / 2));
    sf_close(sf);

    if (written <= 0) {
        result.success = false;
        result.error   = "Écriture WAV échouée";
        return result;
    }

    result.success = true;
    return result;
}

// ──────────────────────────────────────────────────────────────────
// mixStep
// Le pitch est géré comme dans VoicePool : lecture à vitesse variable
// (pos += pitchFactor) avec interpolation linéaire entre frames.
// Même comportement qu'en live — durée raccourcie/allongée, zéro latence.
// ──────────────────────────────────────────────────────────────────
void Exporter::mixStep(
    const std::vector<int>&       activePads,
    const seq::TrackSteps&        stepPositions,
    const model::KitManager&      kit,
    const seq::Pattern&           pat,
    std::array<EffectChain*, 10>& chains,
    float                         masterVolume,
    std::vector<float>&           outBuffer,
    int                           framesPerStep,
    float&                        peakOut)
{
    const auto* currentKit = kit.currentKit();
    if (!currentKit) return;

    static constexpr int MAX_PADS = seq::MAX_PADS;
    std::vector<std::vector<float>> chanBufs(
        MAX_PADS,
        std::vector<float>(static_cast<size_t>(framesPerStep * 2), 0.f));

    for (int padIdx : activePads) {
        if (!pat.shouldPlay(padIdx)) continue;

        const model::Pad* pad = currentKit->pad(padIdx);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;

        const seq::StepData& sd = pat.getStepData(padIdx, stepPositions[padIdx]);
        float vol         = pad->volume * sd.volume * pat.trackVolumes[padIdx];
        float pitchFactor = std::pow(2.0f, sd.pitch / 12.0f);

        const AudioBuffer* buf = AudioCache::instance().get(pad->filePath);
        if (!buf) continue;

        auto& cb    = chanBufs[padIdx];
        float pos   = 0.f;
        int64_t len = buf->frameCount;

        for (int f = 0; f < framesPerStep; ++f) {
            int idx = static_cast<int>(pos);
            if (idx >= len - 1) break;

            float frac = pos - idx;
            float l = buf->samples[idx*2]     + (buf->samples[(idx+1)*2]     - buf->samples[idx*2])     * frac;
            float r = buf->samples[idx*2 + 1] + (buf->samples[(idx+1)*2 + 1] - buf->samples[idx*2 + 1]) * frac;

            cb[f*2]     += l * vol;
            cb[f*2 + 1] += r * vol;
            pos         += pitchFactor;
        }
    }

    std::vector<float> masterBuf(static_cast<size_t>(framesPerStep * 2), 0.f);

    for (int p = 0; p < MAX_PADS; ++p) {
        if (chains[p])
            chains[p]->process(chanBufs[p].data(), framesPerStep);
        for (int f = 0; f < framesPerStep * 2; ++f)
            masterBuf[f] += chanBufs[p][f];
    }

    if (chains[9])
        chains[9]->process(masterBuf.data(), framesPerStep);

    float peak = 0.f;
    for (int f = 0; f < framesPerStep * 2; ++f) {
        masterBuf[f] *= masterVolume;
        float a = std::abs(masterBuf[f]);
        if (a > peak) peak = a;
    }

    peakOut = peak;
    std::copy(masterBuf.begin(), masterBuf.end(), outBuffer.begin());
}

} // namespace wako::audio