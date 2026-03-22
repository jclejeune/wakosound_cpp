#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <memory>
#include <array>

namespace wako::ui {

class EffectWindow;

// ── VuMeter ───────────────────────────────────────────────────────
class VuMeter : public QWidget {
    Q_OBJECT
public:
    explicit VuMeter(QWidget* parent = nullptr);
    void setLevel(float level);
    void setPeakHold(float peak);

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {18, 120}; }

private:
    float level_    = 0.f;
    float peakHold_ = 0.f;
};

// ── MixerPanel ────────────────────────────────────────────────────
class MixerPanel : public QWidget {
    Q_OBJECT
public:
    explicit MixerPanel(std::shared_ptr<seq::Pattern> pattern,
                        QWidget* parent = nullptr);
    ~MixerPanel();

    void setKit(const model::Kit* kit);
    void resetAll();

signals:
    void trackVolumeChanged(int pad, float volume);
    void masterVolumeChanged(float volume);
    void trackMuteToggled(int pad);
    void trackSoloToggled(int pad);

private slots:
    void onTimer();

private:
    void buildStrip(int pad, QWidget* container);
    void buildMasterStrip(QWidget* container);
    void updateMuteSoloButtons();

    std::shared_ptr<seq::Pattern> pattern_;

    std::array<VuMeter*,     seq::MAX_PADS> trackVu_{};
    std::array<QWidget*,     seq::MAX_PADS> trackLabels_{};
    std::array<QPushButton*, seq::MAX_PADS> muteButtons_{};
    std::array<QPushButton*, seq::MAX_PADS> soloButtons_{};
    std::array<QPushButton*, seq::MAX_PADS> fxButtons_{};
    std::array<EffectWindow*, seq::MAX_PADS + 1> fxWindows_{};  // +1 master

    VuMeter* masterVuL_ = nullptr;
    VuMeter* masterVuR_ = nullptr;
    QPushButton* masterFxBtn_ = nullptr;

    QTimer* timer_ = nullptr;

    static constexpr int TIMER_MS = 33;
};

} // namespace wako::ui