#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
#include <algorithm>

namespace wako::seq {

static constexpr int MAX_PADS  = 9;
static constexpr int MAX_STEPS = 32;

// ──────────────────────────────────────────────────────────────────
// StepData — paramètres d'un step individuel
// ──────────────────────────────────────────────────────────────────
struct StepData {
    bool  active = false;
    float volume = 1.0f;   // 0.0 → 1.0
    int   pitch  = 0;      // semitones -12 → +12

    bool hasCustomParams() const {
        return volume != 1.0f || pitch != 0;
    }
};

// ──────────────────────────────────────────────────────────────────
// Pattern — grille 2D [pad][step] de StepData
// ──────────────────────────────────────────────────────────────────
struct Pattern {
    std::array<std::array<StepData, MAX_STEPS>, MAX_PADS> grid{};

    std::array<int, MAX_PADS> trackLengths{};
    std::array<int, MAX_PADS> trackSteps{};

    int bpm           = 120;
    int patternLength = 16;

    Pattern() {
        trackLengths.fill(16);
        trackSteps.fill(0);
    }

    // ── Accès ─────────────────────────────────────────────────────
    bool get(int pad, int step) const {
        if (pad < 0 || pad >= MAX_PADS || step < 0 || step >= MAX_STEPS)
            return false;
        return grid[pad][step].active;
    }

    const StepData& getStepData(int pad, int step) const {
        return grid[pad][step];
    }

    StepData& getStepData(int pad, int step) {
        return grid[pad][step];
    }

    std::vector<int> activeNow() const;

    // ── Mutations ─────────────────────────────────────────────────
    Pattern& set(int pad, int step, bool v) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].active = v;
        return *this;
    }

    Pattern& toggle(int pad, int step) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].active = !grid[pad][step].active;
        return *this;
    }

    Pattern& setStepVolume(int pad, int step, float v) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].volume = std::clamp(v, 0.0f, 1.0f);
        return *this;
    }

    Pattern& setStepPitch(int pad, int step, int p) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].pitch = std::clamp(p, -12, 12);
        return *this;
    }

    Pattern& clearAll() {
        for (auto& row : grid) row.fill(StepData{});
        trackSteps.fill(0);
        return *this;
    }

    Pattern& clearPad(int pad) {
        if (pad >= 0 && pad < MAX_PADS) {
            grid[pad].fill(StepData{});
            trackSteps[pad] = 0;
        }
        return *this;
    }

    Pattern& setBpm(int b) {
        bpm = std::max(1, std::min(9999, b));
        return *this;
    }

    Pattern& setLength(int l) {
        patternLength = std::max(1, std::min(MAX_STEPS, l));
        trackLengths.fill(patternLength);
        return *this;
    }

    Pattern& setTrackLength(int pad, int l) {
        if (pad >= 0 && pad < MAX_PADS)
            trackLengths[pad] = std::max(1, std::min(MAX_STEPS, l));
        return *this;
    }

    // ── Avance toutes les tracks ───────────────────────────────────
    // Retourne les pads actifs ce tick
    std::vector<int> advance();

    // ── Timing ───────────────────────────────────────────────────
    static int stepIntervalMs(int bpm) {
        return std::max(1, (60 * 1000) / (bpm * 4));
    }

    // ── Sérialisation ────────────────────────────────────────────
    bool saveToFile(const std::string& path) const;
    static std::optional<Pattern> loadFromFile(const std::string& path);
};

using TrackSteps = std::array<int, MAX_PADS>;

} // namespace wako::seq