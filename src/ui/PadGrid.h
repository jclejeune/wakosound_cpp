#pragma once
#include "../model/KitManager.h"
#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <array>

namespace wako::ui {

class PadGrid : public QWidget {
    Q_OBJECT
public:
    explicit PadGrid(QWidget* parent = nullptr);

    void refresh(const model::Kit* kit);
    void flashPad(int idx);

signals:
    void padTriggered(int padIdx);

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    std::array<QPushButton*, 9> buttons_{};
};

} // namespace wako::ui