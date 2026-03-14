#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include <QWidget>
#include <memory>

namespace wako::ui {

class StepGrid : public QWidget {
    Q_OBJECT
public:
    explicit StepGrid(std::shared_ptr<seq::Pattern> pattern, QWidget* parent = nullptr);

    void setCurrentStep(int step);          // appelé depuis le thread séquenceur via Qt::QueuedConnection
    void updatePattern(const seq::Pattern* pat);
    void setKit(const model::Kit* kit);

signals:
    void stepToggled(int pad, int step);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {800, 300}; }

private:
    static constexpr int LABEL_W  = 72;
    static constexpr int HEADER_H = 22;

    float cellW() const { return float(width()  - LABEL_W)  / seq::MAX_STEPS; }
    float cellH() const { return float(height() - HEADER_H) / seq::MAX_PADS;  }

    std::shared_ptr<seq::Pattern> pattern_;
    int                           currentStep_ = -1;
    std::vector<std::string>      padNames_;    // cache des noms affichés
};

} // namespace wako::ui
