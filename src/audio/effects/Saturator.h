#pragma once
#include <atomic>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Saturator — trois modes de saturation
//
// TUBE     : atan asymétrique, doux, style overdrive tube
// TRANSISTOR : hard clip asymétrique style TB-303 / 808
// FUZZ     : rectification + hard clip, style fuzz/big muff
// ──────────────────────────────────────────────────────────────────
class Saturator {
public:
    enum class Mode : int { Tube = 0, Transistor = 1, Fuzz = 2 };

    void setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool enabled()    const { return enabled_.load(std::memory_order_relaxed); }

    void  setDrive(float d);
    float getDrive() const { return drive_.load(std::memory_order_relaxed); }

    void  setMix(float m);
    float getMix()   const { return mix_.load(std::memory_order_relaxed); }

    void  setMode(Mode m)  { mode_.store(static_cast<int>(m), std::memory_order_relaxed); }
    Mode  getMode()  const { return static_cast<Mode>(mode_.load(std::memory_order_relaxed)); }

    void process(float* stereo, int frames) noexcept;

private:
    static float processSample(float x, float pregain, Mode mode) noexcept;

    std::atomic<bool>  enabled_{false};
    std::atomic<float> drive_  {0.f};
    std::atomic<float> mix_    {1.f};
    std::atomic<int>   mode_   {static_cast<int>(Mode::Transistor)};
};

} // namespace wako::audio