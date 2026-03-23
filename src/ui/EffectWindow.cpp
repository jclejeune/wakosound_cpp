#include "EffectWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QFrame>
#include <QPushButton>

namespace wako::ui {

static const QString FX_STYLE = R"(
QWidget {
    background: #1A1A1C;
    color: #CCCCCC;
    font-size: 11px;
}
QWidget#sectionHeader {
    background: #242428;
    border-radius: 3px;
}
QFrame#sectionBody {
    background: #1E1E21;
    border: 1px solid #2E2E32;
    border-radius: 3px;
}
QCheckBox {
    color: #AAAAAA;
    font-size: 11px;
    font-weight: bold;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #555; border-radius: 3px; background: #2A2A2E;
}
QCheckBox::indicator:checked { background: #3A7ABF; border-color: #5A9ADF; }
QSlider::groove:horizontal { background:#333; height:4px; border-radius:2px; }
QSlider::handle:horizontal {
    background:#6699CC; border:1px solid #4477AA;
    width:12px; height:12px; margin:-4px 0; border-radius:6px;
}
QSlider::handle:horizontal:hover { background:#88BBEE; }
QSlider::sub-page:horizontal     { background:#4477AA; border-radius:2px; }
QSlider:disabled::groove:horizontal   { background:#282828; }
QSlider:disabled::handle:horizontal   { background:#3A3A3A; border-color:#2A2A2A; }
QSlider:disabled::sub-page:horizontal { background:#2A2A2A; }
QLabel#paramLabel          { color:#666; font-size:9px; }
QLabel#paramLabel:disabled { color:#444; }
QLabel#paramValue          { color:#BBBBBB; font-size:10px; min-width:40px; }
QLabel#paramValue:disabled { color:#444; }
QPushButton#modeBtn {
    background:#252528; color:#777;
    border:1px solid #3A3A3E; border-radius:3px;
    font-size:9px; font-weight:bold; padding:2px 6px;
}
QPushButton#modeBtn:checked {
    background:#2A4A2A; color:#66BB66; border-color:#448844;
}
QPushButton#modeBtn:disabled { color:#3A3A3A; border-color:#2A2A2A; }
)";

// ──────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────
struct Section {
    QCheckBox* check = nullptr;
    QFrame*    body  = nullptr;
};

static Section buildSection(const QString& title, bool startEnabled, QWidget* parent) {
    auto* outer = new QVBoxLayout(parent);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("sectionHeader");
    hdr->setFixedHeight(26);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(8, 0, 8, 0);
    auto* cb = new QCheckBox(title);
    cb->setChecked(startEnabled);
    hdrLay->addWidget(cb);
    outer->addWidget(hdr);

    auto* body = new QFrame;
    body->setObjectName("sectionBody");
    body->setEnabled(startEnabled);
    outer->addWidget(body);

    QObject::connect(cb, &QCheckBox::toggled, body, &QWidget::setEnabled);
    return {cb, body};
}

static QSlider* addParamRow(QGridLayout* g, int row,
                             const QString& label,
                             int min, int max, int val,
                             QLabel*& valLbl) {
    auto* lbl = new QLabel(label);
    lbl->setObjectName("paramLabel");
    auto* sl = new QSlider(Qt::Horizontal);
    sl->setRange(min, max);
    sl->setValue(val);
    sl->setFixedHeight(18);
    valLbl = new QLabel;
    valLbl->setObjectName("paramValue");
    valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valLbl->setFixedWidth(42);
    g->addWidget(lbl,    row, 0);
    g->addWidget(sl,     row, 1);
    g->addWidget(valLbl, row, 2);
    return sl;
}

// ──────────────────────────────────────────────────────────────────
// EffectWindow
// ──────────────────────────────────────────────────────────────────
EffectWindow::EffectWindow(audio::EffectChain& chain,
                           const QString& channelName,
                           QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool), chain_(chain)
{
    setWindowTitle("FX — " + channelName);
    setStyleSheet(FX_STYLE);
    setFixedWidth(340);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    buildSat(root);
    buildEQ(root);
    buildReverb(root);
    buildDelay(root);
    root->addStretch();
    adjustSize();
}

void EffectWindow::setChannelName(const QString& name) {
    setWindowTitle("FX — " + name);
}

// ── EQ5 ───────────────────────────────────────────────────────────
void EffectWindow::buildEQ(QVBoxLayout* root) {
    auto& eq = chain_.eq();
    auto* container = new QWidget;
    auto [cb, body] = buildSection("EQ 5 bandes", eq.enabled(), container);
    root->addWidget(container);

    auto* lay  = new QVBoxLayout(body);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(3);
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    const char* names[5] = {"80 Hz", "250 Hz", "1 kHz", "4 kHz", "12 kHz"};
    for (int b = 0; b < 5; ++b) {
        QLabel* vl = nullptr;
        auto* sl = addParamRow(grid, b, names[b], -120, 120,
                               int(eq.getBandGain(b) * 10.f), vl);
        vl->setText(QString::asprintf("%+.1f", eq.getBandGain(b)));
        connect(sl, &QSlider::valueChanged, this, [this, b, vl](int v) {
            float g = v / 10.f;
            chain_.eq().setBandGain(b, g);
            vl->setText(QString::asprintf("%+.1f", g));
        });
    }
    lay->addLayout(grid);

    connect(cb, &QCheckBox::toggled, this, [this](bool v) {
        chain_.eq().setEnabled(v);
    });
}

// ── Distortion────────────────────────────────────────────────────
void EffectWindow::buildSat(QVBoxLayout* root) {
    auto& sat = chain_.sat();
    auto* container = new QWidget;
    auto [cb, body] = buildSection("Saturation", sat.enabled(), container);
    root->addWidget(container);

    auto* lay = new QVBoxLayout(body);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(4);

    // ── Sélecteur de mode ─────────────────────────────────────────
    using Mode = audio::Saturator::Mode;

    const Mode  modeValues[3] = { Mode::Tube, Mode::Transistor, Mode::Fuzz };
    const char* modeLabels[3] = { "Tube", "Transistor", "Fuzz" };
    const char* modeTips[3]   = {
        "Overdrive doux atan asymétrique",
        "Hard clip transistor, style TB-303 / Roland 808",
        "Rectification + hard clip, style Big Muff"
    };

    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(4);

    // Étape 1 : créer TOUS les boutons avant de connecter
    std::array<QPushButton*, 3> modeBtns{};
    for (int i = 0; i < 3; ++i) {
        auto* btn = new QPushButton(modeLabels[i]);
        btn->setObjectName("modeBtn");
        btn->setCheckable(true);
        btn->setChecked(sat.getMode() == modeValues[i]);
        btn->setToolTip(modeTips[i]);
        modeBtns[i] = btn;
        modeRow->addWidget(btn);
    }

    // Étape 2 : connecter après — modeBtns est complet, tous les pointeurs valides
    for (int i = 0; i < 3; ++i) {
        Mode capturedMode = modeValues[i];
        connect(modeBtns[i], &QPushButton::clicked, this,
                [this, modeBtns, i, capturedMode](bool) {
            chain_.sat().setMode(capturedMode);
            for (int j = 0; j < 3; ++j)
                modeBtns[j]->setChecked(j == i);
        });
    }
    lay->addLayout(modeRow);

    // ── Sliders ───────────────────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    QLabel *driveV = nullptr, *mixV = nullptr;
    auto* driveSl = addParamRow(grid, 0, "Drive", 0, 100,
                                int(sat.getDrive() * 100.f), driveV);
    auto* mixSl   = addParamRow(grid, 1, "Mix",   0, 100,
                                int(sat.getMix()   * 100.f), mixV);

    driveV->setText(QString::number(int(sat.getDrive() * 100)) + "%");
    mixV  ->setText(QString::number(int(sat.getMix()   * 100)) + "%");

    connect(driveSl, &QSlider::valueChanged, this, [this, driveV](int v) {
        chain_.sat().setDrive(v / 100.f);
        driveV->setText(QString::number(v) + "%");
    });
    connect(mixSl, &QSlider::valueChanged, this, [this, mixV](int v) {
        chain_.sat().setMix(v / 100.f);
        mixV->setText(QString::number(v) + "%");
    });
    lay->addLayout(grid);

    connect(cb, &QCheckBox::toggled, this, [this](bool v) {
        chain_.sat().setEnabled(v);
    });
}

// ── Reverb ────────────────────────────────────────────────────────
void EffectWindow::buildReverb(QVBoxLayout* root) {
    auto& rev = chain_.reverb();
    auto* container = new QWidget;
    auto [cb, body] = buildSection("Reverb", rev.enabled(), container);
    root->addWidget(container);

    auto* lay  = new QVBoxLayout(body);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(3);
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    QLabel *roomV = nullptr, *dampV = nullptr, *wetV = nullptr;
    auto* roomSl = addParamRow(grid, 0, "Room", 0, 100,
                               int(rev.getRoomSize() * 100.f), roomV);
    auto* dampSl = addParamRow(grid, 1, "Damp", 0, 100,
                               int(rev.getDamping()  * 100.f), dampV);
    auto* wetSl  = addParamRow(grid, 2, "Wet",  0, 100,
                               int(rev.getWet()      * 100.f), wetV);

    roomV->setText(QString::number(int(rev.getRoomSize() * 100)) + "%");
    dampV->setText(QString::number(int(rev.getDamping()  * 100)) + "%");
    wetV ->setText(QString::number(int(rev.getWet()      * 100)) + "%");

    connect(roomSl, &QSlider::valueChanged, this, [this, roomV](int v) {
        chain_.reverb().setRoomSize(v / 100.f);
        roomV->setText(QString::number(v) + "%");
    });
    connect(dampSl, &QSlider::valueChanged, this, [this, dampV](int v) {
        chain_.reverb().setDamping(v / 100.f);
        dampV->setText(QString::number(v) + "%");
    });
    connect(wetSl, &QSlider::valueChanged, this, [this, wetV](int v) {
        chain_.reverb().setWet(v / 100.f);
        wetV->setText(QString::number(v) + "%");
    });
    lay->addLayout(grid);

    connect(cb, &QCheckBox::toggled, this, [this](bool v) {
        chain_.reverb().setEnabled(v);
    });
}

// ── Delay ─────────────────────────────────────────────────────────
void EffectWindow::buildDelay(QVBoxLayout* root) {
    auto& del = chain_.delay();
    auto* container = new QWidget;
    auto [cb, body] = buildSection("Delay", del.enabled(), container);
    root->addWidget(container);

    auto* lay  = new QVBoxLayout(body);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(3);
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    QLabel *timeV = nullptr, *fbV = nullptr, *mixV = nullptr;
    auto* timeSl = addParamRow(grid, 0, "Time ms",  0, 2000,
                               int(del.getTimeMs()),           timeV);
    auto* fbSl   = addParamRow(grid, 1, "Feedback", 0, 95,
                               int(del.getFeedback() * 100.f), fbV);
    auto* mixSl  = addParamRow(grid, 2, "Mix",      0, 100,
                               int(del.getMix()      * 100.f), mixV);

    timeV->setText(QString::number(int(del.getTimeMs()))         + "ms");
    fbV  ->setText(QString::number(int(del.getFeedback() * 100)) + "%");
    mixV ->setText(QString::number(int(del.getMix()      * 100)) + "%");

    connect(timeSl, &QSlider::valueChanged, this, [this, timeV](int v) {
        chain_.delay().setTimeMs(float(v));
        timeV->setText(QString::number(v) + "ms");
    });
    connect(fbSl, &QSlider::valueChanged, this, [this, fbV](int v) {
        chain_.delay().setFeedback(v / 100.f);
        fbV->setText(QString::number(v) + "%");
    });
    connect(mixSl, &QSlider::valueChanged, this, [this, mixV](int v) {
        chain_.delay().setMix(v / 100.f);
        mixV->setText(QString::number(v) + "%");
    });
    lay->addLayout(grid);

    connect(cb, &QCheckBox::toggled, this, [this](bool v) {
        chain_.delay().setEnabled(v);
    });
}

} // namespace wako::ui