#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include "../audio/EffectChain.h"
#include <string>
#include <functional>
#include <array>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// ExportResult
// ──────────────────────────────────────────────────────────────────
struct ExportResult {
    bool        success  = false;
    bool        clipping = false;   // true si peak > 1.0 détecté
    float       peakLevel= 0.f;    // niveau max absolu
    std::string error;              // message si !success
};

// ──────────────────────────────────────────────────────────────────
// Exporter — render offline du pattern vers un fichier WAV
//
// Simule le séquenceur step par step sans PortAudio.
// Applique les EffectChains (tracks + master) et le masterVolume.
//
// progressCb : appelé avec 0.0→1.0 pendant le render (UI thread safe
//              si appelé depuis un QThread avec signal/slot).
// ──────────────────────────────────────────────────────────────────
class Exporter {
public:
    // sampleRate : 44100 Hz
    // framesPerStep : frames audio par step (calculé depuis BPM)
    static ExportResult render(
        const seq::Pattern&                          pattern,
        const model::KitManager&                     kitManager,
        std::array<EffectChain*, 10>&                chains,    // 0-8 tracks, 9 master
        float                                        masterVolume,
        int                                          loops,
        const std::string&                           outputPath,
        int                                          sampleRate,
        std::function<void(float)>                   progressCb = nullptr
    );

private:
    // Mix un step dans outBuffer (stereo interleaved)
    static void mixStep(
        const std::vector<int>&  activePads,
        const seq::TrackSteps&   stepPositions,
        const model::KitManager& kit,
        const seq::Pattern&      pat,
        std::array<EffectChain*, 10>& chains,
        float                    masterVolume,
        std::vector<float>&      outBuffer,
        int                      framesPerStep,
        int                      sampleRate,
        float&                   peakOut
    );
};

} // namespace wako::audio