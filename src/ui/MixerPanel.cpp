#include "MixerPanel.h"
#include "EffectWindow.h"
#include "../audio/Player.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

namespace wako::ui {

// ──────────────────────────────────────────────────────────────────
// VuMeter
// ──────────────────────────────────────────────────────────────────
VuMeter::VuMeter(QWidget* parent) : QWidget(parent) {
    setMinimumSize(14, 80);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}
void VuMeter::setLevel(float level)   { level_    = std::clamp(level, 0.f, 1.f); update(); }
void VuMeter::setPeakHold(float peak) { peakHold_ = std::clamp(peak, 0.f, 1.f); }

void VuMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    int w = width(), h = height();
    p.fillRect(0, 0, w, h, QColor(20, 20, 22));
    constexpr int   SEGS = 20;
    int segH    = h / SEGS;
    int filled  = static_cast<int>(level_ * SEGS);
    for (int i = 0; i < SEGS; ++i) {
        int y  = h - (i + 1) * segH + 1;
        int sh = segH - 2;
        if (sh < 1) sh = 1;
        float pos = float(i) / float(SEGS);
        QColor col = pos < 0.70f ? QColor(30,180,60)
                   : pos < 0.88f ? QColor(210,190,20)
                   :               QColor(220,40,40);
        if (i >= filled) col = col.darker(400);
        p.fillRect(1, y, w-2, sh, col);
    }
    if (peakHold_ > 0.01f) {
        int py = std::clamp(h - int(peakHold_ * h) - 1, 0, h-2);
        p.setPen(QPen(QColor(255,255,255,200), 1));
        p.drawLine(1, py, w-2, py);
    }
}

// ──────────────────────────────────────────────────────────────────
// Styles
// ──────────────────────────────────────────────────────────────────
static const QString MIXER_STYLE = R"(
QWidget#mixerPanel { background: #1E1E20; }
QWidget#strip { background: #252527; border-radius: 4px; }
QLabel#trackLabel {
    color: #AAAAAA; font-size: 10px; font-weight: bold;
    qproperty-alignment: AlignCenter;
}
QLabel#masterLabel {
    color: #FFA040; font-size: 10px; font-weight: bold;
    qproperty-alignment: AlignCenter;
}
QLabel#dbLabel { color: #666; font-size: 9px; qproperty-alignment: AlignCenter; }
QSlider::groove:vertical { background:#333; width:4px; border-radius:2px; }
QSlider::handle:vertical {
    background:#888; border:1px solid #555;
    height:12px; width:16px; margin:0 -6px; border-radius:3px;
}
QSlider::handle:vertical:hover { background:#AAAAAA; }
QSlider::sub-page:vertical { background:#555; border-radius:2px; }
QPushButton#muteBtn {
    background:#3A2020; color:#CC5555;
    border:1px solid #5A3030; border-radius:3px;
    font-size:9px; font-weight:bold; padding:1px;
}
QPushButton#muteBtn:checked {
    background:#CC4444; color:#FFF;
    border-color:#FF6666;
}
QPushButton#soloBtn {
    background:#2A2A18; color:#AA9900;
    border:1px solid #4A4A28; border-radius:3px;
    font-size:9px; font-weight:bold; padding:1px;
}
QPushButton#soloBtn:checked {
    background:#BB9900; color:#1A1A00;
    border-color:#FFCC00;
}
QPushButton#fxBtn {
    background:#1A2A3A; color:#4A8ABF;
    border:1px solid #2A4A6A; border-radius:3px;
    font-size:9px; font-weight:bold; padding:1px;
}
QPushButton#fxBtn:checked {
    background:#2A5A8A; color:#AADDFF;
    border-color:#4A9ADF;
}
)";

static QFrame* makeVSep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setFixedWidth(1);
    f->setStyleSheet("QFrame { background: #3A3A3A; margin: 8px 0; }");
    return f;
}

