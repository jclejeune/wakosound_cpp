#include "Saturator.h"
#include <cmath>
#include <algorithm>

namespace wako::audio {

void Saturator::setDrive(float d) {
    drive_.store(std::clamp(d, 0.f, 1.f), std::memory_order_relaxed);
}
void Saturator::setMix(float m) {
    mix_.store(std::clamp(m, 0.f, 1.f), std::memory_order_relaxed);
}

// ──────────────────────────────────────────────────────────────────
// TUBE — atan asymétrique doux (overdrive style amplitube)
// ──────────────────────────────────────────────────────────────────
static inline float tubeSat(float x, float pg) noexcept {
    float driven = x * pg;
    float clipped = driven >= 0.f
        ? (2.f / 3.14159265f) * std::atan(driven * 1.5f)
        : std::tanh(driven * 1.2f);
    return clipped / std::sqrt(pg > 1.f ? pg : 1.f);
}

// ──────────────────────────────────────────────────────────────────
// TRANSISTOR — style TB-303 / Roland 808
//
// Circuit réel : diode clipping asymétrique + compression forte
// Alternance + : clipping doux à +0.7 V (diode silicium forward)
// Alternance - : clipping plus dur à -0.3 V (asymétrie transistor)
// Post-normalize : compresse pour garder niveau constant = "punch"
// ──────────────────────────────────────────────────────────────────
static inline float transistorSat(float x, float pg) noexcept {
    float driven = x * pg;

    // Seuils de clip asymétriques (normalisés)
    constexpr float THRESH_POS =  0.8f;   // diode forward ~0.7V
    constexpr float THRESH_NEG = -0.3f;   // transistor bias

    float clipped;
    if (driven > THRESH_POS) {
        // Soft knee au-dessus du seuil positif (polynomiale cubique)
        float over = driven - THRESH_POS;
        clipped = THRESH_POS + over / (1.f + over * over * 3.f);
    } else if (driven < THRESH_NEG) {
        // Hard knee négatif plus brutal (caractère transistor)
        float over = driven - THRESH_NEG;
        clipped = THRESH_NEG + over / (1.f + std::abs(over) * 6.f);
    } else {
        clipped = driven;
    }

    // Makeup gain normalisé : compense ET ajoute le "punch compressé"
    // À drive max le signal est réduit à ±1 mais avec volume compensé
    float makeup = pg > 1.f ? std::pow(pg, -0.6f) : 1.f;
    return clipped * makeup;
}

// ──────────────────────────────────────────────────────────────────
// FUZZ — rectification partielle + hard clip
// Style Big Muff / Fuzz Face :
// redresse les alternances négatives + sature très fort
// ──────────────────────────────────────────────────────────────────
static inline float fuzzSat(float x, float pg) noexcept {
    float driven = x * pg;

    // Redressement partiel (mélange entre le signal et sa valeur absolue)
    // rect=0 = symétrique, rect=1 = full wave rectify
    constexpr float RECT = 0.4f;
    float rectified = driven * (1.f - RECT) + std::abs(driven) * RECT;

    // Hard clip brutal à ±1
    float clipped = std::clamp(rectified, -1.f, 1.f);

    // Légère coloration basse → boost sub harmonique
    // (simplifié : on garde le signal tel quel ici,
    //  un vrai fuzz aurait un filtre après)
    float makeup = pg > 1.f ? 1.f / pg : 1.f;
    return clipped * makeup;
}

// ──────────────────────────────────────────────────────────────────
float Saturator::processSample(float x, float pregain, Mode mode) noexcept {
    switch (mode) {
        case Mode::Tube:        return tubeSat(x, pregain);
        case Mode::Transistor:  return transistorSat(x, pregain);
        case Mode::Fuzz:        return fuzzSat(x, pregain);
    }
    return x;
}

void Saturator::process(float* stereo, int frames) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    float drive  = drive_.load(std::memory_order_relaxed);
    float wet    = mix_.load(std::memory_order_relaxed);
    float dry    = 1.f - wet;
    Mode  mode   = static_cast<Mode>(mode_.load(std::memory_order_relaxed));

    // Pregain selon le mode :
    // Tube       : 1→4   (doux)
    // Transistor : 1→20  (agressif, 303/808 style)
    // Fuzz       : 1→40  (brutal)
    float maxGain = (mode == Mode::Tube) ? 4.f
                  : (mode == Mode::Transistor) ? 20.f
                  : 40.f;

    float pregain = 1.f + drive * drive * (maxGain - 1.f);

    for (int f = 0; f < frames; ++f) {
        float inL = stereo[f * 2];
        float inR = stereo[f * 2 + 1];

        stereo[f * 2]     = dry * inL + wet * processSample(inL, pregain, mode);
        stereo[f * 2 + 1] = dry * inR + wet * processSample(inR, pregain, mode);
    }
}

} // namespace wako::audio