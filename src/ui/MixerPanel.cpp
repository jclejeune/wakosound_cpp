#include "MixerPanel.h"
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

void VuMeter::setLevel(float level) {
    level_    = std::clamp(level, 0.f, 1.f);
    update();
}
void VuMeter::setPeakHold(float peak) {
    peakHold_ = std::clamp(peak, 0.f, 1.f);
}

void VuMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // Fond
    p.fillRect(0, 0, w, h, QColor(20, 20, 22));

    constexpr int   SEGMENTS  = 20;
    constexpr float GREEN_MAX = 0.70f;
    constexpr float YELLOW_MAX= 0.88f;

    int segH    = h / SEGMENTS;
    int filled  = static_cast<int>(level_ * SEGMENTS);
    int margin  = 1;

    for (int i = 0; i < SEGMENTS; ++i) {
        // i=0 en bas, i=SEGMENTS-1 en haut
        int y = h - (i + 1) * segH + margin;
        int sh = segH - margin * 2;
        if (sh < 1) sh = 1;

        float pos = float(i) / float(SEGMENTS);

        QColor col;
        if (pos < GREEN_MAX)       col = QColor(30, 180, 60);
        else if (pos < YELLOW_MAX) col = QColor(210, 190, 20);
        else                       col = QColor(220, 40, 40);

        if (i >= filled)
            col = col.darker(400);   // éteint

        p.fillRect(1, y, w - 2, sh, col);
    }

    // Peak hold — ligne blanche
    if (peakHold_ > 0.01f) {
        int py = h - static_cast<int>(peakHold_ * h) - 1;
        py = std::clamp(py, 0, h - 2);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1));
        p.drawLine(1, py, w - 2, py);
    }
}

// ──────────────────────────────────────────────────────────────────
// Helpers style
// ──────────────────────────────────────────────────────────────────
static const QString MIXER_STYLE = R"(
QWidget#mixerPanel {
    background: #1E1E20;
}
QWidget#strip {
    background: #252527;
    border-radius: 4px;
}
QLabel#trackLabel {
    color: #AAAAAA;
    font-size: 10px;
    font-weight: bold;
    qproperty-alignment: AlignCenter;
}
QLabel#dbLabel {
    color: #666;
    font-size: 9px;
    qproperty-alignment: AlignCenter;
}
QLabel#masterLabel {
    color: #FFA040;
    font-size: 10px;
    font-weight: bold;
    qproperty-alignment: AlignCenter;
}
QSlider::groove:vertical {
    background: #333;
    width: 4px;
    border-radius: 2px;
}
QSlider::handle:vertical {
    background: #888;
    border: 1px solid #555;
    height: 12px;
    width: 16px;
    margin: 0 -6px;
    border-radius: 3px;
}
QSlider::handle:vertical:hover { background: #AAAAAA; }
QSlider::sub-page:vertical     { background: #555; border-radius: 2px; }
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

    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(4);

    // ── 9 strips tracks ───────────────────────────────────────────
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* strip = new QWidget;
        strip->setObjectName("strip");
        strip->setFixedWidth(52);
        buildStrip(i, strip);
        mainLay->addWidget(strip);
    }

    mainLay->addWidget(makeVSep());

    // ── Strip master ──────────────────────────────────────────────
    auto* masterStrip = new QWidget;
    masterStrip->setObjectName("strip");
    masterStrip->setFixedWidth(70);
    buildMasterStrip(masterStrip);
    mainLay->addWidget(masterStrip);

    mainLay->addStretch();

    // ── Timer 30 fps ──────────────────────────────────────────────
    timer_ = new QTimer(this);
    timer_->setInterval(TIMER_MS);
    connect(timer_, &QTimer::timeout, this, &MixerPanel::onTimer);
    timer_->start();
}

void MixerPanel::buildStrip(int pad, QWidget* container) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(4, 6, 4, 6);
    lay->setSpacing(3);

    // Label pad
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
    slider->setFixedHeight(80);
    slider->setToolTip(QString("Track %1 volume").arg(pad + 1));
    connect(slider, &QSlider::valueChanged, this, [this, pad](int v) {
        float vol = v / 100.f;
        pattern_->setTrackVolume(pad, vol);
        emit trackVolumeChanged(pad, vol);
    });
    lay->addWidget(slider, 0, Qt::AlignHCenter);

    // Label dB
    auto* dbLbl = new QLabel("0 dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        if (v == 0)
            dbLbl->setText("-∞");
        else {
            float db = 20.f * std::log10(v / 100.f);
            dbLbl->setText(QString::number(int(db)) + "dB");
        }
    });
    lay->addWidget(dbLbl);
}

void MixerPanel::buildMasterStrip(QWidget* container) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(4, 6, 4, 6);
    lay->setSpacing(3);

    auto* lbl = new QLabel("Master");
    lbl->setObjectName("masterLabel");
    lay->addWidget(lbl);

    // Deux VU meters L + R côte à côte
    auto* vuRow = new QHBoxLayout;
    vuRow->setSpacing(2);
    masterVuL_ = new VuMeter(container);
    masterVuR_ = new VuMeter(container);
    vuRow->addWidget(masterVuL_);
    vuRow->addWidget(masterVuR_);
    lay->addLayout(vuRow, 1);

    // Slider master
    auto* slider = new QSlider(Qt::Vertical);
    slider->setRange(0, 100);
    slider->setValue(100);
    slider->setFixedHeight(80);
    slider->setToolTip("Master volume");
    connect(slider, &QSlider::valueChanged, this, [this](int v) {
        float vol = v / 100.f;
        pattern_->setMasterVolume(vol);
        audio::Player::instance().setMasterVolume(vol);
        emit masterVolumeChanged(vol);
    });
    lay->addWidget(slider, 0, Qt::AlignHCenter);

    auto* dbLbl = new QLabel("0 dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        if (v == 0)
            dbLbl->setText("-∞");
        else {
            float db = 20.f * std::log10(v / 100.f);
            dbLbl->setText(QString::number(int(db)) + "dB");
        }
    });
    lay->addWidget(dbLbl);
}

// ──────────────────────────────────────────────────────────────────
// Timer — lecture peaks + decay
// ──────────────────────────────────────────────────────────────────
void MixerPanel::onTimer() {
    auto& player = audio::Player::instance();

    // Tracks
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        float pk = player.trackPeak(i);
        trackVu_[i]->setLevel(pk);
        trackVu_[i]->setPeakHold(pk);   // peak hold géré par decay
    }

    // Master
    masterVuL_->setLevel(player.masterPeakL());
    masterVuR_->setLevel(player.masterPeakR());
    masterVuL_->setPeakHold(player.masterPeakL());
    masterVuR_->setPeakHold(player.masterPeakR());

    // Decay des atomics côté audio
    player.decayPeaks(0.82f);
}

// ──────────────────────────────────────────────────────────────────
void MixerPanel::setKit(const model::Kit* kit) {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* lbl = static_cast<QLabel*>(trackLabels_[i]);
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        lbl->setText(pad ? QString::fromStdString(pad->name)
                         : QString("Pad %1").arg(i + 1));
    }
}

void MixerPanel::resetAll() {
    // Remettre tous les sliders à 100 → déclenche valueChanged → reset pattern
    for (auto* child : findChildren<QSlider*>())
        child->setValue(100);
}

} // namespace wako::ui