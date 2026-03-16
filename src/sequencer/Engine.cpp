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

static constexpr int FREQ_THRESHOLD_MS = 5;
static constexpr int CHUNK_MS          = 50;

static void sleepInterruptible(milliseconds sleepFor,
                                const std::atomic<bool>& running) {
    if (sleepFor <= milliseconds(0)) return;
    if (sleepFor.count() < FREQ_THRESHOLD_MS) {
        std::this_thread::sleep_for(sleepFor);
    } else {
        auto sleepEnd = steady_clock::now() + sleepFor;
        while (running.load(std::memory_order_relaxed)) {
            auto remaining = duration_cast<milliseconds>(
                                 sleepEnd - steady_clock::now());
            if (remaining <= milliseconds(0)) break;
            std::this_thread::sleep_for(
                std::min(remaining, milliseconds(CHUNK_MS)));
        }
    }
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
        auto sleepFor = duration_cast<milliseconds>(
                            target - steady_clock::now());

        sleepInterruptible(sleepFor, running_);

        if (!running_.load()) break;

        TrackSteps currentSteps = pat.trackSteps;
        auto activePads = pat.advance();
        playPads(activePads, currentSteps, *kitPtr, pat);
        onStep(currentSteps);

        ++tickCount;
    }
}

void Engine::playPads(const std::vector<int>&  activePads,
                      const TrackSteps&         currentSteps,
                      const model::KitManager& kit,
                      const Pattern&           pat)
{
    auto& player = audio::Player::instance();
    const auto* currentKit = kit.currentKit();
    if (!currentKit) return;

    // Stopper les voix gate du tick précédent pour les tracks en gate
    for (auto& [padIdx, voiceId] : activeVoices_) {
        if (pat.trackGate[padIdx])
            player.stop(voiceId);
    }
    // Nettoyer les voix des tracks en gate
    for (int i = 0; i < seq::MAX_PADS; ++i)
        if (pat.trackGate[i])
            activeVoices_.erase(i);

    for (int padIdx : activePads) {
        // Mute / Solo
        if (!pat.shouldPlay(padIdx)) continue;

        const model::Pad* pad = currentKit->pad(padIdx);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;

        const StepData& sd        = pat.getStepData(padIdx, currentSteps[padIdx]);
        float           stepVol   = pad->volume * sd.volume;
        int             stepPitch = sd.pitch;

        // Gate : track gate OU step gate local
        bool useGate = pat.trackGate[padIdx] || sd.gate;

        int voiceId = player.play(pad->filePath, stepVol, stepPitch, useGate);

        if (useGate && voiceId >= 0)
            activeVoices_[padIdx] = voiceId;
    }
}

} // namespace wako::seq