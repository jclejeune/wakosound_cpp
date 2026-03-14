#include "Player.h"
#include "AudioCache.h"
#include <iostream>

namespace wako::audio {

Player& Player::instance() {
    static Player inst;
    return inst;
}

Player::~Player() { shutdown(); }

bool Player::init(int sampleRate, int framesPerBuffer) {
    sampleRate_ = sampleRate;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "[Player] Pa_Initialize: " << Pa_GetErrorText(err) << "\n";
        return false;
    }

    PaStreamParameters out{};
    out.device                    = Pa_GetDefaultOutputDevice();
    out.channelCount              = 2;
    out.sampleFormat              = paFloat32;
    out.suggestedLatency          = Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;
    out.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&stream_, nullptr, &out,
                        sampleRate, framesPerBuffer,
                        paClipOff, &Player::paCallback, this);
    if (err != paNoError) {
        std::cerr << "[Player] Pa_OpenStream: " << Pa_GetErrorText(err) << "\n";
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "[Player] Pa_StartStream: " << Pa_GetErrorText(err) << "\n";
        Pa_CloseStream(stream_);
        Pa_Terminate();
        stream_ = nullptr;
        return false;
    }

    std::cout << "[Player] Stream ouvert — "
              << sampleRate << " Hz, buffer " << framesPerBuffer << " frames\n";
    return true;
}

void Player::shutdown() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        Pa_Terminate();
    }
}

// ── API publique ──────────────────────────────────────────────────

int Player::play(const std::string& filePath, float volume, bool gate) {
    const AudioBuffer* buf = AudioCache::instance().get(filePath);
    if (!buf) return -1;
    return voicePool_.play(buf, volume, gate);
}

void Player::stop(int voiceId) {
    voicePool_.stop(voiceId);
}

void Player::stopAll() {
    voicePool_.stopAll();
}

// ── Callback PortAudio (RT thread) ───────────────────────────────

int Player::paCallback(const void*, void* output, unsigned long frames,
                       const PaStreamCallbackTimeInfo*,
                       PaStreamCallbackFlags, void* userData) {
    auto* self = static_cast<Player*>(userData);
    self->voicePool_.mix(static_cast<float*>(output), frames);
    return paContinue;
}

} // namespace wako::audio
