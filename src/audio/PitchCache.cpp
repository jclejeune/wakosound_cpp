#include "PitchCache.h"
#include <rubberband/RubberBandStretcher.h>
#include <cmath>
#include <iostream>

namespace wako::audio {

PitchCache& PitchCache::instance() {
    static PitchCache inst;
    return inst;
}

const AudioBuffer* PitchCache::get(const std::string& filePath, int semitones) {
    // pitch 0 → AudioCache direct, pas de traitement
    if (semitones == 0)
        return AudioCache::instance().get(filePath);

    Key key{filePath, semitones};

    {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end())
            return &it->second;
    }

    // Charger le buffer original
    const AudioBuffer* original = AudioCache::instance().get(filePath);
    if (!original) return nullptr;

    // Traiter hors lock (RubberBand peut être lent sur gros fichiers)
    AudioBuffer shifted = process(*original, semitones);

    std::lock_guard lock(mutex_);
    // Double-check
    auto it = cache_.find(key);
    if (it != cache_.end())
        return &it->second;

    auto [newIt, _] = cache_.emplace(key, std::move(shifted));
    return &newIt->second;
}

// ──────────────────────────────────────────────────────────────────
// process — RubberBand offline, durée préservée
// ──────────────────────────────────────────────────────────────────
AudioBuffer PitchCache::process(const AudioBuffer& input, int semitones) {
    double pitchScale = std::pow(2.0, semitones / 12.0);

    using namespace RubberBand;
    RubberBandStretcher stretcher(
        input.sampleRate,
        2,  // stereo
        RubberBandStretcher::OptionProcessOffline |
        RubberBandStretcher::OptionPitchHighQuality
    );

    stretcher.setPitchScale(pitchScale);
    stretcher.setTimeRatio(1.0);  // durée préservée

    // Désentrelacer L/R pour RubberBand
    auto frames = static_cast<int>(input.frameCount);
    std::vector<float> left(frames), right(frames);
    for (int i = 0; i < frames; ++i) {
        left[i]  = input.samples[i * 2];
        right[i] = input.samples[i * 2 + 1];
    }

    const float* inChannels[2] = {left.data(), right.data()};

    // Phase d'étude (offline)
    stretcher.study(inChannels, frames, true);

    // Phase de traitement
    stretcher.process(inChannels, frames, true);

    // Récupérer la sortie
    int available = stretcher.available();
    if (available <= 0) {
        std::cerr << "[PitchCache] RubberBand: no output available\n";
        return input;  // fallback : retourner l'original
    }

    std::vector<float> outL(available), outR(available);
    float* outChannels[2] = {outL.data(), outR.data()};
    stretcher.retrieve(outChannels, available);

    // Réentrelacer
    AudioBuffer out;
    out.sampleRate = input.sampleRate;
    out.channels   = 2;
    out.frameCount = available;
    out.samples.resize(static_cast<size_t>(available * 2));

    for (int i = 0; i < available; ++i) {
        out.samples[i * 2]     = outL[i];
        out.samples[i * 2 + 1] = outR[i];
    }

    return out;
}

void PitchCache::clear() {
    std::lock_guard lock(mutex_);
    cache_.clear();
}

} // namespace wako::audio