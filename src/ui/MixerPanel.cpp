#include "MixerPanel.h"
#include "EffectWindow.h"
#include "../audio/Player.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

namespace wako::ui {

// ──────────────────────────────────────────────────────────────────
// Helpers +12 dB max → 0 dB au milieu (slider 0..200)
// ──────────────────────────────────────────────────────────────────
static constexpr int   SLIDER_MAX  = 200;
static constexpr int   ZERO_DB_POS = 100;
static constexpr float MAX_GAIN    = 4.0f;  // +12 dB

static float sliderToVolume(int v) {
    float t = static_cast<float>(v) / SLIDER_MAX;
    return t * t * MAX_GAIN;
}

static int volumeToSlider(float vol) {
    float t = std::sqrt(std::clamp(vol, 0.f, MAX_GAIN) / MAX_GAIN);
    return static_cast<int>(t * SLIDER_MAX);
}

static QString volumeToDb(float vol) {
    if (vol <= 0.0001f) return QStringLiteral("-∞ dB");
    float db = 20.f * std::log10(vol);
    return QString::asprintf("%+.1f dB", db);
}

// ──────────────────────────────────────────────────────────────────
// VuMeter
// ──────────────────────────────────────────────────────────────────
VuMeter::VuMeter(QWidget* parent) : QWidget(parent) {
    setFixedHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void VuMeter::setLevel(float level)   { level_ = std::clamp(level, 0.f, 1.f); update(); }
void VuMeter::setPeakHold(float peak) { peakHold_ = std::clamp(peak, 0.f, 1.f); }

void VuMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    int w = width(), h = height();
    p.fillRect(0, 0, w, h, QColor(20, 20, 22));

    constexpr int SEGS = 20;
    int segH = h / SEGS;
    int filled = static_cast<int>(level_ * SEGS);

    for (int i = 0; i < SEGS; ++i) {
        int y = h - (i + 1) * segH + 1;
        int sh = segH - 2; if (sh < 1) sh = 1;
        float pos = float(i) / SEGS;
        QColor col = pos < 0.70f ? QColor(30,180,60)
                    : pos < 0.88f ? QColor(210,190,20)
                                  : QColor(220,40,40);
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
// MixerFader – FADER STYLE CONSOLE ANALOGIQUE HAUT DE GAMME
// Graduation 0 dB noire + encoche centrale + traits +6/–6 dB
// ──────────────────────────────────────────────────────────────────
MixerFader::MixerFader(Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent), zeroDbPos_(ZERO_DB_POS)
{
    setFocusPolicy(Qt::NoFocus);  // enlève le rectangle de focus moche
}

void MixerFader::paintEvent(QPaintEvent* e)
{
    QSlider::paintEvent(e);  // Qt dessine groove + handle

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int handleH = 18;
    const int usable = height() - handleH;

    // Position 0 dB
    const float ratio0 = 1.0f - float(zeroDbPos_) / float(maximum());
    const int y0 = handleH / 2 + static_cast<int>(ratio0 * usable);

    // Position +6 dB et –6 dB
    const int vPlus6  = volumeToSlider(2.0f);   // +6 dB
    const int vMinus6 = volumeToSlider(0.5f);  // –6 dB
    const float ratioP6 = 1.0f - float(vPlus6)  / SLIDER_MAX;
    const float ratioM6 = 1.0f - float(vMinus6) / SLIDER_MAX;
    const int yPlus6  = handleH / 2 + static_cast<int>(ratioP6 * usable);
    const int yMinus6 = handleH / 2 + static_cast<int>(ratioM6 * usable);

    const int cx = width() / 2;

    // 1. Ligne principale 0 dB – noir mat légèrement relevé
    p.setPen(QPen(QColor(38, 38, 42), 1.4));
    p.drawLine(0, y0, width(), y0);

    // 2. Micro-relief (effet gravé)
    p.setPen(QPen(QColor(15, 15, 15), 1.0));
    p.drawLine(0, y0 + 1, width(), y0 + 1);
    p.setPen(QPen(QColor(72, 72, 76), 1.0));
    p.drawLine(0, y0 - 1, width(), y0 - 1);

    // 3. Encoche centrale 0 dB – repère immédiat (style SSL 9000)
    p.setPen(QPen(QColor(12, 12, 12), 2.2));
    p.drawLine(cx - 5, y0, cx + 5, y0);

    // 4. Traits +6 dB et –6 dB – très discrets
    p.setPen(QPen(QColor(28, 28, 30), 0.9));
    p.drawLine(cx - 3, yPlus6,  cx + 3, yPlus6);
    p.drawLine(cx - 3, yMinus6, cx + 3, yMinus6);
}

// ──────────────────────────────────────────────────────────────────
// Style – Version finale PRO AUDIO (à ne plus toucher)
// ──────────────────────────────────────────────────────────────────
static const QString MIXER_STYLE = R"(
QWidget#mixerPanel { background: #1E1E20; }
QWidget#strip { background: #252527; border-radius: 4px; }
QLabel#trackLabel  { color: #AAAAAA; font-size: 10px; font-weight: bold; qproperty-alignment: AlignCenter; }
QLabel#masterLabel { color: #FFA040; font-size: 10px; font-weight: bold; qproperty-alignment: AlignCenter; }
QLabel#dbLabel     { color: #888; font-size: 9px; qproperty-alignment: AlignCenter; }
QSlider::groove:vertical { background: #1e1e22; width: 6px; border-radius: 3px; }
QSlider::handle:vertical {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #484854, stop:0.35 #6e6e7a, stop:0.5 #787888,
        stop:0.65 #6e6e7a, stop:1 #484854);
    border: 1px solid #2a2a2e;
    height: 18px; width: 26px; margin: 0 -10px; border-radius: 3px;
}
QSlider::handle:vertical:hover   { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #585866, stop:0.5 #88889c, stop:1 #585866); }
QSlider::handle:vertical:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #686880, stop:0.5 #9999b4, stop:1 #686880); }
QSlider::sub-page:vertical, QSlider::add-page:vertical { background: #1e1e22; border-radius: 3px; }
QPushButton#muteBtn      { background:#3A2020; color:#CC5555; border:1px solid #5A3030; border-radius:3px; font-size:9px; font-weight:bold; padding:1px; }
QPushButton#muteBtn:checked { background:#CC4444; color:#FFF; border-color:#FF6666; }
QPushButton#soloBtn      { background:#2A2A18; color:#AA9900; border:1px solid #4A4A28; border-radius:3px; font-size:9px; font-weight:bold; padding:1px; }
QPushButton#soloBtn:checked { background:#BB9900; color:#1A1A00; border-color:#FFCC00; }
QPushButton#retriggerBtn { background:#1A2A1A; color:#449944; border:1px solid #2A4A2A; border-radius:3px; font-size:9px; font-weight:bold; padding:1px; }
QPushButton#retriggerBtn:checked { background:#228822; color:#CCFFCC; border-color:#44CC44; }
QPushButton#fxBtn        { background:#1A2A3A; color:#4A8ABF; border:1px solid #2A4A6A; border-radius:3px; font-size:9px; font-weight:bold; padding:1px; }
QPushButton#fxBtn:checked { background:#2A5A8A; color:#AADDFF; border-color:#4A9ADF; }
)";

static QFrame* makeVSep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setFixedWidth(1);
    f->setStyleSheet("background: #3A3A3A; margin: 8px 0;");
    return f;
}

// ──────────────────────────────────────────────────────────────────
// MixerPanel – tout le reste (inchangé mais propre)
// ──────────────────────────────────────────────────────────────────
MixerPanel::MixerPanel(std::shared_ptr<seq::Pattern> pattern, QWidget* parent)
    : QWidget(parent), pattern_(std::move(pattern))
{
    setObjectName("mixerPanel");
    setStyleSheet(MIXER_STYLE);
    fxWindows_.fill(nullptr);
    trackSliders_.fill(nullptr);

    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(4);

    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* strip = new QWidget;
        strip->setObjectName("strip");
        strip->setMinimumWidth(48);
        buildStrip(i, strip);
        mainLay->addWidget(strip, 1);
    }

    mainLay->addWidget(makeVSep());

    auto* masterStrip = new QWidget;
    masterStrip->setObjectName("strip");
    masterStrip->setMinimumWidth(60);
    buildMasterStrip(masterStrip);
    mainLay->addWidget(masterStrip, 1);

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MixerPanel::onTimer);
    timer_->start();
}

MixerPanel::~MixerPanel() {
    for (auto* w : fxWindows_) if (w) delete w;
}

void MixerPanel::buildStrip(int pad, QWidget* container [[maybe_unused]]) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(6, 8, 6, 8);
    lay->setSpacing(5);

    auto* lbl = new QLabel(QString("Pad %1").arg(pad + 1));
    lbl->setObjectName("trackLabel");
    lbl->setWordWrap(true);
    trackLabels_[pad] = lbl;
    lay->addWidget(lbl);

    auto* vu = new VuMeter(container);
    trackVu_[pad] = vu;
    lay->addWidget(vu);

    auto* slider = new MixerFader(Qt::Vertical);
    slider->setRange(0, SLIDER_MAX);
    slider->setValue(ZERO_DB_POS);
    slider->setZeroDbPos(ZERO_DB_POS);
    slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    trackSliders_[pad] = slider;

    connect(slider, &QSlider::valueChanged, this, [this, pad](int v) {
        float vol = sliderToVolume(v);
        pattern_->setTrackVolume(pad, vol);
        emit trackVolumeChanged(pad, vol);
    });
    lay->addWidget(slider, 1, Qt::AlignHCenter);

    auto* dbLbl = new QLabel("0.0 dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        dbLbl->setText(volumeToDb(sliderToVolume(v)));
    });
    lay->addWidget(dbLbl);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);
    btnRow->setContentsMargins(0, 2, 0, 0);

    auto* mBtn = new QPushButton("M"); mBtn->setObjectName("muteBtn");      mBtn->setCheckable(true); mBtn->setFixedSize(16,14); muteButtons_[pad] = mBtn;
    auto* sBtn = new QPushButton("S"); sBtn->setObjectName("soloBtn");      sBtn->setCheckable(true); sBtn->setFixedSize(16,14); soloButtons_[pad] = sBtn;
    auto* rBtn = new QPushButton("R"); rBtn->setObjectName("retriggerBtn"); rBtn->setCheckable(true); rBtn->setFixedSize(16,14); retriggerButtons_[pad] = rBtn;
    auto* fxBtn = new QPushButton("FX"); fxBtn->setObjectName("fxBtn"); fxBtn->setCheckable(true); fxBtn->setFixedSize(20,14); fxButtons_[pad] = fxBtn;

    connect(mBtn, &QPushButton::clicked, this, [this, pad]() { emit trackMuteToggled(pad); });
    connect(sBtn, &QPushButton::clicked, this, [this, pad]() { emit trackSoloToggled(pad); });
    connect(rBtn, &QPushButton::clicked, this, [this, pad]() { emit trackRetriggerToggled(pad); });
    connect(fxBtn, &QPushButton::clicked, this, [this, pad, fxBtn](bool checked) {
        if (!fxWindows_[pad]) {
            QString name = static_cast<QLabel*>(trackLabels_[pad])->text();
            fxWindows_[pad] = new EffectWindow(audio::Player::instance().trackChain(pad), name, this);
            connect(fxWindows_[pad], &EffectWindow::closed, fxBtn, [fxBtn]() { fxBtn->setChecked(false); });
        }
        fxWindows_[pad]->setVisible(checked);
        if (checked) fxWindows_[pad]->raise();
    });

    btnRow->addWidget(mBtn); btnRow->addWidget(sBtn); btnRow->addWidget(rBtn); btnRow->addWidget(fxBtn);
    lay->addLayout(btnRow);
}

