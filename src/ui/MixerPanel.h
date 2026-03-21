#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include <QWidget>
#include <QTimer>
#include <memory>

namespace wako::ui {

// ── VuMeter ───────────────────────────────────────────────────────
// Widget vertical : segments colorés + peak hold + décroissance
class VuMeter : public QWidget {
    Q_OBJECT
public:
    explicit VuMeter(QWidget* parent = nullptr);
    void setLevel(float level);   // 0.0–1.0, appelé depuis QTimer
    void setPeakHold(float peak);

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {18, 120}; }

private:
    float level_    = 0.f;
    float peakHold_ = 0.f;
};

// ── MixerPanel ────────────────────────────────────────────────────
// Onglet mixage : 9 strips track + 1 strip master
// Chaque strip : label | VuMeter | QSlider vertical
class MixerPanel : public QWidget {
    Q_OBJECT
public:
    explicit MixerPanel(std::shared_ptr<seq::Pattern> pattern,
                        QWidget* parent = nullptr);

    // Mise à jour des labels (appelé au changement de kit)
    void setKit(const model::Kit* kit);

    // Reset tous les faders à 1.0
    void resetAll();

signals:
    void trackVolumeChanged(int pad, float volume);
    void masterVolumeChanged(float volume);

private:
    void onTimer();
    void buildStrip(int pad, QWidget* container);
    void buildMasterStrip(QWidget* container);

    std::shared_ptr<seq::Pattern> pattern_;

    // Par track
    std::array<VuMeter*, seq::MAX_PADS> trackVu_{};
    std::array<QWidget*, seq::MAX_PADS> trackLabels_{};   // QLabel*

    // Master
    VuMeter* masterVuL_ = nullptr;
    VuMeter* masterVuR_ = nullptr;

    QTimer* timer_ = nullptr;

    static constexpr int TIMER_MS = 33;   // ~30 fps
};

} // namespace wako::ui