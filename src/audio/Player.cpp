#include "Player.h"
#include "AudioCache.h"
#include <sndfile.h>
#include <iostream>
#include <cstring>

namespace wako::audio {

Player& Player::instance() {
    static Player inst;
    return inst;
}

Player::~Player() {
    shutdown();
}

bool Player::init(int sampleRate, int framesPerBuffer) {
    sampleRate_ = sampleRate;

    for (auto& c : chains_)
        c.setSampleRate(sampleRate);

    voicePool_.setSampleRate(sampleRate);

    for (int i = 0; i < MAX_PADS_METER; ++i)
        voicePool_.setTrackChain(i, &chains_[i]);

    voicePool_.setMasterChain(&chains_[MASTER_CHAIN]);

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "[Player] Pa_Initialize: " << Pa_GetErrorText(err) << "\n";
        return false;
    }

    PaStreamParameters out{};
    out.device = Pa_GetDefaultOutputDevice();

    if (out.device == paNoDevice) {
        std::cerr << "[Player] Aucun device audio disponible\n";
        return false;
    }

    out.channelCount = 2;
    out.sampleFormat = paFloat32;
    out.suggestedLatency =
        Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;

    PaError openErr = Pa_OpenStream(
        &stream_, nullptr, &out,
        sampleRate, framesPerBuffer,
        paClipOff, &Player::paCallback, this
    );

    if (openErr != paNoError) {
        std::cerr << "[Player] Pa_OpenStream: " << Pa_GetErrorText(openErr) << "\n";
        Pa_Terminate();
        return false;
    }

    PaError startErr = Pa_StartStream(stream_);
    if (startErr != paNoError) {
        std::cerr << "[Player] Pa_StartStream: " << Pa_GetErrorText(startErr) << "\n";
        Pa_CloseStream(stream_);
        Pa_Terminate();
        stream_ = nullptr;
        return false;
    }

    std::cout << "[Player] Stream ouvert — "
              << sampleRate << " Hz, buffer "
              << framesPerBuffer << " frames\n";
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
                 int pitch, bool gate, int padIdx,
                 model::PlayMode mode) {
    const AudioBuffer* buf = AudioCache::instance().get(filePath);
    if (!buf) return -1;
    return voicePool_.play(buf, volume, gate, padIdx, pitch, mode);
}

void Player::stop(int voiceId)  { voicePool_.stop(voiceId); }
void Player::stopAll()          { voicePool_.stopAll(); }

void Player::startRecording(int totalFrames) {
    recordBuf_.assign(static_cast<size_t>(totalFrames * 2), 0.f);
    recordTotal_ = totalFrames;
    recordPos_.store(0, std::memory_order_release);
    recording_.store(true, std::memory_order_release);
}

bool Player::stopRecording(const std::string& outputPath) {
    recording_.store(false, std::memory_order_release);

    int framesRecorded = recordPos_.load(std::memory_order_acquire);
    if (framesRecorded <= 0) {
        std::cerr << "[Player] stopRecording: rien à écrire\n";
        return false;
    }

    SF_INFO info{};
    info.samplerate = sampleRate_;
    info.channels   = 2;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* sf = sf_open(outputPath.c_str(), SFM_WRITE, &info);
    if (!sf) {
        std::cerr << "[Player] stopRecording: " << sf_strerror(nullptr) << "\n";
        return false;
    }

    sf_count_t written = sf_writef_float(
        sf, recordBuf_.data(),
        static_cast<sf_count_t>(framesRecorded)
    );
    sf_close(sf);

    std::cout << "[Player] WAV écrit : "
              << framesRecorded << " frames → " << outputPath << "\n";
    return written > 0;
}

int Player::paCallback(const void*, void* output,
                       unsigned long frames,
                       const PaStreamCallbackTimeInfo*,
                       PaStreamCallbackFlags,
                       void* userData) {
    auto* self = static_cast<Player*>(userData);

    float masterVol = self->masterVolume_.load(std::memory_order_relaxed);
    self->voicePool_.mix(static_cast<float*>(output), frames, masterVol);

    float* out = static_cast<float*>(output);
    for (unsigned long i = 0; i < frames * 2; ++i) {
        if (out[i] >  1.0f) out[i] =  1.0f;
        else if (out[i] < -1.0f) out[i] = -1.0f;
    }

    if (self->recording_.load(std::memory_order_relaxed)) {
        int pos       = self->recordPos_.load(std::memory_order_relaxed);
        int remaining = self->recordTotal_ - pos;
        if (remaining > 0) {
            int toCopy = std::min<int>((int)frames, remaining);
            std::memcpy(&self->recordBuf_[(size_t)pos * 2], output,
                        (size_t)toCopy * 2 * sizeof(float));
            self->recordPos_.fetch_add(toCopy, std::memory_order_relaxed);
        }
    }

    return paContinue;
}

} // namespace wako::audio