void MixerPanel::buildMasterStrip(QWidget* container [[maybe_unused]]) {
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(6, 8, 6, 8);
    lay->setSpacing(5);

    auto* lbl = new QLabel("Master");
    lbl->setObjectName("masterLabel");
    lay->addWidget(lbl);

    auto* vuRow = new QHBoxLayout;
    vuRow->setSpacing(2);
    masterVuL_ = new VuMeter(container);
    masterVuR_ = new VuMeter(container);
    vuRow->addWidget(masterVuL_);
    vuRow->addWidget(masterVuR_);
    lay->addLayout(vuRow);

    auto* slider = new MixerFader(Qt::Vertical);
    slider->setRange(0, SLIDER_MAX);
    slider->setValue(ZERO_DB_POS);
    slider->setZeroDbPos(ZERO_DB_POS);
    slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    masterSlider_ = slider;

    connect(slider, &QSlider::valueChanged, this, [this](int v) {
        float vol = sliderToVolume(v);
        pattern_->setMasterVolume(vol);
        audio::Player::instance().setMasterVolume(vol);
        emit masterVolumeChanged(vol);
    });
    lay->addWidget(slider, 1, Qt::AlignHCenter);

    auto* dbLbl = new QLabel("0.0 dB");
    dbLbl->setObjectName("dbLabel");
    connect(slider, &QSlider::valueChanged, this, [dbLbl](int v) {
        dbLbl->setText(volumeToDb(sliderToVolume(v)));
    });
    lay->addWidget(dbLbl);

    masterFxBtn_ = new QPushButton("FX");
    masterFxBtn_->setObjectName("fxBtn");
    masterFxBtn_->setCheckable(true);
    masterFxBtn_->setFixedHeight(14);
    connect(masterFxBtn_, &QPushButton::clicked, this, [this](bool checked) {
        constexpr int MASTER_IDX = seq::MAX_PADS;
        if (!fxWindows_[MASTER_IDX]) {
            fxWindows_[MASTER_IDX] = new EffectWindow(audio::Player::instance().masterChain(), "Master", this);
            connect(fxWindows_[MASTER_IDX], &EffectWindow::closed, masterFxBtn_, [this]() { masterFxBtn_->setChecked(false); });
        }
        fxWindows_[MASTER_IDX]->setVisible(checked);
        if (checked) fxWindows_[MASTER_IDX]->raise();
    });
    lay->addWidget(masterFxBtn_);
}

