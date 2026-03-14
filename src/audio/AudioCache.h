#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// AudioBuffer — données audio d'un sample, chargées UNE SEULE FOIS
// Stockées en float interleaved stereo (L R L R …)
// Pas de copie après le chargement initial.
// ──────────────────────────────────────────────────────────────────
struct AudioBuffer {
    std::vector<float> samples;  // interleaved stereo float32
    int                channels  = 2;
    int                sampleRate = 44100;
    int64_t            frameCount = 0;    // samples.size() / channels

    bool empty() const { return samples.empty(); }
};

// ──────────────────────────────────────────────────────────────────
// AudioCache — singleton thread-safe
// get() retourne un pointeur stable vers le buffer en cache.
// Le pointeur reste valide pendant toute la durée de vie du cache.
// ──────────────────────────────────────────────────────────────────
class AudioCache {
public:
    static AudioCache& instance();

    // Charge ou retourne depuis le cache. Thread-safe.
    // Retourne nullptr si le fichier est invalide.
    const AudioBuffer* get(const std::string& filePath);

    // Précharge un ensemble de fichiers (appel au démarrage).
    void preload(const std::vector<std::string>& paths);

    void clear();
    std::size_t size() const;

private:
    AudioCache() = default;
    std::optional<AudioBuffer> loadFile(const std::string& path);

    mutable std::mutex                              mutex_;
    std::unordered_map<std::string, AudioBuffer>   cache_;
};

} // namespace wako::audio
