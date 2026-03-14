#include "Pattern.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace wako::seq {

// ──────────────────────────────────────────────────────────────────
// activeNow — pads actifs sur leur step courant respectif
// ──────────────────────────────────────────────────────────────────
std::vector<int> Pattern::activeNow() const {
    std::vector<int> result;
    result.reserve(MAX_PADS);
    for (int p = 0; p < MAX_PADS; ++p)
        if (grid[p][trackSteps[p]])
            result.push_back(p);
    return result;
}

// ──────────────────────────────────────────────────────────────────
// advance — avance chaque track indépendamment
// Retourne les pads actifs sur ce tick (AVANT d'avancer)
// ──────────────────────────────────────────────────────────────────
std::vector<int> Pattern::advance() {
    // 1. Collecter les pads actifs sur le step courant de chaque track
    auto active = activeNow();

    // 2. Avancer chaque track sur sa propre longueur
    for (int p = 0; p < MAX_PADS; ++p)
        trackSteps[p] = (trackSteps[p] + 1) % trackLengths[p];

    return active;
}

// ──────────────────────────────────────────────────────────────────
// save — inclut trackLengths pour la polymétrie
// ──────────────────────────────────────────────────────────────────
bool Pattern::saveToFile(const std::string& path) const {
    nlohmann::json j;
    j["bpm"]    = bpm;
    j["length"] = patternLength;

    // trackLengths par track
    auto jLengths = nlohmann::json::array();
    for (int p = 0; p < MAX_PADS; ++p)
        jLengths.push_back(trackLengths[p]);
    j["trackLengths"] = jLengths;

    // grille
    auto jgrid = nlohmann::json::array();
    for (int p = 0; p < MAX_PADS; ++p) {
        auto row = nlohmann::json::array();
        for (int s = 0; s < MAX_STEPS; ++s)
            row.push_back(grid[p][s]);
        jgrid.push_back(row);
    }
    j["grid"] = jgrid;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

// ──────────────────────────────────────────────────────────────────
// load — backward compatible avec l'ancien format (sans trackLengths)
// ──────────────────────────────────────────────────────────────────
std::optional<Pattern> Pattern::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;

    nlohmann::json j;
    try { f >> j; } catch (...) { return std::nullopt; }

    Pattern pat;
    pat.setBpm(j.value("bpm", 120));
    int globalLength = j.value("length", 16);
    pat.patternLength = globalLength;

    // trackLengths — fallback sur globalLength si absent (ancien format)
    if (j.contains("trackLengths") && j["trackLengths"].is_array()) {
        for (int p = 0; p < MAX_PADS && p < (int)j["trackLengths"].size(); ++p)
            pat.trackLengths[p] = std::max(1, std::min(MAX_STEPS,
                                  j["trackLengths"][p].get<int>()));
    } else {
        pat.trackLengths.fill(globalLength);
    }

    // grille
    if (j.contains("grid") && j["grid"].is_array()) {
        for (int p = 0; p < MAX_PADS && p < (int)j["grid"].size(); ++p) {
            const auto& row = j["grid"][p];
            for (int s = 0; s < MAX_STEPS && s < (int)row.size(); ++s)
                pat.grid[p][s] = row[s].get<bool>();
        }
    }

    pat.trackSteps.fill(0);
    return pat;
}

} // namespace wako::seq