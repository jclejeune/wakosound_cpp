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
// Helpers privés
// ──────────────────────────────────────────────────────────────────
std::string KitManager::nameToId(const std::string& name) {
    std::string id;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (c == ' ' || c == '_' || c == '-')
            id += '_';
    }
    return id.empty() ? "kit" : id;
}

// Lecture d'un fichier JSON de kits.
// baseDir est utilisé pour résoudre les chemins relatifs des samples.
std::vector<Kit> KitManager::parseFile(const std::string& jsonPath, bool isFactory) {
    std::ifstream f(jsonPath);
    if (!f.is_open()) return {};

    json root;
    try { f >> root; }
    catch (const std::exception& e) {
        std::cerr << "[KitManager] JSON parse error in " << jsonPath
                  << ": " << e.what() << "\n";
        return {};
    }

    const fs::path base = fs::path(jsonPath).parent_path();
    std::vector<Kit> result;

    for (const auto& jkit : root.at("kits")) {
        Kit kit;
        kit.id          = jkit.value("id", "");
        kit.name        = jkit.value("name", "Unnamed");
        kit.description = jkit.value("description", "");
        kit.isFactory   = isFactory;

        for (const auto& jpad : jkit.at("pads")) {
            if (static_cast<int>(kit.pads.size()) >= Kit::MAX_PADS) break;
            Pad pad;
            pad.name        = jpad.value("name", "");
            pad.volume      = jpad.value("volume", 1.0f);
            pad.enabled     = jpad.value("enabled", true);
            pad.description = jpad.value("description", "");
            pad.color       = jpad.value("color", "");
            std::string rel = jpad.value("file", "");
            if (!rel.empty())
                pad.filePath = (base / rel).lexically_normal().string();
            kit.pads.push_back(std::move(pad));
        }
        result.push_back(std::move(kit));
    }
    return result;
}

bool KitManager::writeKits(const std::string& path,
                            const std::vector<Kit>& kits,
                            const fs::path& base) const {
    json root;
    auto jarr = json::array();
    for (const auto& kit : kits) {
        json jkit;
        jkit["id"]          = kit.id.empty() ? nameToId(kit.name) : kit.id;
        jkit["name"]        = kit.name;
        jkit["description"] = kit.description;

        auto jpads = json::array();
        for (const auto& pad : kit.pads) {
            json jpad;
            jpad["name"] = pad.name;

            // Stocker relatif si possible
            std::string stored;
            try {
                stored = fs::relative(fs::path(pad.filePath), base).generic_string();
            } catch (...) {
                stored = pad.filePath;
            }
            jpad["file"] = stored;

            if (pad.volume != 1.0f)       jpad["volume"]      = pad.volume;
            if (!pad.enabled)             jpad["enabled"]     = false;
            if (!pad.description.empty()) jpad["description"] = pad.description;
            if (!pad.color.empty())       jpad["color"]       = pad.color;
            jpads.push_back(jpad);
        }
        jkit["pads"] = jpads;
        jarr.push_back(jkit);
    }
    root["kits"] = jarr;

    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "[KitManager] Cannot write: " << path << "\n";
        return false;
    }
    f << root.dump(2);
    return true;
}

// ──────────────────────────────────────────────────────────────────
// API publique
// ──────────────────────────────────────────────────────────────────
bool KitManager::loadFactory(const std::string& jsonPath) {
    auto kits = parseFile(jsonPath, /*isFactory=*/true);
    if (kits.empty()) {
        std::cerr << "[KitManager] No factory kits in: " << jsonPath << "\n";
        return false;
    }
    kits_       = std::move(kits);
    currentIdx_ = 0;
    factoryBase_ = fs::absolute(fs::path(jsonPath)).parent_path();
    return true;
}

void KitManager::loadUser(const std::string& jsonPath) {
    userPath_ = jsonPath;
    auto userKits = parseFile(jsonPath, /*isFactory=*/false);
    for (auto& k : userKits)
        kits_.push_back(std::move(k));
    // currentIdx_ reste sur le premier kit factory
}

// ── Accès ─────────────────────────────────────────────────────────
const Kit* KitManager::currentKit() const {
    if (currentIdx_ < 0 || currentIdx_ >= static_cast<int>(kits_.size()))
        return nullptr;
    return &kits_[currentIdx_];
}

Kit* KitManager::currentKitMutable() {
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

// ── Mutations utilisateur ──────────────────────────────────────────
bool KitManager::setPadFile(int padIdx,
                             const std::string& filePath,
                             const std::string& name) {
    Kit* kit = currentKitMutable();
    if (!kit) return false;

    // Refus silencieux si factory — l'appelant doit d'abord dupliquer
    if (kit->isFactory) {
        std::cerr << "[KitManager] setPadFile: kit factory non modifiable\n";
        return false;
    }

    while (static_cast<int>(kit->pads.size()) <= padIdx) {
        Pad empty;
        empty.name    = "Pad " + std::to_string(kit->pads.size() + 1);
        empty.enabled = false;
        kit->pads.push_back(std::move(empty));
    }

    Pad& pad     = kit->pads[padIdx];
    pad.filePath = filePath;
    pad.enabled  = true;
    pad.name     = name.empty()
                   ? fs::path(filePath).stem().string()
                   : name;
    return true;
}

int KitManager::upsertUserKit(Kit kit) {
    kit.isFactory = false;
    if (kit.id.empty()) kit.id = nameToId(kit.name);

    // Cherche parmi les kits utilisateur uniquement
    for (int i = 0; i < static_cast<int>(kits_.size()); ++i) {
        if (!kits_[i].isFactory && kits_[i].name == kit.name) {
            kits_[i] = std::move(kit);
            return i;
        }
    }
    kits_.push_back(std::move(kit));
    return static_cast<int>(kits_.size()) - 1;
}

void KitManager::clearUserKits() {
    // Supprimer tous les kits !isFactory
    kits_.erase(
        std::remove_if(kits_.begin(), kits_.end(),
                       [](const Kit& k){ return !k.isFactory; }),
        kits_.end()
    );
    // Recentrer sur le premier kit factory si nécessaire
    currentIdx_ = 0;   // toujours rebascule sur le premier factory
}

// ── Persistance ───────────────────────────────────────────────────
bool KitManager::saveUserKits() const {
    if (userPath_.empty()) {
        std::cerr << "[KitManager] saveUserKits: userPath_ non défini\n";
        return false;
    }

    // Collecter les kits utilisateur
    std::vector<Kit> userKits;
    for (const auto& k : kits_)
        if (!k.isFactory) userKits.push_back(k);

    // Base = même dossier que le factory pour chemins relatifs cohérents
    return writeKits(userPath_, userKits, factoryBase_);
}

} // namespace wako::model