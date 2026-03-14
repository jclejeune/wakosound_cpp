#include "AudioCache.h"
#include <sndfile.h>
#include <iostream>
#include <algorithm>

namespace wako::audio {

AudioCache& AudioCache::instance() {
    static AudioCache inst;
    return inst;
}

const AudioBuffer* AudioCache::get(const std::string& filePath) {
    {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(filePath);
        if (it != cache_.end())
            return &it->second;
    }
    // Chargement hors lock pour ne pas bloquer les autres threads
    auto maybeBuffer = loadFile(filePath);
    if (!maybeBuffer) return nullptr;

    std::lock_guard lock(mutex_);
    // Double-check : un autre thread a peut-être chargé entre temps
    auto [it, inserted] = cache_.emplace(filePath, std::move(*maybeBuffer));
    return &it->second;
}

void AudioCache::preload(const std::vector<std::string>& paths) {
    for (const auto& p : paths)
        get(p);
}

void AudioCache::clear() {
    std::lock_guard lock(mutex_);
    cache_.clear();
}

std::size_t AudioCache::size() const {
    std::lock_guard lock(mutex_);
    return cache_.size();
}

// ──────────────────────────────────────────────────────────────────
// Chargement via libsndfile → float stereo interleaved
// Si le fichier est mono, on duplique sur les deux canaux.
// ──────────────────────────────────────────────────────────────────
std::optional<AudioBuffer> AudioCache::loadFile(const std::string& path) {
    SF_INFO info{};
    SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
    if (!sf) {
        std::cerr << "[AudioCache] Cannot open: " << path
                  << " — " << sf_strerror(nullptr) << "\n";
        return std::nullopt;
    }

    const int outChannels = 2;
    const int64_t frames = info.frames;

    AudioBuffer buf;
    buf.channels   = outChannels;
    buf.sampleRate = info.samplerate;
    buf.frameCount = frames;
    buf.samples.resize(static_cast<size_t>(frames * outChannels));

    if (info.channels == 1) {
        // Mono → lire en mono puis dupliquer
        std::vector<float> mono(static_cast<size_t>(frames));
        sf_readf_float(sf, mono.data(), frames);
        for (int64_t i = 0; i < frames; ++i) {
            buf.samples[i * 2]     = mono[i];
            buf.samples[i * 2 + 1] = mono[i];
        }
    } else if (info.channels == 2) {
        sf_readf_float(sf, buf.samples.data(), frames);
    } else {
        // > 2 canaux : on prend L et R uniquement
        std::vector<float> multi(static_cast<size_t>(frames * info.channels));
        sf_readf_float(sf, multi.data(), frames);
        for (int64_t i = 0; i < frames; ++i) {
            buf.samples[i * 2]     = multi[i * info.channels];
            buf.samples[i * 2 + 1] = multi[i * info.channels + 1];
        }
    }

    sf_close(sf);
    return buf;
}

} // namespace wako::audio
