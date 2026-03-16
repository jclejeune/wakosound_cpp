#pragma once
#include "Pattern.h"
#include "../model/KitManager.h"
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>

namespace wako::seq {

// PlayMode retiré — gate géré par track dans Pattern::trackGate

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

private:
    void loop(std::shared_ptr<Pattern>           pattern,
              std::shared_ptr<model::KitManager> kit,
              StepCallback                       onStep);

    void playPads(const std::vector<int>&  activePads,
                  const TrackSteps&         currentSteps,
                  const model::KitManager& kit,
                  const Pattern&           pat);

    std::thread        thread_;
    std::atomic<bool>  running_  {false};
    std::unordered_map<int, int> activeVoices_;
};

} // namespace wako::seq