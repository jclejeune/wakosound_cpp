#include "Player.h"
#include "AudioCache.h"
#include "PitchCache.h"
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

int Player::play(const std::string& filePath, float volume,
                 int pitch, bool gate, int padIdx) {
    const AudioBuffer* buf = (pitch == 0)
        ? AudioCache::instance().get(filePath)
        : PitchCache::instance().get(filePath, pitch);
    if (!buf) return -1;
    return voicePool_.play(buf, volume, gate, padIdx);
}

void Player::stop(int voiceId)  { voicePool_.stop(voiceId); }
void Player::stopAll()          { voicePool_.stopAll(); }

int Player::paCallback(const void*, void* output, unsigned long frames,
                       const PaStreamCallbackTimeInfo*,
                       PaStreamCallbackFlags, void* userData) {
    auto* self = static_cast<Player*>(userData);
    float mv   = self->masterVolume_.load(std::memory_order_relaxed);
    self->voicePool_.mix(static_cast<float*>(output), frames, mv);
    return paContinue;
}

} // namespace wako::audio