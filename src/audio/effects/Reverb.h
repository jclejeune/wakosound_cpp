#pragma once
#include <atomic>
#include <vector>
#include <array>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Reverb — algorithme Freeverb simplifié
// 4 filtres comb parallèles + 2 filtres allpass en série par canal
// ──────────────────────────────────────────────────────────────────
class Reverb {
public:
    Reverb();

    void setSampleRate(int sr);

    void setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool enabled()    const { return enabled_.load(std::memory_order_relaxed); }

    // roomSize : 0.0 → 1.0
    void  setRoomSize(float r);
    float getRoomSize() const { return roomSize_.load(std::memory_order_relaxed); }

    // damping : 0.0 (brillant) → 1.0 (sombre)
    void  setDamping(float d);
    float getDamping()  const { return damping_.load(std::memory_order_relaxed); }

    // wet : 0.0 → 1.0
    void  setWet(float w);
    float getWet()      const { return wet_.load(std::memory_order_relaxed); }

    void process(float* stereo, int frames) noexcept;
    void reset();

private:
    // Comb filter with damping (Schroeder)
    struct Comb {
        std::vector<float> buf;
        int   pos    = 0;
        float filterstore = 0.f;

        void init(int size) { buf.assign(size, 0.f); pos = 0; filterstore = 0.f; }

        float process(float input, float feedback, float damp) noexcept {
            float output  = buf[pos];
            filterstore   = output * (1.f - damp) + filterstore * damp;
            buf[pos]      = input + filterstore * feedback;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            return output;
        }
    };

    // Allpass filter
    struct Allpass {
        std::vector<float> buf;
        int pos = 0;

        void init(int size) { buf.assign(size, 0.f); pos = 0; }

        float process(float input) noexcept {
            float bufout = buf[pos];
            buf[pos]     = input + bufout * 0.5f;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            return bufout - input;
        }
    };

    static constexpr int NUM_COMBS    = 4;
    static constexpr int NUM_ALLPASSES= 2;

    // Tailles de base (à 44100 Hz)
    static constexpr int COMB_SIZES[NUM_COMBS]     = {1116, 1188, 1277, 1356};
    static constexpr int ALLPASS_SIZES[NUM_ALLPASSES] = {556, 441};
    // Offset stéréo (samples)
    static constexpr int STEREO_SPREAD = 23;

    std::array<Comb,    NUM_COMBS>     combL_{}, combR_{};
    std::array<Allpass, NUM_ALLPASSES> apL_{},   apR_{};

    std::atomic<bool>  enabled_ {false};
    std::atomic<float> roomSize_{0.5f};
    std::atomic<float> damping_ {0.5f};
    std::atomic<float> wet_     {0.3f};

    int sampleRate_ = 44100;
};

} // namespace wako::audio