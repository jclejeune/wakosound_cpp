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

// ──────────────────────────────────────────────────────────────────
// Boucle principale — horloge absolue drift-free
// BPM et PlayMode lus à chaque tick → changement en realtime
// ──────────────────────────────────────────────────────────────────
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

        // Rebase horloge si BPM changé à chaud
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

        // Lire le mode à chaque tick — même pattern que le BPM
        PlayMode currentMode = static_cast<PlayMode>(
            playMode_.load(std::memory_order_relaxed));

        // Snapshot steps courants AVANT d'avancer
        TrackSteps currentSteps = pat.trackSteps;

        auto activePads = pat.advance();
        playPads(activePads, *kitPtr, currentMode);
        onStep(currentSteps);

        ++tickCount;
    }
}

// ──────────────────────────────────────────────────────────────────
// playPads
// ──────────────────────────────────────────────────────────────────
void Engine::playPads(const std::vector<int>&  activePads,
                      const model::KitManager& kit,
                      PlayMode                 mode)
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

        bool gateMode = (mode == PlayMode::Gate);
        int  voiceId  = player.play(pad->filePath, pad->volume, gateMode);

        if (gateMode && voiceId >= 0)
            activeVoices_[padIdx] = voiceId;
    }
}

} // namespace wako::seq