// ──────────────────────────────────────────────────────────────────
// MixerPanel
// ──────────────────────────────────────────────────────────────────
MixerPanel::MixerPanel(std::shared_ptr<seq::Pattern> pattern, QWidget* parent)
    : QWidget(parent), pattern_(std::move(pattern))
{
    setObjectName("mixerPanel");
    setStyleSheet(MIXER_STYLE);
    fxWindows_.fill(nullptr);

    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(4);

    // 9 strips tracks
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* strip = new QWidget;
        strip->setObjectName("strip");
        strip->setFixedWidth(52);
        buildStrip(i, strip);
        mainLay->addWidget(strip);
    }

    mainLay->addWidget(makeVSep());

    // Strip master
    auto* masterStrip = new QWidget;
    masterStrip->setObjectName("strip");
    masterStrip->setFixedWidth(70);
    buildMasterStrip(masterStrip);
    mainLay->addWidget(masterStrip);

    mainLay->addStretch();

    timer_ = new QTimer(this);
    timer_->setInterval(TIMER_MS);
    connect(timer_, &QTimer::timeout, this, &MixerPanel::onTimer);
    timer_->start();
}

MixerPanel::~MixerPanel() {
    for (auto* w : fxWindows_)
        if (w) delete w;
}

void MixerPanel::buildStrip(int pad, QWidget* container) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(4, 6, 4, 4);
    lay->setSpacing(2);

    // Label
    auto* lbl = new QLabel("Pad " + QString::number(pad + 1));
    lbl->setObjectName("trackLabel");
    lbl->setWordWrap(true);
    trackLabels_[pad] = lbl;
    lay->addWidget(lbl);

    // VU meter
    auto* vu = new VuMeter(container);
    trackVu_[pad] = vu;
    lay->addWidget(vu, 1, Qt::AlignHCenter);

    // Slider volume
    auto* slider = new QSlider(Qt::Vertical);
    slider->setRange(0, 100);
    slider->setValue(100);
    slider->setFixedHeight(70);
    connect(slider, &QSlider::valueChanged, this, [this, pad](int v) {
        pattern_->setTrackVolume(pad, v / 100.f);
        emit trackVolumeChanged(pad, v / 100.f);
    });
    lay->addWidget(slider, 0, Qt::AlignHCenter);

    // dB label
    auto* dbLbl = new QLabel("0dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        dbLbl->setText(v == 0 ? "-∞" : QString::number(int(20.f * std::log10(v/100.f))) + "dB");
    });
    lay->addWidget(dbLbl);

    // ── M / S / FX boutons ────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);
    btnRow->setContentsMargins(0, 2, 0, 0);

    auto* mBtn = new QPushButton("M");
    mBtn->setObjectName("muteBtn");
    mBtn->setCheckable(true);
    mBtn->setFixedSize(16, 14);
    mBtn->setChecked(pattern_->muted[pad]);
    muteButtons_[pad] = mBtn;
    connect(mBtn, &QPushButton::clicked, this, [this, pad]() {
        emit trackMuteToggled(pad);
    });

    auto* sBtn = new QPushButton("S");
    sBtn->setObjectName("soloBtn");
    sBtn->setCheckable(true);
    sBtn->setFixedSize(16, 14);
    sBtn->setChecked(pattern_->soloed[pad]);
    soloButtons_[pad] = sBtn;
    connect(sBtn, &QPushButton::clicked, this, [this, pad]() {
        emit trackSoloToggled(pad);
    });

    auto* fxBtn = new QPushButton("FX");
    fxBtn->setObjectName("fxBtn");
    fxBtn->setCheckable(true);
    fxBtn->setFixedSize(20, 14);
    fxButtons_[pad] = fxBtn;
    connect(fxBtn, &QPushButton::clicked, this, [this, pad, fxBtn](bool checked) {
        if (!fxWindows_[pad]) {
            auto* lbl  = static_cast<QLabel*>(trackLabels_[pad]);
            QString name = lbl ? lbl->text() : QString("Pad %1").arg(pad + 1);
            auto*   win  = new EffectWindow(
                               audio::Player::instance().trackChain(pad), name, this);
            fxWindows_[pad] = win;
            connect(win, &QWidget::destroyed, fxBtn, [fxBtn]() {
                fxBtn->setChecked(false);
            });
        }
        if (checked) {
            fxWindows_[pad]->show();
            fxWindows_[pad]->raise();
        } else {
            fxWindows_[pad]->hide();
        }
    });

    btnRow->addWidget(mBtn);
    btnRow->addWidget(sBtn);
    btnRow->addWidget(fxBtn);
    lay->addLayout(btnRow);
}

