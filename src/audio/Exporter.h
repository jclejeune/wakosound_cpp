#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include "../audio/EffectChain.h"
#include <string>
#include <functional>
#include <array>

namespace wako::audio {

struct ExportResult {
    bool        success  = false;
    bool        clipping = false;
    float       peakLevel= 0.f;
    std::string error;
};

// ──────────────────────────────────────────────────────────────────
// Exporter — render offline du pattern vers un fichier WAV
//
// Simule le séquenceur step par step sans PortAudio.
// Le pitch est géré par lecture à vitesse variable (identique au live) :
// pitchFactor = 2^(semitones/12), interpolation linéaire entre frames.
// Zéro latence, zéro dépendance externe.
// ──────────────────────────────────────────────────────────────────
class Exporter {
public:
    static ExportResult render(
        const seq::Pattern&                          pattern,
        const model::KitManager&                     kitManager,
        std::array<EffectChain*, 10>&                chains,
        float                                        masterVolume,
        int                                          loops,
        const std::string&                           outputPath,
        int                                          sampleRate,
        std::function<void(float)>                   progressCb = nullptr
    );

private:
    static void mixStep(
        const std::vector<int>&       activePads,
        const seq::TrackSteps&        stepPositions,
        const model::KitManager&      kit,
        const seq::Pattern&           pat,
        std::array<EffectChain*, 10>& chains,
        float                         masterVolume,
        std::vector<float>&           outBuffer,
        int                           framesPerStep,
        float&                        peakOut
    );
};

} // namespace wako::audio