#include "TransportBar.h"
#include "SvgIcons.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QFrame>
#include <QResizeEvent>
#include <QTimer>

namespace wako::ui {

static const QString BAR_STYLE = R"(
QWidget { background: #3C3F41; }
QToolButton {
    background: transparent; color: #DCDCDC;
    border: none; padding: 3px 8px;
    border-radius: 4px; font-size: 10px;
}
QToolButton:hover   { background: #505050; }
QToolButton:pressed { background: #3a3a3a; }
QSpinBox {
    background: #4D4D4D; color: #DCDCDC;
    border: 1px solid #555; border-radius: 3px;
    padding: 2px 4px; font-size: 12px;
}
QComboBox {
    background: #4D4D4D; color: #DCDCDC;
    border: 1px solid #555; border-radius: 3px;
    padding: 2px 8px; font-size: 12px;
    min-width: 90px;
}
QComboBox::drop-down { border: none; width: 16px; }
QComboBox QAbstractItemView {
    background: #3C3F41; color: #DCDCDC;
    selection-background-color: #505050;
    border: 1px solid #555;
}
QLabel#sectionLabel { color: #888888; font-size: 10px; }
QLabel#lcd {
    background: #050A14; color: #50DCFF;
    border: 1px solid #282840;
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

    auto* saveBtn = makeBtn(icons::SAVE,        "Save");
    auto* loadBtn = makeBtn(icons::FOLDER_OPEN, "Open");
    connect(saveBtn, &QToolButton::clicked, this, &TransportBar::saveClicked);
    connect(loadBtn, &QToolButton::clicked, this, &TransportBar::loadClicked);

    kitCombo_ = new QComboBox;
    connect(kitCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TransportBar::kitChanged);

    playBtn_       = makeBtn(icons::PLAY,  "Play");
    auto* clearBtn = makeBtn(icons::CLEAR, "Clear");
    connect(playBtn_,  &QToolButton::clicked, this, &TransportBar::playStopClicked);
    connect(clearBtn,  &QToolButton::clicked, this, &TransportBar::clearClicked);

    stepLcd_ = new QLabel(" 1");
    stepLcd_->setObjectName("lcd");
    stepLcd_->setAlignment(Qt::AlignCenter);
    stepLcd_->setFont(icons::lcdFont(20));
    stepSection_ = makeSection("Step", stepLcd_);

    auto* bpmSpin = new QSpinBox;
    bpmSpin->setRange(1, 9999);
    bpmSpin->setValue(120);
    bpmSpin->setFixedWidth(68);
    auto* bpmDebounce = new QTimer(this);
    bpmDebounce->setSingleShot(true);
    bpmDebounce->setInterval(400);
    connect(bpmSpin, &QSpinBox::valueChanged,
            this, [bpmDebounce](int) { bpmDebounce->start(); });
    connect(bpmDebounce, &QTimer::timeout,
            this, [bpmSpin, this] { emit bpmChanged(bpmSpin->value()); });
    bpmSection_ = makeSection("BPM", bpmSpin);

    auto* stepSpin = new QSpinBox;
    stepSpin->setRange(1, 32);
    stepSpin->setValue(16);
    stepSpin->setFixedWidth(54);
    connect(stepSpin, &QSpinBox::valueChanged, this, &TransportBar::lengthChanged);
    stepsSection_ = makeSection("Steps", stepSpin);

    lay->addWidget(saveBtn);
    lay->addWidget(loadBtn);
    lay->addSpacing(2);
    lay->addWidget(makeSep());
    lay->addSpacing(2);
    lay->addWidget(kitCombo_);
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

void TransportBar::setKits(const QStringList& names, int currentIndex) {
    kitCombo_->blockSignals(true);
    kitCombo_->clear();
    for (const auto& n : names)
        kitCombo_->addItem(n);
    kitCombo_->setCurrentIndex(currentIndex);
    kitCombo_->blockSignals(false);
}

void TransportBar::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    bool compact = ev->size().width() < 550;
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