void MixerPanel::buildMasterStrip(QWidget* container) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(4, 6, 4, 4);
    lay->setSpacing(2);

    auto* lbl = new QLabel("Master");
    lbl->setObjectName("masterLabel");
    lay->addWidget(lbl);

    // VU L+R
    auto* vuRow = new QHBoxLayout;
    vuRow->setSpacing(2);
    masterVuL_ = new VuMeter(container);
    masterVuR_ = new VuMeter(container);
    vuRow->addWidget(masterVuL_);
    vuRow->addWidget(masterVuR_);
    lay->addLayout(vuRow, 1);

    auto* slider = new QSlider(Qt::Vertical);
    slider->setRange(0, 100);
    slider->setValue(100);
    slider->setFixedHeight(70);
    connect(slider, &QSlider::valueChanged, this, [this](int v) {
        float vol = v / 100.f;
        pattern_->setMasterVolume(vol);
        audio::Player::instance().setMasterVolume(vol);
        emit masterVolumeChanged(vol);
    });
    lay->addWidget(slider, 0, Qt::AlignHCenter);

    auto* dbLbl = new QLabel("0dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        dbLbl->setText(v == 0 ? "-∞" : QString::number(int(20.f * std::log10(v/100.f))) + "dB");
    });
    lay->addWidget(dbLbl);

    // FX master
    masterFxBtn_ = new QPushButton("FX");
    masterFxBtn_->setObjectName("fxBtn");
    masterFxBtn_->setCheckable(true);
    masterFxBtn_->setFixedHeight(14);

    connect(masterFxBtn_, &QPushButton::clicked, this, [this](bool checked) {
        constexpr int MASTER_IDX = seq::MAX_PADS;
        if (!fxWindows_[MASTER_IDX]) {
            auto* win = new EffectWindow(
                audio::Player::instance().masterChain(), "Master", this);
            fxWindows_[MASTER_IDX] = win;
            connect(win, &QWidget::destroyed, masterFxBtn_, [this]() {
                masterFxBtn_->setChecked(false);
            });
        }
        if (checked) { fxWindows_[MASTER_IDX]->show(); fxWindows_[MASTER_IDX]->raise(); }
        else           fxWindows_[MASTER_IDX]->hide();
    });
    lay->addWidget(masterFxBtn_);
}

// ──────────────────────────────────────────────────────────────────
// Timer — peaks + sync M/S
// ──────────────────────────────────────────────────────────────────
void MixerPanel::onTimer() {
    auto& player = audio::Player::instance();

    for (int i = 0; i < seq::MAX_PADS; ++i) {
        float pk = player.trackPeak(i);
        trackVu_[i]->setLevel(pk);
        trackVu_[i]->setPeakHold(pk);
    }
    masterVuL_->setLevel(player.masterPeakL());
    masterVuR_->setLevel(player.masterPeakR());
    masterVuL_->setPeakHold(player.masterPeakL());
    masterVuR_->setPeakHold(player.masterPeakR());

    player.decayPeaks(0.82f);

    // Sync visuel M/S depuis le pattern
    updateMuteSoloButtons();
}

void MixerPanel::updateMuteSoloButtons() {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        if (muteButtons_[i])
            muteButtons_[i]->setChecked(pattern_->muted[i]);
        if (soloButtons_[i])
            soloButtons_[i]->setChecked(pattern_->soloed[i]);
    }
}

void MixerPanel::setKit(const model::Kit* kit) {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* lbl = static_cast<QLabel*>(trackLabels_[i]);
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        lbl->setText(pad ? QString::fromStdString(pad->name)
                         : QString("Pad %1").arg(i+1));
        // Mettre à jour le titre de la fenêtre FX si ouverte
        if (fxWindows_[i])
            fxWindows_[i]->setChannelName(lbl->text());
    }
}

void MixerPanel::resetAll() {
    for (auto* child : findChildren<QSlider*>())
        child->setValue(100);
}

} // namespace wako::ui