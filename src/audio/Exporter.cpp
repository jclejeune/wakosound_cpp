#include "Exporter.h"
#include "AudioCache.h"
#include "PitchCache.h"
#include <sndfile.h>
#include <cstring>
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

    // ── Paramètres de timing ──────────────────────────────────────
    int bpm          = pattern.bpm;
    int stepLenMs    = seq::Pattern::stepIntervalMs(bpm);
    int framesPerStep= static_cast<int>(
                           static_cast<double>(sampleRate) * stepLenMs / 1000.0);

    int patLen       = pattern.patternLength;
    int totalSteps   = patLen * loops;

    // Buffer de sortie complet
    std::vector<float> outBuffer;
    outBuffer.reserve(static_cast<size_t>(totalSteps) * framesPerStep * 2);

    // ── Simuler le séquenceur ─────────────────────────────────────
    // Copie du pattern pour simuler l'avance des steps
    seq::Pattern sim = pattern;
    sim.trackSteps.fill(0);

    float globalPeak = 0.f;

    // Réinitialiser les chaînes d'effets pour un render propre
    for (int i = 0; i < 10; ++i)
        if (chains[i]) chains[i]->reset();

    for (int step = 0; step < totalSteps; ++step) {
        // Récupérer les pads actifs sur ce step
        seq::TrackSteps currentSteps = sim.trackSteps;
        auto activePads = sim.advance();   // avance les positions

        // Step buffer temporaire
        std::vector<float> stepBuf(static_cast<size_t>(framesPerStep * 2), 0.f);

        float stepPeak = 0.f;
        mixStep(activePads, currentSteps, kitManager, pattern,
                chains, masterVolume,
                stepBuf, framesPerStep, sampleRate, stepPeak);

        globalPeak = std::max(globalPeak, stepPeak);

        // Ajouter le step au buffer global
        outBuffer.insert(outBuffer.end(), stepBuf.begin(), stepBuf.end());

        // Progression
        if (progressCb)
            progressCb(static_cast<float>(step + 1) / static_cast<float>(totalSteps));
    }

    result.peakLevel = globalPeak;
    result.clipping  = (globalPeak > 1.0f);

    // ── Écriture WAV via libsndfile ───────────────────────────────
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

    // Normaliser si clipping pour ne pas écrire des samples > ±1
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
// mixStep — simule un step du séquenceur en offline
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
    int                           /*sampleRate*/,
    float&                        peakOut)
{
    const auto* currentKit = kit.currentKit();
    if (!currentKit) return;

    // Buffers par track
    static constexpr int MAX_PADS = seq::MAX_PADS;
    std::vector<std::vector<float>> chanBufs(
        MAX_PADS,
        std::vector<float>(static_cast<size_t>(framesPerStep * 2), 0.f));

    // Pour chaque pad actif → écrire dans son channel buffer
    for (int padIdx : activePads) {
        if (!pat.shouldPlay(padIdx)) continue;

        const model::Pad* pad = currentKit->pad(padIdx);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;

        const seq::StepData& sd = pat.getStepData(padIdx, stepPositions[padIdx]);
        float vol = pad->volume * sd.volume * pat.trackVolumes[padIdx];
        int   pitch = sd.pitch;

        const AudioBuffer* buf = (pitch == 0)
            ? AudioCache::instance().get(pad->filePath)
            : PitchCache::instance().get(pad->filePath, pitch);
        if (!buf) continue;

        // Écrire les frames du sample dans le channel buffer
        auto& cb = chanBufs[padIdx];
        for (int f = 0; f < framesPerStep; ++f) {
            if (f >= buf->frameCount) break;
            cb[f * 2]     += buf->samples[f * 2]     * vol;
            cb[f * 2 + 1] += buf->samples[f * 2 + 1] * vol;
        }
    }

    // Appliquer les effect chains par track + sommer dans masterBuf
    std::vector<float> masterBuf(static_cast<size_t>(framesPerStep * 2), 0.f);

    for (int p = 0; p < MAX_PADS; ++p) {
        if (chains[p])
            chains[p]->process(chanBufs[p].data(), framesPerStep);

        for (int f = 0; f < framesPerStep * 2; ++f)
            masterBuf[f] += chanBufs[p][f];
    }

    // Appliquer master chain + master volume
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