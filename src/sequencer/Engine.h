#pragma once
#include "Pattern.h"
#include "../model/KitManager.h"
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>

namespace wako::seq {

enum class PlayMode { OneShot, Gate };

class Engine {
public:
    using StepCallback = std::function<void(const TrackSteps&)>;

    Engine() = default;
    ~Engine() { stop(); }

    void start(std::shared_ptr<Pattern>           pattern,
               std::shared_ptr<model::KitManager> kit,
               StepCallback                       onStep);

    void stop();
    bool isRunning() const { return running_.load(); }

    // Changement en realtime — effet au tick suivant
    void setPlayMode(PlayMode mode) {
        playMode_.store(static_cast<int>(mode), std::memory_order_relaxed);
    }

private:
    void loop(std::shared_ptr<Pattern>           pattern,
              std::shared_ptr<model::KitManager> kit,
              StepCallback                       onStep);

    void playPads(const std::vector<int>&  activePads,
                  const model::KitManager& kit,
                  PlayMode                 mode);

    std::thread        thread_;
    std::atomic<bool>  running_  {false};
    std::atomic<int>   playMode_ {0};      // 0=OneShot, 1=Gate
    std::unordered_map<int, int> activeVoices_;
};

} // namespace wako::seq