#include "PitchCache.h"
#include <cstring>                           // requis par signalsmith-linear/fft.h
#include <signalsmith-stretch.h>
#include <cmath>
#include <iostream>

namespace wako::audio {

PitchCache& PitchCache::instance() {
    static PitchCache inst;
    return inst;
}

const AudioBuffer* PitchCache::get(const std::string& filePath, int semitones) {
    if (semitones == 0)
        return AudioCache::instance().get(filePath);

    Key key{filePath, semitones};

    {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end())
            return &it->second;
    }

    const AudioBuffer* original = AudioCache::instance().get(filePath);
    if (!original) return nullptr;

    AudioBuffer shifted = process(*original, semitones);

    std::lock_guard lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end())
        return &it->second;

    auto [newIt, _] = cache_.emplace(key, std::move(shifted));
    return &newIt->second;
}

// ──────────────────────────────────────────────────────────────────
// process — signalsmith-stretch offline
// API : presetDefault → setTransposeFactor → process(in, n, out, n)
// ──────────────────────────────────────────────────────────────────
AudioBuffer PitchCache::process(const AudioBuffer& input, int semitones) {
    double pitchScale = std::pow(2.0, semitones / 12.0);

    const int channels   = 2;
    const int sampleRate = input.sampleRate;
    const int frames     = static_cast<int>(input.frameCount);

    // Désentrelacer L/R
    std::vector<float> left(frames), right(frames);
    for (int i = 0; i < frames; ++i) {
        left[i]  = input.samples[i * 2];
        right[i] = input.samples[i * 2 + 1];
    }

    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    stretcher.presetDefault(channels, static_cast<float>(sampleRate));
    stretcher.setTransposeFactor(static_cast<float>(pitchScale));

    constexpr int BLOCK = 1024;

    std::vector<float> outL, outR;
    outL.reserve(frames);
    outR.reserve(frames);

    std::vector<float> tmpL(BLOCK), tmpR(BLOCK);
    float* outPtrs[2] = { tmpL.data(), tmpR.data() };

    int pos = 0;
    while (pos < frames) {
        int toProcess = std::min(BLOCK, frames - pos);

        const float* inBlock[2] = {
            left.data()  + pos,
            right.data() + pos
        };

        // 4 arguments : inputs, inputSamples, outputs, outputSamples
        stretcher.process(inBlock, toProcess, outPtrs, toProcess);

        for (int i = 0; i < toProcess; ++i) {
            outL.push_back(tmpL[i]);
            outR.push_back(tmpR[i]);
        }
        pos += toProcess;
    }

    int outFrames = static_cast<int>(outL.size());
    if (outFrames == 0) {
        std::cerr << "[PitchCache] signalsmith: no output\n";
        return input;
    }

    AudioBuffer out;
    out.sampleRate = sampleRate;
    out.channels   = channels;
    out.frameCount = outFrames;
    out.samples.resize(static_cast<size_t>(outFrames * 2));

    for (int i = 0; i < outFrames; ++i) {
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