#include "Engine.h"
#include "../audio/Player.h"
#include <chrono>
#include <thread>

using namespace std::chrono;

namespace wako::seq {

void Engine::start(std::shared_ptr<Pattern>           pattern,
                   std::shared_ptr<model::KitManager> kit,
                   StepCallback                       onStep) {
    stop();
    running_.store(true);
    thread_ = std::thread([this, pattern, kit, onStep] {
        loop(pattern, kit, onStep);
    });
}

void Engine::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    audio::Player::instance().stopAll();
    activeVoices_.clear();
}

void Engine::loop(std::shared_ptr<Pattern>           patternPtr,
                  std::shared_ptr<model::KitManager> kitPtr,
                  StepCallback                       onStep)
{
    auto    startTime = steady_clock::now();
    int64_t tickCount = 0;
    int     lastBpm   = -1;

    while (running_.load()) {
        Pattern& pat        = *patternPtr;
        int      currentBpm = pat.bpm;

        if (currentBpm != lastBpm) {
            auto interval = milliseconds(Pattern::stepIntervalMs(currentBpm));
            startTime = steady_clock::now() - tickCount * interval;
            lastBpm   = currentBpm;
        }

        auto interval = milliseconds(Pattern::stepIntervalMs(currentBpm));
        auto target   = startTime + tickCount * interval;
        auto sleepFor = target - steady_clock::now();

        if (sleepFor > milliseconds(0))
            std::this_thread::sleep_for(sleepFor);

        if (!running_.load()) break;

        PlayMode currentMode = static_cast<PlayMode>(
            playMode_.load(std::memory_order_relaxed));

        // Snapshot des steps courants AVANT d'avancer
        TrackSteps currentSteps = pat.trackSteps;

        auto activePads = pat.advance();
        playPads(activePads, currentSteps, *kitPtr, currentMode, pat);
        onStep(currentSteps);

        ++tickCount;
    }
}

void Engine::playPads(const std::vector<int>&  activePads,
                      const TrackSteps&         currentSteps,
                      const model::KitManager& kit,
                      PlayMode                 mode,
                      const Pattern&           pat)
{
    auto& player = audio::Player::instance();
    const auto* currentKit = kit.currentKit();
    if (!currentKit) return;

    if (mode == PlayMode::Gate) {
        for (auto& [padIdx, voiceId] : activeVoices_)
            player.stop(voiceId);
        activeVoices_.clear();
    }

    for (int padIdx : activePads) {
        const model::Pad* pad = currentKit->pad(padIdx);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;

        // Lire volume et pitch depuis le StepData
        const StepData& sd = pat.getStepData(padIdx, currentSteps[padIdx]);
        float stepVolume = pad->volume * sd.volume;  // volume pad × volume step
        int   stepPitch  = sd.pitch;

        bool gateMode = (mode == PlayMode::Gate);
        int  voiceId  = player.play(pad->filePath, stepVolume, stepPitch, gateMode);

        if (gateMode && voiceId >= 0)
            activeVoices_[padIdx] = voiceId;
    }
}

} // namespace wako::seq