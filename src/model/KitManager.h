#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace wako::model {

struct Pad {
    std::string name;
    std::string filePath;
    float       volume      = 1.0f;
    bool        enabled     = true;
    std::string description;
    std::string color;

    bool valid() const;
    std::string displayName() const;
};

struct Kit {
    static constexpr int MAX_PADS = 9;

    std::string      id;
    std::string      name;
    std::string      description;
    std::vector<Pad> pads;
    bool             isFactory = false;   // true = factory, jamais écrit

    bool       full()    const { return static_cast<int>(pads.size()) == MAX_PADS; }
    bool       empty()   const { return pads.empty(); }
    int        padCount()const { return static_cast<int>(pads.size()); }
    const Pad* pad(int i)const { return (i >= 0 && i < padCount()) ? &pads[i] : nullptr; }
};

// ──────────────────────────────────────────────────────────────────
// KitManager
//
// Deux sources distinctes :
//   loadFactory("kits.json")    → isFactory=true,  jamais réécrits
//   loadUser("user_kits.json")  → isFactory=false, silencieux si absent
//
// saveUserKits()  → écrit uniquement les kits utilisateur
// clearUserKits() → vide les kits utilisateur en mémoire
// ──────────────────────────────────────────────────────────────────
class KitManager {
public:
    // Charge les kits factory. Réinitialise la liste complète.
    bool loadFactory(const std::string& jsonPath);

    // Ajoute les kits utilisateur par-dessus. Silencieux si absent.
    void loadUser(const std::string& jsonPath);

    // ── Accès ─────────────────────────────────────────────────────
    const Kit*              currentKit()   const;
    Kit*                    currentKitMutable();
    const std::vector<Kit>& kits()         const { return kits_; }
    int                     currentIndex() const { return currentIdx_; }
    const std::string&      userPath()     const { return userPath_; }

    bool switchTo(int index);
    bool switchByName(const std::string& name);

    // ── Mutations utilisateur ─────────────────────────────────────

    // Assigne un fichier à un pad du kit courant.
    // Retourne false si le kit courant est factory.
    bool setPadFile(int padIdx,
                    const std::string& filePath,
                    const std::string& name = "");

    // Ajoute ou remplace un kit utilisateur (même nom → remplacement).
    // Ne touche jamais aux kits factory.
    // Retourne l'index résultant dans kits_.
    int upsertUserKit(Kit kit);

    // Supprime tous les kits !isFactory. Les factory restent intacts.
    // Si le kit courant était utilisateur, bascule sur le premier factory.
    void clearUserKits();

    // ── Persistance ───────────────────────────────────────────────
    // Écrit uniquement les kits !isFactory dans userPath_.
    bool saveUserKits() const;

    // ── Helpers ───────────────────────────────────────────────────
    std::vector<std::string> currentKitFilePaths() const;

private:
    std::vector<Kit>          kits_;
    int                       currentIdx_  = 0;
    std::string               userPath_;
    std::filesystem::path     factoryBase_;   // dossier du kits.json factory

    static std::string nameToId(const std::string& name);

    static std::vector<Kit> parseFile(const std::string& jsonPath, bool isFactory);

    bool writeKits(const std::string& path,
                   const std::vector<Kit>& kits,
                   const std::filesystem::path& base) const;
};

} // namespace wako::model