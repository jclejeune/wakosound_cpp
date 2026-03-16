#include "TransportBar.h"
#include "SvgIcons.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QResizeEvent>
#include <QToolButton>

namespace wako::ui {

static const QString BAR_STYLE = R"(
QWidget       { background: #3C3F41; }

QPushButton {
    background: transparent;
    color:      #DCDCDC;
    border:     none;
    padding:    3px 8px;
    border-radius: 4px;
    font-size:  10px;
}
QPushButton:hover  { background: #505050; }
QPushButton:pressed{ background: #3a3a3a; }

QSpinBox {
    background: #4D4D4D;
    color:      #DCDCDC;
    border:     1px solid #555;
    border-radius: 3px;
    padding:    2px 4px;
    font-size:  12px;
}

QRadioButton          { color: #DCDCDC; font-size: 11px; spacing: 4px; }
QRadioButton::indicator { width: 12px; height: 12px; }

QLabel#sectionLabel   { color: #888888; font-size: 10px; }

QLabel#lcd {
    background: #050A14;
    color:      #50DCFF;
    font:       bold 18px Monospace;
    border:     1px solid #282840;
    padding:    1px 8px;
    border-radius: 3px;
    min-width:  44px;
}
)";

// ── Helpers ───────────────────────────────────────────────────────

static QFrame* makeSep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setFixedWidth(1);
    f->setStyleSheet("QFrame { background: #555; margin: 6px 0px; }");
    return f;
}

// Bouton avec icône SVG (haut) + label texte (bas) — QToolButton natif
static QToolButton* makeBtn(const char* svg, const QString& label,
                             int iconSize = 18) {
    auto* btn = new QToolButton;
    btn->setIcon(icons::icon(svg, iconSize));
    btn->setIconSize({iconSize, iconSize});
    btn->setText(label);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setMinimumSize(44, 44);
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    btn->setStyleSheet(
        "QToolButton { background:transparent; color:#DCDCDC; border:none;"
        "  padding:2px 6px; border-radius:4px; font-size:10px; }"
        "QToolButton:hover  { background:#505050; }"
        "QToolButton:pressed{ background:#3a3a3a; }"
    );
    return btn;
}

// Section : petit label gris dessus + widget dessous
static QWidget* makeSection(const QString& lbl, QWidget* ctrl) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    auto* label = new QLabel(lbl);
    label->setObjectName("sectionLabel");
    label->setAlignment(Qt::AlignCenter);

    lay->addWidget(label,  0, Qt::AlignHCenter);
    lay->addWidget(ctrl,   0, Qt::AlignHCenter);
    return w;
}

static QWidget* makeSectionH(const QString& lbl, QWidget* w1, QWidget* w2) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    auto* label = new QLabel(lbl);
    label->setObjectName("sectionLabel");
    label->setAlignment(Qt::AlignCenter);

    auto* row    = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(6);
    rowLay->addWidget(w1);
    rowLay->addWidget(w2);

    lay->addWidget(label, 0, Qt::AlignHCenter);
    lay->addWidget(row,   0, Qt::AlignHCenter);
    return w;
}

// ── Constructeur ──────────────────────────────────────────────────

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
    connect(saveBtn, &QPushButton::clicked, this, &TransportBar::saveClicked);
    connect(loadBtn, &QPushButton::clicked, this, &TransportBar::loadClicked);

    // Transport
    playBtn_ = makeBtn(icons::PLAY,  "Play");
    auto* clearBtn = makeBtn(icons::CLEAR, "Clear");
    connect(playBtn_,  &QPushButton::clicked, this, &TransportBar::playStopClicked);
    connect(clearBtn,  &QPushButton::clicked, this, &TransportBar::clearClicked);

    // Step LCD
    stepLcd_ = new QLabel(" 1");
    stepLcd_->setObjectName("lcd");
    stepLcd_->setAlignment(Qt::AlignCenter);
    stepSection_ = makeSection("Step", stepLcd_);

    // BPM
    auto* bpmSpin = new QSpinBox;
    bpmSpin->setRange(1, 9999);
    bpmSpin->setValue(120);
    bpmSpin->setFixedWidth(68);
    connect(bpmSpin, &QSpinBox::valueChanged, this, &TransportBar::bpmChanged);
    bpmSection_ = makeSection("BPM", bpmSpin);

    // Steps
    auto* stepSpin = new QSpinBox;
    stepSpin->setRange(1, 32);
    stepSpin->setValue(16);
    stepSpin->setFixedWidth(54);
    connect(stepSpin, &QSpinBox::valueChanged, this, &TransportBar::lengthChanged);
    stepsSection_ = makeSection("Steps", stepSpin);

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
    modeSection_ = makeSectionH("Mode", oneShotBtn, gateBtn);

    // Layout
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
    lay->addSpacing(4);
    lay->addWidget(makeSep());
    lay->addSpacing(4);
    lay->addWidget(modeSection_);
    lay->addStretch();
}

// ── Responsive ───────────────────────────────────────────────────

void TransportBar::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    bool compact = ev->size().width() < 650;
    if (compact != compact_) {
        compact_ = compact;
        updateCompact(compact);
    }
}

void TransportBar::updateCompact(bool compact) {
    stepSection_->setVisible(!compact);
    bpmSection_->setVisible(!compact);
    stepsSection_->setVisible(!compact);
    modeSection_->setVisible(!compact);
}

// ── Setters ───────────────────────────────────────────────────────

void TransportBar::setPlaying(bool playing) {
    playBtn_->setIcon(icons::icon(playing ? icons::PAUSE : icons::PLAY));
    playBtn_->setText(playing ? "Stop" : "Play");
}

void TransportBar::setStep(int step) {
    stepLcd_->setText(QString("%1").arg(step + 1, 2));
}

} // namespace wako::ui