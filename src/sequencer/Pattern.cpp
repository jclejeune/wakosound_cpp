#include "Pattern.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace wako::seq {

std::vector<int> Pattern::activeNow() const {
    std::vector<int> result;
    result.reserve(MAX_PADS);
    for (int p = 0; p < MAX_PADS; ++p)
        if (grid[p][trackSteps[p]].active)
            result.push_back(p);
    return result;
}

std::vector<int> Pattern::advance() {
    auto active = activeNow();
    for (int p = 0; p < MAX_PADS; ++p)
        trackSteps[p] = (trackSteps[p] + 1) % trackLengths[p];
    return active;
}

bool Pattern::saveToFile(const std::string& path) const {
    nlohmann::json j;
    j["bpm"]    = bpm;
    j["length"] = patternLength;

    auto jLengths = nlohmann::json::array();
    for (int p = 0; p < MAX_PADS; ++p)
        jLengths.push_back(trackLengths[p]);
    j["trackLengths"] = jLengths;

    // mute/solo/gate par track
    auto jMuted     = nlohmann::json::array();
    auto jSoloed    = nlohmann::json::array();
    auto jTrackGate = nlohmann::json::array();
    for (int p = 0; p < MAX_PADS; ++p) {
        jMuted.push_back(muted[p]);
        jSoloed.push_back(soloed[p]);
        jTrackGate.push_back(trackGate[p]);
    }
    j["muted"]     = jMuted;
    j["soloed"]    = jSoloed;
    j["trackGate"] = jTrackGate;

    auto jgrid = nlohmann::json::array();
    for (int p = 0; p < MAX_PADS; ++p) {
        auto row = nlohmann::json::array();
        for (int s = 0; s < MAX_STEPS; ++s) {
            const auto& sd = grid[p][s];
            if (sd.active && sd.hasCustomParams()) {
                row.push_back({
                    {"a", sd.active},
                    {"v", sd.volume},
                    {"p", sd.pitch},
                    {"g", sd.gate}
                });
            } else {
                row.push_back(sd.active);
            }
        }
        jgrid.push_back(row);
    }
    j["grid"] = jgrid;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

std::optional<Pattern> Pattern::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;

    nlohmann::json j;
    try { f >> j; } catch (...) { return std::nullopt; }

    Pattern pat;
    pat.setBpm(j.value("bpm", 120));
    int globalLength = j.value("length", 16);
    pat.patternLength = globalLength;

    if (j.contains("trackLengths") && j["trackLengths"].is_array())
        for (int p = 0; p < MAX_PADS && p < (int)j["trackLengths"].size(); ++p)
            pat.trackLengths[p] = std::max(1, std::min(MAX_STEPS,
                                  j["trackLengths"][p].get<int>()));
    else
        pat.trackLengths.fill(globalLength);

    if (j.contains("muted") && j["muted"].is_array())
        for (int p = 0; p < MAX_PADS && p < (int)j["muted"].size(); ++p)
            pat.muted[p] = j["muted"][p].get<bool>();

    if (j.contains("soloed") && j["soloed"].is_array())
        for (int p = 0; p < MAX_PADS && p < (int)j["soloed"].size(); ++p)
            pat.soloed[p] = j["soloed"][p].get<bool>();

    if (j.contains("trackGate") && j["trackGate"].is_array())
        for (int p = 0; p < MAX_PADS && p < (int)j["trackGate"].size(); ++p)
            pat.trackGate[p] = j["trackGate"][p].get<bool>();

    if (j.contains("grid") && j["grid"].is_array()) {
        for (int p = 0; p < MAX_PADS && p < (int)j["grid"].size(); ++p) {
            const auto& row = j["grid"][p];
            for (int s = 0; s < MAX_STEPS && s < (int)row.size(); ++s) {
                const auto& cell = row[s];
                StepData& sd = pat.grid[p][s];
                if (cell.is_boolean()) {
                    sd.active = cell.get<bool>();
                } else if (cell.is_object()) {
                    sd.active = cell.value("a", false);
                    sd.volume = std::clamp(cell.value("v", 1.0f), 0.0f, 1.0f);
                    sd.pitch  = std::clamp(cell.value("p", 0), -12, 12);
                    sd.gate   = cell.value("g", false);
                }
            }
        }
    }

    pat.trackSteps.fill(0);
    return pat;
}

} // namespace wako::seq