#pragma once
#include <cmath>

namespace wako::audio {

// ──────────────────────────────────────────────────────────────────
// Biquad — filtre IIR du second ordre (Direct Form II Transposed)
// Thread-safety : process() appelé depuis RT, set*() depuis UI.
// Les coefficients sont recalculés dans process() si dirty_.
// Une frame de latence de recalcul est inaudible.
// ──────────────────────────────────────────────────────────────────
struct Biquad {
    // Coefficients
    double b0 = 1, b1 = 0, b2 = 0;
    double a1 = 0, a2 = 0;
    // État
    double z1 = 0, z2 = 0;

    float process(float x) noexcept {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float>(y);
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    // ── Calcul des coefficients ───────────────────────────────────
    static constexpr double PI = 3.14159265358979323846;

    void setPeaking(double sr, double freq, double gainDb, double Q) noexcept {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * PI * freq / sr;
        double alpha = std::sin(w0) / (2.0 * Q);
        double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * std::cos(w0)) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * std::cos(w0)) / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    void setLowShelf(double sr, double freq, double gainDb) noexcept {
        double A  = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * PI * freq / sr;
        double cosW = std::cos(w0), sinW = std::sin(w0);
        double sqA  = std::sqrt(A);
        double alpha = sinW / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.707 - 1.0) + 2.0);
        double a0 = (A+1) + (A-1)*cosW + 2*sqA*alpha;
        b0 =  A * ((A+1) - (A-1)*cosW + 2*sqA*alpha) / a0;
        b1 =  2*A * ((A-1) - (A+1)*cosW) / a0;
        b2 =  A * ((A+1) - (A-1)*cosW - 2*sqA*alpha) / a0;
        a1 = -2.0 * ((A-1) + (A+1)*cosW) / a0;
        a2 = ((A+1) + (A-1)*cosW - 2*sqA*alpha) / a0;
    }

    void setHighShelf(double sr, double freq, double gainDb) noexcept {
        double A  = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * PI * freq / sr;
        double cosW = std::cos(w0), sinW = std::sin(w0);
        double sqA  = std::sqrt(A);
        double alpha = sinW / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.707 - 1.0) + 2.0);
        double a0 = (A+1) - (A-1)*cosW + 2*sqA*alpha;
        b0 =  A * ((A+1) + (A-1)*cosW + 2*sqA*alpha) / a0;
        b1 = -2*A * ((A-1) + (A+1)*cosW) / a0;
        b2 =  A * ((A+1) + (A-1)*cosW - 2*sqA*alpha) / a0;
        a1 =  2.0 * ((A-1) - (A+1)*cosW) / a0;
        a2 = ((A+1) - (A-1)*cosW - 2*sqA*alpha) / a0;
    }

    void setBypass() noexcept {
        b0 = 1; b1 = b2 = a1 = a2 = 0;
    }
};

} // namespace wako::audio