void MixerPanel::onTimer() {
    if (!masterVuL_ || !masterVuR_) return;

    auto& player = audio::Player::instance();
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        if (trackVu_[i]) {
            float pk = player.trackPeak(i);
            trackVu_[i]->setLevel(pk);
            trackVu_[i]->setPeakHold(pk);
        }
    }
    masterVuL_->setLevel(player.masterPeakL());
    masterVuR_->setLevel(player.masterPeakR());
    masterVuL_->setPeakHold(player.masterPeakL());
    masterVuR_->setPeakHold(player.masterPeakR());

    player.decayPeaks(0.82f);
    updateMuteSoloButtons();
}

void MixerPanel::updateMuteSoloButtons() {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        if (muteButtons_[i])      muteButtons_[i]->setChecked(pattern_->muted[i]);
        if (soloButtons_[i])      soloButtons_[i]->setChecked(pattern_->soloed[i]);
        if (retriggerButtons_[i]) retriggerButtons_[i]->setChecked(pattern_->trackRetrigger[i]);
    }
}

void MixerPanel::setKit(const model::Kit* kit) {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        auto* lbl = static_cast<QLabel*>(trackLabels_[i]);
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        lbl->setText(pad ? QString::fromStdString(pad->name)
                         : QString("Pad %1").arg(i + 1));
        if (fxWindows_[i]) fxWindows_[i]->setChannelName(lbl->text());
    }
}

