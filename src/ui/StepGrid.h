#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include <QWidget>
#include <memory>

namespace wako::ui {

class StepGrid : public QWidget {
    Q_OBJECT
public:
    explicit StepGrid(std::shared_ptr<seq::Pattern> pattern,
                      QWidget* parent = nullptr);

    void setCurrentSteps(const seq::TrackSteps& steps);
    void setCurrentStep(int step);
    void updatePattern(const seq::Pattern* pat);
    void setKit(const model::Kit* kit);

signals:
    void stepToggled(int pad, int step);
    void stepGateToggled(int pad, int step);
    void trackLengthChanged(int pad, int length);
    void trackMuteToggled(int pad);
    void trackSoloToggled(int pad);
    void stepDataChanged(int pad, int step, seq::StepData data);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {800, 300}; }

private:
    static constexpr int LABEL_W  = 65;
    static constexpr int BTN_W    = 18;
    static constexpr int TOTAL_LW = LABEL_W + BTN_W * 2;
    static constexpr int HEADER_H = 22;
    static constexpr int DRAG_THRESHOLD = 5;
    static constexpr int PITCH_PX       = 18;
    static constexpr float VOL_PX       = 200.0f;

    int stepX0(int s) const;
    int stepX1(int s) const;
    int padY0(int p)  const;
    int padY1(int p)  const;
    int padAtY(int y) const;
    int stepAtX(int x) const;  // >=0=step, -2=M, -3=S, -1=ailleurs

    QColor stepColor(int pitch, float volume, bool active) const;
    QColor textColor(const QColor& bg) const;

    enum class Axis { None, Horizontal, Vertical };
    struct DragState {
        bool  active    = false;
        int   pad       = -1;
        int   step      = -1;
        int   startX    = 0;
        int   startY    = 0;
        float origVol   = 1.0f;
        int   origPitch = 0;
        bool  hasMoved  = false;
        Axis  axis      = Axis::None;
    };

    // Drag droit séparé — longueur de track
    struct RightDragState {
        bool active    = false;
        int  pad       = -1;
        int  step      = -1;
        int  startX    = 0;
        bool hasMoved  = false;
    };

    std::shared_ptr<seq::Pattern> pattern_;
    seq::TrackSteps               currentSteps_;
    std::vector<std::string>      padNames_;
    DragState                     drag_;
    RightDragState                rdrag_;
};

} // namespace wako::ui