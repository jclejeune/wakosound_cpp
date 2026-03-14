#pragma once
#include "Pattern.h"
#include "../model/KitManager.h"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace wako::seq {

enum class PlayMode { OneShot, Gate };

// ──────────────────────────────────────────────────────────────────
// Engine — boucle de séquençage sur un thread dédié
//
// Horloge absolue (drift-free) : même technique que le code Clojure
// mais sans core.async. On calcule le timestamp cible de chaque step
// et on dort exactement le bon nombre de ms.
// ──────────────────────────────────────────────────────────────────
class Engine {
public:
    using StepCallback = std::function<void(int step)>;

    Engine() = default;
    ~Engine() { stop(); }

    // Démarre le séquençage. onStep est appelé dans le thread RT du séquenceur.
    void start(std::shared_ptr<Pattern>          pattern,
               std::shared_ptr<model::KitManager> kit,
               StepCallback                       onStep,
               PlayMode                           mode = PlayMode::OneShot);

    void stop();
    bool isRunning() const { return running_.load(); }

private:
    void loop(std::shared_ptr<Pattern>          pattern,
              std::shared_ptr<model::KitManager> kit,
              StepCallback                       onStep,
              PlayMode                           mode);

    void playStep(const Pattern& pat, const model::KitManager& kit,
                  int step, PlayMode mode);

    std::thread        thread_;
    std::atomic<bool>  running_{false};
    // voiceIds actives (pour gate : arrêt au step suivant)
    std::unordered_map<int, int> activeVoices_;  // padIdx → voiceId
};

} // namespace wako::seq
