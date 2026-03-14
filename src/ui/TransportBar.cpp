#include "TransportBar.h"
#include <QHBoxLayout>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>

namespace wako::ui {

static const QString BAR_STYLE =
    "QWidget { background:#3C3F41; }"
    "QPushButton { background:#4D4D4D; color:#DCDCDC; border:none;"
    "  padding:4px 10px; border-radius:3px; }"
    "QPushButton:checked { background:#FFA500; color:#1A1A1A; }"
    "QSpinBox { background:#4D4D4D; color:#DCDCDC; border:1px solid #666;"
    "  padding:2px; }"
    "QRadioButton { color:#DCDCDC; }"
    "QLabel#lcd { background:#050A14; color:#50DCFF;"
    "  font:bold 18px Monospace; border:1px solid #282840;"
    "  padding:1px 6px; border-radius:2px; min-width:42px; }";

TransportBar::TransportBar(QWidget* parent) : QWidget(parent) {
    setStyleSheet(BAR_STYLE);
    setFixedHeight(44);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    // Play / Stop
    playBtn_ = new QPushButton("▶");
    playBtn_->setFixedSize(34, 28);
    connect(playBtn_, &QPushButton::clicked, this, &TransportBar::playStopClicked);

    // Clear
    auto* clearBtn = new QPushButton("⟳");
    clearBtn->setFixedSize(30, 28);
    connect(clearBtn, &QPushButton::clicked, this, &TransportBar::clearClicked);

    // Séparateur
    auto sep = [&]() -> QFrame* {
        auto* f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFixedWidth(1);
        f->setStyleSheet("QFrame { background:#666; }");
        return f;
    };

    // BPM
    auto* bpmSpin = new QSpinBox;
    bpmSpin->setRange(1, 9999);
    bpmSpin->setValue(120);
    bpmSpin->setFixedWidth(72);
    connect(bpmSpin, &QSpinBox::valueChanged, this, &TransportBar::bpmChanged);

    // Steps
    auto* stepSpin = new QSpinBox;
    stepSpin->setRange(1, 32);
    stepSpin->setValue(16);
    stepSpin->setFixedWidth(48);
    connect(stepSpin, &QSpinBox::valueChanged, this, &TransportBar::lengthChanged);

    // Mode
    auto* oneShotBtn = new QRadioButton("1-Shot");
    auto* gateBtn    = new QRadioButton("Gate");
    oneShotBtn->setChecked(true);
    auto* grp = new QButtonGroup(this);
    grp->addButton(oneShotBtn, 0);
    grp->addButton(gateBtn,    1);
    connect(grp, &QButtonGroup::idClicked, this, [this](int id) {
        emit modeChanged(id == 1);
    });

    auto* saveBtn = new QPushButton("💾");
    saveBtn->setFixedSize(30, 28);
    connect(saveBtn, &QPushButton::clicked, this, &TransportBar::saveClicked);

    auto* loadBtn = new QPushButton("📂");
    loadBtn->setFixedSize(30, 28);
    connect(loadBtn, &QPushButton::clicked, this, &TransportBar::loadClicked);


    // Step LCD
    stepLcd_ = new QLabel(" 1");
    stepLcd_->setObjectName("lcd");
    stepLcd_->setAlignment(Qt::AlignCenter);


    lay->addWidget(saveBtn);
    lay->addWidget(loadBtn);
    lay->addWidget(playBtn_);
    lay->addWidget(clearBtn);    
    lay->addWidget(sep());
    lay->addWidget(new QLabel("Step"));
    lay->addWidget(stepLcd_);
    lay->addWidget(sep());
    lay->addWidget(new QLabel("BPM"));
    lay->addWidget(bpmSpin);
    lay->addWidget(sep());
    lay->addWidget(new QLabel("Steps"));
    lay->addWidget(stepSpin);
    lay->addWidget(sep());
    lay->addWidget(new QLabel("Mode"));
    lay->addWidget(oneShotBtn);
    lay->addWidget(gateBtn);

    lay->addStretch();
}

void TransportBar::setPlaying(bool playing) {
    playBtn_->setText(playing ? "⏸" : "▶");
}

void TransportBar::setStep(int step) {
    stepLcd_->setText(QString("%1").arg(step + 1, 2));
}

} // namespace wako::ui
