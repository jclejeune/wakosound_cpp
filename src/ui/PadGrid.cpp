#include "PadGrid.h"
#include <QTimer>

namespace wako::ui {

static constexpr const char* STYLE_NORMAL =
    "QPushButton { background:#4D4D4D; color:#DCDCDC; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:4px; }"
    "QPushButton:pressed { background:#666; }";

static constexpr const char* STYLE_FLASH =
    "QPushButton { background:#FFA500; color:#1A1A1A; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:4px; }";

static constexpr const char* STYLE_DISABLED =
    "QPushButton { background:#323232; color:#666; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:4px; }";

PadGrid::PadGrid(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setSpacing(3);
    grid->setContentsMargins(4, 4, 4, 4);

    // Disposition MPC : 7 8 9 / 4 5 6 / 1 2 3
    const int rows[9] = {0,0,0, 1,1,1, 2,2,2};
    const int cols[9] = {0,1,2, 0,1,2, 0,1,2};

    for (int i = 0; i < 9; ++i) {
        auto* btn = new QPushButton(QString("Pad %1").arg(i + 1), this);
        btn->setMinimumSize(90, 90);
        btn->setStyleSheet(STYLE_NORMAL);
        btn->setFocusPolicy(Qt::NoFocus);
        buttons_[i] = btn;
        grid->addWidget(btn, rows[i], cols[i]);

        connect(btn, &QPushButton::clicked, this, [this, i] {
            emit padTriggered(i);
            flashPad(i);
        });
    }
}

void PadGrid::refresh(const model::Kit* kit) {
    for (int i = 0; i < 9; ++i) {
        auto* btn = buttons_[i];
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        if (pad) {
            btn->setText(QString::fromStdString(pad->displayName()));
            btn->setEnabled(pad->enabled);
            btn->setStyleSheet(pad->enabled ? STYLE_NORMAL : STYLE_DISABLED);
        } else {
            btn->setText("—");
            btn->setEnabled(false);
            btn->setStyleSheet(STYLE_DISABLED);
        }
    }
}

void PadGrid::flashPad(int idx) {
    if (idx < 0 || idx >= 9) return;
    auto* btn = buttons_[idx];
    btn->setStyleSheet(STYLE_FLASH);
    QTimer::singleShot(150, btn, [btn] {
        btn->setStyleSheet(STYLE_NORMAL);
    });
}

} // namespace wako::ui