void MixerPanel::syncFromPattern() {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        if (trackSliders_[i]) {
            trackSliders_[i]->blockSignals(true);
            trackSliders_[i]->setValue(volumeToSlider(pattern_->trackVolumes[i]));
            trackSliders_[i]->blockSignals(false);
        }
    }
    if (masterSlider_) {
        masterSlider_->blockSignals(true);
        masterSlider_->setValue(volumeToSlider(pattern_->masterVolume));
        masterSlider_->blockSignals(false);
    }
    updateMuteSoloButtons();
}

void MixerPanel::resetAll() {
    for (auto*& w : fxWindows_) { delete w; w = nullptr; }
    for (auto* b : fxButtons_) if (b) b->setChecked(false);
    if (masterFxBtn_) masterFxBtn_->setChecked(false);

    for (auto* s : trackSliders_) if (s) s->setValue(ZERO_DB_POS);
    if (masterSlider_) masterSlider_->setValue(ZERO_DB_POS);

    for (int i = 0; i < seq::MAX_PADS; ++i) pattern_->setTrackVolume(i, 1.0f);
    pattern_->setMasterVolume(1.0f);
    audio::Player::instance().setMasterVolume(1.0f);

    pattern_->muted.fill(false);
    pattern_->soloed.fill(false);
    pattern_->trackRetrigger.fill(true);
    updateMuteSoloButtons();

    auto resetChain = [](audio::EffectChain& chain) {
        chain.sat().setEnabled(false);
        chain.eq().setEnabled(false);
        chain.reverb().setEnabled(false);
        chain.delay().setEnabled(false);
        chain.sat().setDrive(0.f);
        chain.sat().setMix(1.f);
        for (int b = 0; b < 5; ++b) chain.eq().setBandGain(b, 0.f);
        chain.reverb().setRoomSize(0.5f); chain.reverb().setDamping(0.5f); chain.reverb().setWet(0.3f);
        chain.delay().setTimeMs(250.f);   chain.delay().setFeedback(0.4f); chain.delay().setMix(0.4f);
        chain.reset();
    };

    for (int i = 0; i < seq::MAX_PADS; ++i) resetChain(audio::Player::instance().trackChain(i));
    resetChain(audio::Player::instance().masterChain());
}

} // namespace wako::ui