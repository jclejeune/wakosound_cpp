#pragma once
#include "effects/EQ5.h"
#include "effects/Saturator.h"
#include "effects/Reverb.h"
#include "effects/Delay.h"

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// EffectChain — chaîne d'effets par channel
// Ordre : EQ5 → Saturateur → Reverb → Delay
//
// Toutes les instances sont pré-allouées — zéro allocation en RT.
// process() est appelé depuis le callback PortAudio.
// Tous les setters sont thread-safe (atomics dans chaque effet).
// ──────────────────────────────────────────────────────────────────
class EffectChain {
public:
    EffectChain() = default;

    void setSampleRate(int sr);
    void reset();

    // Traite un buffer stereo interleaved en place.
    // Appelé depuis le thread RT.
    void process(float* stereo, int frames) noexcept;

    // ── Accès aux effets (pour l'UI) ─────────────────────────────
    EQ5&       eq()       { return eq_; }
    Saturator& sat()      { return sat_; }
    Reverb&    reverb()   { return rev_; }
    Delay&     delay()    { return del_; }

    const EQ5&       eq()    const { return eq_; }
    const Saturator& sat()   const { return sat_; }
    const Reverb&    reverb()const { return rev_; }
    const Delay&     delay() const { return del_; }

private:
    EQ5       eq_;
    Saturator sat_;
    Reverb    rev_;
    Delay     del_;
};

} // namespace wako::audio