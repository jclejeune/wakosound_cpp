#pragma once
#include "VoicePool.h"
#include <portaudio.h>
#include <string>

namespace wako::audio {

class Player {
public:
    static Player& instance();
    ~Player();

    bool init(int sampleRate = 44100, int framesPerBuffer = 256);
    void shutdown();

    // pitch : semitones -12 → +12 (0 = pas de traitement)
    int  play(const std::string& filePath,
              float volume = 1.0f,
              int   pitch  = 0,
              bool  gate   = false);

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