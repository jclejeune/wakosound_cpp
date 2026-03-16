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
// sleepInterruptible
//
// Deux régimes selon la durée :
//
//  < FREQ_THRESHOLD_MS  → sleep direct
//    Le step est si court qu'un chunk serait plus long que le step lui-même.
//    On est en mode "générateur de fréquence" — pas d'interruption,
//    stop() peut attendre quelques ms, acceptable à ce régime.
//
//  >= FREQ_THRESHOLD_MS → chunks de 50ms
//    Mode séquenceur normal. stop() répond en max 50ms
//    quelle que soit la lenteur du tempo (1 BPM = 15s/step).
// ──────────────────────────────────────────────────────────────────

static constexpr int FREQ_THRESHOLD_MS = 5;   // ~3000 BPM
static constexpr int CHUNK_MS          = 50;

static void sleepInterruptible(milliseconds sleepFor,
                                const std::atomic<bool>& running) {
    if (sleepFor <= milliseconds(0)) return;

    if (sleepFor.count() < FREQ_THRESHOLD_MS) {
        // Mode fréquence — sleep direct, pas d'interruption
        std::this_thread::sleep_for(sleepFor);
    } else {
        // Mode séquenceur — chunks interruptibles
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

// ──────────────────────────────────────────────────────────────────
// Boucle principale — horloge absolue drift-free
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
        auto sleepFor = duration_cast<milliseconds>(target - steady_clock::now());

        sleepInterruptible(sleepFor, running_);

        if (!running_.load()) break;

        PlayMode currentMode = static_cast<PlayMode>(
            playMode_.load(std::memory_order_relaxed));

        TrackSteps currentSteps = pat.trackSteps;

        auto activePads = pat.advance();
        playPads(activePads, currentSteps, *kitPtr, currentMode, pat);
        onStep(currentSteps);

        ++tickCount;
    }
}

// ─────────────────────────────────────────────────────────────────
// playPads
// ─────────────────────────────────────────────────────────────────

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

        const StepData& sd       = pat.getStepData(padIdx, currentSteps[padIdx]);
        float           stepVol  = pad->volume * sd.volume;
        int             stepPitch = sd.pitch;

        bool gateMode = (mode == PlayMode::Gate);
        int  voiceId  = player.play(pad->filePath, stepVol, stepPitch, gateMode);

        if (gateMode && voiceId >= 0)
            activeVoices_[padIdx] = voiceId;
    }
}

} // namespace wako::seq