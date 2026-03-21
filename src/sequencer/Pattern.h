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

struct StepData {
    bool  active = false;
    float volume = 1.0f;
    int   pitch  = 0;
    bool  gate   = false;

    bool hasCustomParams() const {
        return volume != 1.0f || pitch != 0 || gate;
    }
};

struct Pattern {
    std::array<std::array<StepData, MAX_STEPS>, MAX_PADS> grid{};

    std::array<int,  MAX_PADS> trackLengths{};
    std::array<int,  MAX_PADS> trackSteps{};
    std::array<bool, MAX_PADS> muted{};
    std::array<bool, MAX_PADS> soloed{};
    std::array<bool, MAX_PADS> trackGate{};

    // ── Mixage ────────────────────────────────────────────────────
    std::array<float, MAX_PADS> trackVolumes{};  // 0.0–1.0 par track
    float masterVolume = 1.0f;                   // volume master global

    int bpm           = 120;
    int patternLength = 16;

    Pattern() {
        trackLengths.fill(16);
        trackSteps.fill(0);
        muted.fill(false);
        soloed.fill(false);
        trackGate.fill(false);
        trackVolumes.fill(1.0f);
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

    bool anySolo() const {
        return std::any_of(soloed.begin(), soloed.end(),
                           [](bool s){ return s; });
    }

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
            grid[pad][step].volume = std::clamp(v, 0.f, 1.f);
        return *this;
    }
    Pattern& setStepPitch(int pad, int step, int p) {
        if (pad >= 0 && pad < MAX_PADS && step >= 0 && step < MAX_STEPS)
            grid[pad][step].pitch = std::clamp(p, -12, 12);
        return *this;
    }
    Pattern& toggleTrackGate(int pad) {
        if (pad >= 0 && pad < MAX_PADS) trackGate[pad] = !trackGate[pad];
        return *this;
    }
    Pattern& toggleMute(int pad) {
        if (pad >= 0 && pad < MAX_PADS) muted[pad] = !muted[pad];
        return *this;
    }
    Pattern& toggleSolo(int pad) {
        if (pad >= 0 && pad < MAX_PADS) soloed[pad] = !soloed[pad];
        return *this;
    }
    Pattern& setTrackVolume(int pad, float v) {
        if (pad >= 0 && pad < MAX_PADS)
            trackVolumes[pad] = std::clamp(v, 0.f, 1.f);
        return *this;
    }
    Pattern& setMasterVolume(float v) {
        masterVolume = std::clamp(v, 0.f, 1.f);
        return *this;
    }
    Pattern& clearAll() {
        for (auto& row : grid) row.fill(StepData{});
        trackSteps.fill(0);
        muted.fill(false);
        soloed.fill(false);
        trackGate.fill(false);
        trackVolumes.fill(1.0f);
        masterVolume = 1.0f;
        return *this;
    }
    Pattern& clearPad(int pad) {
        if (pad >= 0 && pad < MAX_PADS) {
            grid[pad].fill(StepData{});
            trackSteps[pad] = 0;
        }
        return *this;
    }
    Pattern& setBpm(int b)  { bpm = std::max(1, std::min(9999, b)); return *this; }
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