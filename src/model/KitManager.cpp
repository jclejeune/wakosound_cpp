#include "KitManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace wako::model {

// ──────────────────────────────────────────────────────────────────
// Pad
// ──────────────────────────────────────────────────────────────────
bool Pad::valid() const {
    if (filePath.empty()) return false;
    auto ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".wav" && ext != ".mp3" && ext != ".aiff" && ext != ".flac")
        return false;
    return fs::exists(filePath);
}

std::string Pad::displayName() const {
    return enabled ? name : "[" + name + "]";
}

// ──────────────────────────────────────────────────────────────────
// KitManager::loadFromFile
//
// Format JSON attendu :
// {
//   "defaultKit": "default",
//   "kits": [
//     { "id": "default", "name": "Default", "description": "...",
//       "pads": [
//         { "name": "Kick", "file": "sounds/kick.wav", "volume": 1.0 },
//         ...
//       ]
//     }
//   ]
// }
// ──────────────────────────────────────────────────────────────────
bool KitManager::loadFromFile(const std::string& jsonPath) {
    std::ifstream f(jsonPath);
    if (!f.is_open()) {
        std::cerr << "[KitManager] Cannot open: " << jsonPath << "\n";
        return false;
    }

    json root;
    try {
        f >> root;
    } catch (const std::exception& e) {
        std::cerr << "[KitManager] JSON parse error: " << e.what() << "\n";
        return false;
    }

    const fs::path baseDir = fs::path(jsonPath).parent_path();
    std::string defaultId  = root.value("defaultKit", "");

    kits_.clear();
    for (const auto& jkit : root.at("kits")) {
        Kit kit;
        kit.name        = jkit.value("name", "Unnamed");
        kit.description = jkit.value("description", "");
        std::string kitId = jkit.value("id", "");

        for (const auto& jpad : jkit.at("pads")) {
            if (static_cast<int>(kit.pads.size()) >= Kit::MAX_PADS) break;
            Pad pad;
            pad.name        = jpad.value("name", "");
            pad.volume      = jpad.value("volume", 1.0f);
            pad.enabled     = jpad.value("enabled", true);
            pad.description = jpad.value("description", "");
            pad.color       = jpad.value("color", "");
            // Résoudre le chemin par rapport au répertoire du JSON
            std::string rel = jpad.value("file", "");
            if (!rel.empty())
                pad.filePath = (baseDir / rel).lexically_normal().string();
            kit.pads.push_back(std::move(pad));
        }
        kits_.push_back(std::move(kit));

        if (!defaultId.empty() && kitId == defaultId)
            currentIdx_ = static_cast<int>(kits_.size()) - 1;
    }

    if (kits_.empty()) {
        std::cerr << "[KitManager] No kits loaded\n";
        return false;
    }
    return true;
}

const Kit* KitManager::currentKit() const {
    if (currentIdx_ < 0 || currentIdx_ >= static_cast<int>(kits_.size()))
        return nullptr;
    return &kits_[currentIdx_];
}

bool KitManager::switchTo(int index) {
    if (index < 0 || index >= static_cast<int>(kits_.size())) return false;
    currentIdx_ = index;
    return true;
}

bool KitManager::switchByName(const std::string& name) {
    for (int i = 0; i < static_cast<int>(kits_.size()); ++i)
        if (kits_[i].name == name) { currentIdx_ = i; return true; }
    return false;
}

std::vector<std::string> KitManager::currentKitFilePaths() const {
    std::vector<std::string> paths;
    const Kit* kit = currentKit();
    if (!kit) return paths;
    for (const auto& pad : kit->pads)
        if (!pad.filePath.empty())
            paths.push_back(pad.filePath);
    return paths;
}

} // namespace wako::model
