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
    bool  gate   = false;  // true = tronqué (override local du mode global)

    bool hasCustomParams() const {
        return volume != 1.0f || pitch != 0 || gate;
    }
};

// ──────────────────────────────────────────────────────────────────
// Pattern — grille 2D [pad][step] de StepData
// ──────────────────────────────────────────────────────────────────
struct Pattern {
    std::array<std::array<StepData, MAX_STEPS>, MAX_PADS> grid{};

    std::array<int,  MAX_PADS> trackLengths{};
    std::array<int,  MAX_PADS> trackSteps{};
    std::array<bool, MAX_PADS> muted{};    // mute par track
    std::array<bool, MAX_PADS> soloed{};   // solo par track

    int bpm           = 120;
    int patternLength = 16;

    Pattern() {
        trackLengths.fill(16);
        trackSteps.fill(0);
        muted.fill(false);
        soloed.fill(false);
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

    // Vrai si au moins une track est en solo
    bool anySolo() const {
        return std::any_of(soloed.begin(), soloed.end(),
                           [](bool s){ return s; });
    }

    // Est-ce que ce pad doit jouer compte tenu de mute/solo ?
    bool shouldPlay(int pad) const {
        if (anySolo()) return soloed[pad];
        return !muted[pad];
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

    Pattern& toggleGate(int pad, int step) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].gate = !grid[pad][step].gate;
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

    Pattern& toggleMute(int pad) {
        if (pad >= 0 && pad < MAX_PADS)
            muted[pad] = !muted[pad];
        return *this;
    }

    Pattern& toggleSolo(int pad) {
        if (pad >= 0 && pad < MAX_PADS)
            soloed[pad] = !soloed[pad];
        return *this;
    }

    // Reset complet — notes + gate + mute/solo + longueurs
    Pattern& clearAll() {
        for (auto& row : grid) row.fill(StepData{});
        trackSteps.fill(0);
        muted.fill(false);
        soloed.fill(false);
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

    std::vector<int> advance();

    static int stepIntervalMs(int bpm) {
        return std::max(1, (60 * 1000) / (bpm * 4));
    }

    bool saveToFile(const std::string& path) const;
    static std::optional<Pattern> loadFromFile(const std::string& path);
};

using TrackSteps = std::array<int, MAX_PADS>;

} // namespace wako::seq