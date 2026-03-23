#pragma once
#include "AudioCache.h"
#include <string>
#include <map>
#include <mutex>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// PitchCache — pitch shifting offline via signalsmith-stretch
//
// Licence MIT — aucune contrainte commerciale.
// Chaque combinaison (filePath, semitones) est traitée une seule fois
// et mise en cache. Les appels suivants sont instantanés.
//
// pitch == 0 → renvoi direct vers AudioCache (pas de traitement)
// pitch != 0 → signalsmith offline, durée préservée
// ──────────────────────────────────────────────────────────────────
class PitchCache {
public:
    static PitchCache& instance();

    // Retourne un buffer pitch-shifté. Thread-safe.
    // Retourne nullptr si le fichier est invalide.
    const AudioBuffer* get(const std::string& filePath, int semitones);

    void clear();

private:
    PitchCache() = default;

    AudioBuffer process(const AudioBuffer& input, int semitones);

    using Key = std::pair<std::string, int>;
    mutable std::mutex         mutex_;
    std::map<Key, AudioBuffer> cache_;
};

} // namespace wako::audio