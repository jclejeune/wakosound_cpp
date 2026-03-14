#pragma once
#include "AudioCache.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Voice — une voix en cours de lecture
//
// accédée depuis le callback PortAudio (RT thread) ET depuis
// le thread principal (play/stop).
// → position est atomique pour éviter les data races.
// ──────────────────────────────────────────────────────────────────
struct Voice {
    const AudioBuffer*         buffer   = nullptr;   // non-owning
    std::atomic<int64_t>       position {-1};        // -1 = inactive
    float                      volume   = 1.0f;
    bool                       gate     = false;     // stoppe au release
    std::atomic<bool>          stopping {false};     // demande de stop (gate)
    int                        id       = -1;        // identifiant unique
};

// ──────────────────────────────────────────────────────────────────
// VoicePool — 16 voix fixes, voice stealing si toutes occupées
//
// Thread-safety :
//   - play()  : appelé depuis le thread principal
//   - stop()  : appelé depuis le thread principal
//   - mix()   : appelé depuis le callback PortAudio (RT)
//   Les atomics garantissent la visibilité entre threads.
// ──────────────────────────────────────────────────────────────────
class VoicePool {
public:
    static constexpr int MAX_VOICES = 16;

    // Démarre une voix. Retourne son id (pour stop ultérieur).
    // Voice stealing si toutes les voix sont actives.
    int play(const AudioBuffer* buffer, float volume = 1.0f, bool gate = false);

    // Stoppe une voix par id (mode gate).
    void stop(int voiceId);

    // Stoppe toutes les voix.
    void stopAll();

    // Mix dans outBuffer (stereo interleaved float).
    // Appelé depuis le callback PortAudio : AUCUNE allocation.
    void mix(float* outBuffer, unsigned long frameCount);

private:
    std::array<Voice, MAX_VOICES> voices_;
    std::atomic<int>              nextId_{0};

    int findFreeVoice();
    int stealOldestVoice();
};

} // namespace wako::audio
