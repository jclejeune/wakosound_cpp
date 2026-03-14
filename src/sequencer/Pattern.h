#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
#include <algorithm>

namespace wako::seq {

static constexpr int MAX_PADS  = 9;
static constexpr int MAX_STEPS = 32;   // monte à 32

// ──────────────────────────────────────────────────────────────────
// Pattern — grille 2D [pad][step]
//
// Chaque track a sa propre longueur (1-32) et son propre step courant.
// patternLength reste disponible comme "set all tracks" raccourci.
// ──────────────────────────────────────────────────────────────────
struct Pattern {
    std::array<std::array<bool, MAX_STEPS>, MAX_PADS> grid{};

    // Longueur par track (indépendantes)
    std::array<int, MAX_PADS> trackLengths{};   // initialisé dans constructeur
    std::array<int, MAX_PADS> trackSteps{};     // step courant par track

    int bpm           = 120;
    int patternLength = 16;   // valeur "set all" — sync avec trackLengths

    Pattern() {
        trackLengths.fill(16);
        trackSteps.fill(0);
    }

    // ── Accès ──────────────────────────────────────────────────────
    bool get(int pad, int step) const {
        if (pad < 0 || pad >= MAX_PADS || step < 0 || step >= MAX_STEPS)
            return false;
        return grid[pad][step];
    }

    // Pads actifs sur le step courant de chaque track
    std::vector<int> activeNow() const;

    // ── Mutations ─────────────────────────────────────────────────
    Pattern& set(int pad, int step, bool v) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step] = v;
        return *this;
    }

    Pattern& toggle(int pad, int step) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step] = !grid[pad][step];
        return *this;
    }

    Pattern& clearAll() {
        for (auto& row : grid) row.fill(false);
        trackSteps.fill(0);
        return *this;
    }

    Pattern& clearPad(int pad) {
        if (pad >= 0 && pad < MAX_PADS) {
            grid[pad].fill(false);
            trackSteps[pad] = 0;
        }
        return *this;
    }

    Pattern& setBpm(int b) {
        bpm = std::max(1, std::min(9999, b));
        return *this;
    }

    // Set all tracks à la même longueur
    Pattern& setLength(int l) {
        patternLength = std::max(1, std::min(MAX_STEPS, l));
        trackLengths.fill(patternLength);
        return *this;
    }

    // Set longueur d'une track individuelle
    Pattern& setTrackLength(int pad, int l) {
        if (pad >= 0 && pad < MAX_PADS)
            trackLengths[pad] = std::max(1, std::min(MAX_STEPS, l));
        return *this;
    }

    // ── Avance toutes les tracks d'un tick ────────────────────────
    // Retourne les pads qui jouent ce tick
    std::vector<int> advance();

    // ── Timing ───────────────────────────────────────────────────
    static int stepIntervalMs(int bpm) {
        return std::max(1, (60 * 1000) / (bpm * 4));
    }

    // ── Sérialisation ────────────────────────────────────────────
    bool saveToFile(const std::string& path) const;
    static std::optional<Pattern> loadFromFile(const std::string& path);
};

// Steps courants par track — partagé entre Engine, StepGrid, MainWindow
using TrackSteps = std::array<int, MAX_PADS>;

} // namespace wako::seq