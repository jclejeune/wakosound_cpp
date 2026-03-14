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
    void trackLengthChanged(int pad, int length);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {800, 300}; }

private:
    static constexpr int LABEL_W  = 72;
    static constexpr int HEADER_H = 22;

    // Positions entières — calculées depuis la largeur totale, pas d'accumulation
    int stepX0(int s) const;   // bord gauche du step s
    int stepX1(int s) const;   // bord droit du step s
    int padY0(int p)  const;   // bord haut du pad p
    int padY1(int p)  const;   // bord bas du pad p

    std::shared_ptr<seq::Pattern> pattern_;
    seq::TrackSteps               currentSteps_;
    std::vector<std::string>      padNames_;
};

} // namespace wako::ui