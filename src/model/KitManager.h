#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace wako::model {

// ──────────────────────────────────────────────────────────────────
// PlayMode — mode de lecture du sample
// ──────────────────────────────────────────────────────────────────
enum class PlayMode {
    Once        = 0,  // ▶  joue jusqu'au bout et meurt
    Loop        = 1,  // ↺  boucle infinie
    Reverse     = 2,  // ◀  lecture à l'envers, meurt à 0
    LoopReverse = 3,  // ↺◀ boucle à l'envers
};

inline const char* playModeStr(PlayMode m) {
    switch (m) {
        case PlayMode::Once:        return "once";
        case PlayMode::Loop:        return "loop";
        case PlayMode::Reverse:     return "reverse";
        case PlayMode::LoopReverse: return "loop_reverse";
    }
    return "once";
}

inline PlayMode playModeFromStr(const std::string& s) {
    if (s == "loop")         return PlayMode::Loop;
    if (s == "reverse")      return PlayMode::Reverse;
    if (s == "loop_reverse") return PlayMode::LoopReverse;
    return PlayMode::Once;
}

struct Pad {
    std::string name;
    std::string filePath;
    float       volume      = 1.0f;
    bool        enabled     = true;
    PlayMode    mode        = PlayMode::Once;
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
    bool             isFactory = false;

    bool       full()    const { return static_cast<int>(pads.size()) == MAX_PADS; }
    bool       empty()   const { return pads.empty(); }
    int        padCount()const { return static_cast<int>(pads.size()); }
    const Pad* pad(int i)const { return (i >= 0 && i < padCount()) ? &pads[i] : nullptr; }
};

// ──────────────────────────────────────────────────────────────────
// KitManager
// ──────────────────────────────────────────────────────────────────
class KitManager {
public:
    bool loadFactory(const std::string& jsonPath);
    void loadUser(const std::string& jsonPath);

    const Kit*              currentKit()   const;
    Kit*                    currentKitMutable();
    const std::vector<Kit>& kits()         const { return kits_; }
    int                     currentIndex() const { return currentIdx_; }
    const std::string&      userPath()     const { return userPath_; }

    bool switchTo(int index);
    bool switchByName(const std::string& name);

    bool setPadFile(int padIdx,
                    const std::string& filePath,
                    const std::string& name = "");

    bool setPadMode(int padIdx, PlayMode mode);

    int  upsertUserKit(Kit kit);
    void clearUserKits();
    bool saveUserKits() const;

    std::vector<std::string> currentKitFilePaths() const;

private:
    std::vector<Kit>          kits_;
    int                       currentIdx_  = 0;
    std::string               userPath_;
    std::filesystem::path     factoryBase_;

    static std::string nameToId(const std::string& name);
    static std::vector<Kit> parseFile(const std::string& jsonPath, bool isFactory);
    bool writeKits(const std::string& path,
                   const std::vector<Kit>& kits,
                   const std::filesystem::path& base) const;
};

} // namespace wako::model