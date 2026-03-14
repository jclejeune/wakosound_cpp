#include "Engine.h"
#include "../audio/Player.h"
#include <chrono>
#include <thread>

using namespace std::chrono;

namespace wako::seq {

void Engine::start(std::shared_ptr<Pattern>          pattern,
                   std::shared_ptr<model::KitManager> kit,
                   StepCallback                       onStep,
                   PlayMode                           mode) {
    stop();
    running_.store(true);
    thread_ = std::thread([this, pattern, kit, onStep, mode] {
        loop(pattern, kit, onStep, mode);
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
// ──────────────────────────────────────────────────────────────────
void Engine::loop(std::shared_ptr<Pattern>          patternPtr,
                  std::shared_ptr<model::KitManager> kitPtr,
                  StepCallback                       onStep,
                  PlayMode                           mode)
{
    auto    startTime = steady_clock::now();
    int64_t stepCount = 0;
    int     lastBpm   = -1;          // ← tracker le BPM précédent

    while (running_.load()) {
        Pattern& pat        = *patternPtr;
        int      currentBpm = pat.bpm;

        // BPM changé à chaud → rebase startTime depuis maintenant
        // "fais comme si on avait toujours tourné à ce BPM"
        if (currentBpm != lastBpm) {
            auto intervalMs = milliseconds(Pattern::stepIntervalMs(currentBpm));
            startTime = steady_clock::now() - stepCount * intervalMs;
            lastBpm   = currentBpm;
        }

        auto intervalMs = milliseconds(Pattern::stepIntervalMs(currentBpm));
        auto target     = startTime + stepCount * intervalMs;
        auto sleepFor   = target - steady_clock::now();

        if (sleepFor > milliseconds(0))
            std::this_thread::sleep_for(sleepFor);

        if (!running_.load()) break;

        int step = pat.advance();
        playStep(pat, *kitPtr, step, mode);
        onStep(step);
        ++stepCount;
    }
}

// ──────────────────────────────────────────────────────────────────
// playStep — joue les pads actifs, gère le mode gate
// ──────────────────────────────────────────────────────────────────
void Engine::playStep(const Pattern& pat, const model::KitManager& kit,
                      int step, PlayMode mode) {
    auto& player = audio::Player::instance();
    const auto* currentKit = kit.currentKit();
    if (!currentKit) return;

    // Mode gate : stopper les voix du step précédent
    if (mode == PlayMode::Gate) {
        for (auto& [padIdx, voiceId] : activeVoices_)
            player.stop(voiceId);
        activeVoices_.clear();
    }

    for (int padIdx : pat.activeAtStep(step)) {
        const model::Pad* pad = currentKit->pad(padIdx);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;

        bool gateMode = (mode == PlayMode::Gate);
        int voiceId   = player.play(pad->filePath, pad->volume, gateMode);

        if (gateMode && voiceId >= 0)
            activeVoices_[padIdx] = voiceId;
    }
}

} // namespace wako::seq
