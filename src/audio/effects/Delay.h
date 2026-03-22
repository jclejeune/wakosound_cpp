#pragma once
#include <atomic>
#include <vector>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Delay — délai stéréo avec feedback et mix wet/dry
// Délai max 2 s. Temps ajustable en temps réel (interpolation linéaire).
// ──────────────────────────────────────────────────────────────────
class Delay {
public:
    static constexpr float MAX_DELAY_S = 2.0f;

    Delay();
    void setSampleRate(int sr);

    void setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool enabled()    const { return enabled_.load(std::memory_order_relaxed); }

    // timeMs : 0 → 2000 ms
    void  setTimeMs(float ms);
    float getTimeMs() const { return timeMs_.load(std::memory_order_relaxed); }

    // feedback : 0.0 → 0.95
    void  setFeedback(float f);
    float getFeedback() const { return feedback_.load(std::memory_order_relaxed); }

    // mix : 0.0 (sec) → 1.0 (tout wet)
    void  setMix(float m);
    float getMix()      const { return mix_.load(std::memory_order_relaxed); }

    void process(float* stereo, int frames) noexcept;
    void reset();

private:
    std::vector<float> bufL_, bufR_;
    int   writePos_   = 0;
    int   maxDelay_   = 0;
    int   sampleRate_ = 44100;

    std::atomic<bool>  enabled_  {false};
    std::atomic<float> timeMs_   {250.f};
    std::atomic<float> feedback_ {0.4f};
    std::atomic<float> mix_      {0.4f};
};

} // namespace wako::audio