#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

namespace wako::seq {

static constexpr int MAX_PADS  = 9;
static constexpr int MAX_STEPS = 16;

// ──────────────────────────────────────────────────────────────────
// Pattern — grille 2D [pad][step] de booléens
// Toutes les méthodes sont pures (const ou retournent une copie).
// ──────────────────────────────────────────────────────────────────
struct Pattern {
    std::array<std::array<bool, MAX_STEPS>, MAX_PADS> grid{};
    int patternLength = 16;
    int bpm           = 120;
    int currentStep   = 0;

    // Accès
    bool get(int pad, int step) const { return grid[pad][step]; }
    std::vector<int> activeAtStep(int step) const;

    // Mutations (retournent *this pour chaîner si besoin)
    Pattern& set(int pad, int step, bool v)   { grid[pad][step] = v; return *this; }
    Pattern& toggle(int pad, int step)         { grid[pad][step] = !grid[pad][step]; return *this; }
    Pattern& clearAll();
    Pattern& clearPad(int pad);
    Pattern& setBpm(int b)   { bpm           = std::max(1, std::min(9999, b)); return *this; }
    Pattern& setLength(int l){ patternLength = std::max(1, std::min(MAX_STEPS, l)); return *this; }

    // Avance d'un step (retourne le step joué)
    int advance();

    // Calcul timing
    static int stepIntervalMs(int bpm) { return (60 * 1000) / (bpm * 4); }

    //save pattern
    bool saveToFile(const std::string& path) const;
    static std::optional<Pattern> loadFromFile(const std::string& path);
};

} // namespace wako::seq
