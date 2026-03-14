#pragma once
#include "VoicePool.h"
#include <portaudio.h>
#include <string>
#include <functional>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Player — wraps PortAudio + VoicePool
//
// Singleton. Ouvrir le stream une seule fois au démarrage.
// Toutes les lectures passent par le même callback.
// ──────────────────────────────────────────────────────────────────
class Player {
public:
    static Player& instance();
    ~Player();

    // Initialise PortAudio et ouvre le stream. Retourne false si erreur.
    bool init(int sampleRate = 44100, int framesPerBuffer = 256);
    void shutdown();

    // Joue un fichier audio. Retourne un voice-id (pour stop en mode gate).
    int  play(const std::string& filePath, float volume = 1.0f, bool gate = false);

    // Stoppe une voix (mode gate).
    void stop(int voiceId);
    void stopAll();

    bool isRunning() const { return stream_ != nullptr; }

private:
    Player() = default;
    Player(const Player&) = delete;

    static int paCallback(const void*, void* out, unsigned long frames,
                          const PaStreamCallbackTimeInfo*,
                          PaStreamCallbackFlags, void* userData);

    PaStream*  stream_     = nullptr;
    VoicePool  voicePool_;
    int        sampleRate_ = 44100;
};

} // namespace wako::audio
