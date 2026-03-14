#pragma once
#include <string>
#include <vector>
#include <optional>

namespace wako::model {

// ──────────────────────────────────────────────────────────────────
// Pad
// ──────────────────────────────────────────────────────────────────
struct Pad {
    std::string name;
    std::string filePath;
    float       volume      = 1.0f;
    bool        enabled     = true;
    std::string description;
    std::string color;          // optionnel : "#RRGGBB" ou nom CSS

    bool valid() const;
    std::string displayName() const;
};

// ──────────────────────────────────────────────────────────────────
// Kit — 9 pads max (grille 3×3)
// ──────────────────────────────────────────────────────────────────
struct Kit {
    static constexpr int MAX_PADS = 9;

    std::string       name;
    std::string       description;
    std::vector<Pad>  pads;        // size ≤ MAX_PADS

    bool         full()    const { return static_cast<int>(pads.size()) == MAX_PADS; }
    bool         empty()   const { return pads.empty(); }
    int          padCount()const { return static_cast<int>(pads.size()); }
    const Pad*   pad(int i)const { return (i >= 0 && i < padCount()) ? &pads[i] : nullptr; }
};

// ──────────────────────────────────────────────────────────────────
// KitManager — charge kits.json, gère le kit courant
// ──────────────────────────────────────────────────────────────────
class KitManager {
public:
    // Charge depuis un fichier JSON.
    bool loadFromFile(const std::string& jsonPath);

    const Kit*              currentKit()  const;
    const std::vector<Kit>& kits()        const { return kits_; }
    int                     currentIndex()const { return currentIdx_; }

    // Retourne false si l'index est invalide.
    bool switchTo(int index);
    bool switchByName(const std::string& name);

    // Chemins absolus de tous les fichiers audio du kit courant (pour préchargement).
    std::vector<std::string> currentKitFilePaths() const;

private:
    std::vector<Kit> kits_;
    int              currentIdx_ = 0;
};

} // namespace wako::model
