#include "TransportBar.h"
#include "SvgIcons.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QFrame>
#include <QResizeEvent>
#include <QToolButton>
#include <QTimer>

namespace wako::ui {

static const QString BAR_STYLE = R"(
QWidget       { background: #3C3F41; }
QToolButton {
    background: transparent; color: #DCDCDC;
    border: none; padding: 3px 8px;
    border-radius: 4px; font-size: 10px;
}
QToolButton:hover  { background: #505050; }
QToolButton:pressed{ background: #3a3a3a; }
QSpinBox {
    background: #4D4D4D; color: #DCDCDC;
    border: 1px solid #555; border-radius: 3px;
    padding: 2px 4px; font-size: 12px;
}
QLabel#sectionLabel { color: #888888; font-size: 10px; }
QLabel#lcd {
    background: #050A14; color: #50DCFF;
    font: bold 18px Monospace; border: 1px solid #282840;
    padding: 1px 8px; border-radius: 3px; min-width: 44px;
}
)";

static QFrame* makeSep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setFixedWidth(1);
    f->setStyleSheet("QFrame { background: #555; margin: 6px 0px; }");
    return f;
}

static QToolButton* makeBtn(const char* svg, const QString& label, int iconSize = 18) {
    auto* btn = new QToolButton;
    btn->setIcon(icons::icon(svg, iconSize));
    btn->setIconSize({iconSize, iconSize});
    btn->setText(label);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setMinimumSize(44, 44);
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return btn;
}

static QWidget* makeSection(const QString& lbl, QWidget* ctrl) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    auto* label = new QLabel(lbl);
    label->setObjectName("sectionLabel");
    label->setAlignment(Qt::AlignCenter);
    lay->addWidget(label, 0, Qt::AlignHCenter);
    lay->addWidget(ctrl,  0, Qt::AlignHCenter);
    return w;
}

TransportBar::TransportBar(QWidget* parent) : QWidget(parent) {
    setStyleSheet(BAR_STYLE);
    setFixedHeight(56);
    setMinimumWidth(280);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 4, 10, 4);
    lay->setSpacing(4);

    // Fichiers
    auto* saveBtn = makeBtn(icons::SAVE,        "Save");
    auto* loadBtn = makeBtn(icons::FOLDER_OPEN, "Open");
    connect(saveBtn, &QToolButton::clicked, this, &TransportBar::saveClicked);
    connect(loadBtn, &QToolButton::clicked, this, &TransportBar::loadClicked);

    // Transport
    playBtn_   = makeBtn(icons::PLAY,  "Play");
    auto* clearBtn = makeBtn(icons::CLEAR, "Clear");
    connect(playBtn_,  &QToolButton::clicked, this, &TransportBar::playStopClicked);
    connect(clearBtn,  &QToolButton::clicked, this, &TransportBar::clearClicked);

    // Step LCD
    stepLcd_ = new QLabel(" 1");
    stepLcd_->setObjectName("lcd");
    stepLcd_->setAlignment(Qt::AlignCenter);
    stepSection_ = makeSection("Step", stepLcd_);

    // BPM — debounce 400ms
    auto* bpmSpin = new QSpinBox;
    bpmSpin->setRange(1, 9999);
    bpmSpin->setValue(120);
    bpmSpin->setFixedWidth(68);
    auto* bpmDebounce = new QTimer(this);
    bpmDebounce->setSingleShot(true);
    bpmDebounce->setInterval(400);
    connect(bpmSpin, &QSpinBox::valueChanged, this,
            [bpmDebounce](int) { bpmDebounce->start(); });
    connect(bpmDebounce, &QTimer::timeout, this,
            [bpmSpin, this] { emit bpmChanged(bpmSpin->value()); });
    bpmSection_ = makeSection("BPM", bpmSpin);

    // Steps
    auto* stepSpin = new QSpinBox;
    stepSpin->setRange(1, 32);
    stepSpin->setValue(16);
    stepSpin->setFixedWidth(54);
    connect(stepSpin, &QSpinBox::valueChanged, this, &TransportBar::lengthChanged);
    stepsSection_ = makeSection("Steps", stepSpin);

    // Layout — plus de section Mode
    lay->addWidget(saveBtn);
    lay->addWidget(loadBtn);
    lay->addSpacing(2);
    lay->addWidget(makeSep());
    lay->addSpacing(2);
    lay->addWidget(playBtn_);
    lay->addWidget(clearBtn);
    lay->addSpacing(2);
    lay->addWidget(makeSep());
    lay->addSpacing(4);
    lay->addWidget(stepSection_);
    lay->addSpacing(4);
    lay->addWidget(makeSep());
    lay->addSpacing(4);
    lay->addWidget(bpmSection_);
    lay->addSpacing(4);
    lay->addWidget(makeSep());
    lay->addSpacing(4);
    lay->addWidget(stepsSection_);
    lay->addStretch();
}

void TransportBar::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    bool compact = ev->size().width() < 500;
    if (compact != compact_) {
        compact_ = compact;
        stepSection_->setVisible(!compact);
        bpmSection_->setVisible(!compact);
        stepsSection_->setVisible(!compact);
    }
}

void TransportBar::setPlaying(bool playing) {
    playBtn_->setIcon(icons::icon(playing ? icons::PAUSE : icons::PLAY));
    playBtn_->setText(playing ? "Stop" : "Play");
}

void TransportBar::setStep(int step) {
    stepLcd_->setText(QString("%1").arg(step + 1, 2));
}

} // namespace wako::ui