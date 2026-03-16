#include "PadGrid.h"
#include <QTimer>
#include <QResizeEvent>

namespace wako::ui {

static constexpr const char* STYLE_NORMAL =
    "QPushButton { background:#4D4D4D; color:#DCDCDC; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }"
    "QPushButton:pressed { background:#666; }";

static constexpr const char* STYLE_FLASH =
    "QPushButton { background:#FFA500; color:#1A1A1A; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }";

static constexpr const char* STYLE_DISABLED =
    "QPushButton { background:#323232; color:#666; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }";

PadGrid::PadGrid(QWidget* parent) : QWidget(parent) {
    // Widget centré horizontalement
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* grid = new QGridLayout(this);
    grid->setSpacing(6);
    grid->setContentsMargins(8, 8, 8, 8);

    const int rows[9] = {0,0,0, 1,1,1, 2,2,2};
    const int cols[9] = {0,1,2, 0,1,2, 0,1,2};

    for (int i = 0; i < 9; ++i) {
        auto* btn = new QPushButton(QString("Pad %1").arg(i + 1), this);
        // Taille fixe — sera recalculée dans resizeEvent
        btn->setFixedSize(100, 100);
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

void PadGrid::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);

    int spacing = 6;
    int margin  = 16;

    // padSize = plus grand carré qui tient dans l'espace disponible
    int fromH = (ev->size().height() - margin * 2 - spacing * 2) / 3;
    int fromW = (ev->size().width()  - margin * 2 - spacing * 2) / 3;
    int padSize = std::min(fromH, fromW);
    if (padSize < 60) padSize = 60;

    for (auto* btn : buttons_)
        btn->setFixedSize(padSize, padSize);

    // Centrer la grille dans l'espace disponible
    int gridSize = padSize * 3 + spacing * 2 + margin * 2;
    int offsetX  = std::max(0, (ev->size().width()  - gridSize) / 2);
    int offsetY  = std::max(0, (ev->size().height() - gridSize) / 2);
    layout()->setContentsMargins(offsetX, offsetY, offsetX, offsetY);
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