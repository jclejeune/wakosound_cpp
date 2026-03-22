#include "RenderPanel.h"
#include "SvgIcons.h"
#include "../audio/Player.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QToolButton>
#include <QStyle>
#include <QDateTime>
#include <cmath>

namespace wako::ui {

static constexpr int SAMPLE_RATE    = 44100;
static constexpr int PROGRESS_TICK  = 50;   // ms

static const QString RENDER_STYLE = R"(
QWidget { background: #1E1E20; color: #CCCCCC; font-size: 12px; }
QLabel#sectionTitle {
    color: #888; font-size: 10px; font-weight: bold;
    letter-spacing: 1px; padding: 8px 0 2px 0;
}
QLineEdit {
    background: #2A2A2E; color: #DCDCDC;
    border: 1px solid #3A3A3E; border-radius: 3px;
    padding: 4px 8px; font-size: 12px;
}
QSpinBox {
    background: #2A2A2E; color: #DCDCDC;
    border: 1px solid #3A3A3E; border-radius: 3px;
    padding: 4px 8px; font-size: 12px;
    min-width: 60px;
}
QLabel#infoLabel  { color: #888; font-size: 11px; }
QLabel#durationLabel { color: #AAAAAA; font-size: 13px; font-weight: bold; }
QProgressBar {
    background: #2A2A2E; border: 1px solid #3A3A3E;
    border-radius: 3px; height: 16px;
    text-align: center; color: #DCDCDC; font-size: 10px;
}
QProgressBar::chunk { background: #3A7ABF; border-radius: 2px; }
QPushButton#exportBtn {
    background: #2A5A2A; color: #88EE88;
    border: 1px solid #3A8A3A; border-radius: 4px;
    font-size: 13px; font-weight: bold; padding: 8px 32px;
}
QPushButton#exportBtn:hover   { background: #3A6A3A; }
QPushButton#exportBtn:pressed { background: #1A4A1A; }
QPushButton#exportBtn:disabled { background:#252525; color:#555; border-color:#333; }
QToolButton {
    background: #2A2A2E; border: 1px solid #3A3A3E;
    border-radius: 3px; padding: 4px 8px;
}
QToolButton:hover { background: #3A3A3E; }
QLabel#statusOk   { color: #66CC66; font-size: 13px; font-weight: bold; }
QLabel#statusClip { color: #CC9944; font-size: 13px; font-weight: bold; }
QLabel#statusError{ color: #CC4444; font-size: 13px; font-weight: bold; }
QLabel#hint {
    color: #555; font-size: 10px;
    qproperty-alignment: AlignCenter;
    padding: 8px;
}
)";

RenderPanel::RenderPanel(std::shared_ptr<seq::Pattern>      pattern,
                         std::shared_ptr<model::KitManager> kitManager,
                         QWidget* parent)
    : QWidget(parent)
    , pattern_(std::move(pattern))
    , kitManager_(std::move(kitManager))
{
    setStyleSheet(RENDER_STYLE);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 24, 32, 24);
    root->setSpacing(6);

    // ── Titre ─────────────────────────────────────────────────────
    auto* title = new QLabel("RENDER");
    title->setObjectName("sectionTitle");
    root->addWidget(title);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { background: #2E2E32; max-height:1px; }");
    root->addWidget(sep);
    root->addSpacing(8);

    // // ── Hint ──────────────────────────────────────────────────────
    // auto* hint = new QLabel(
    //     "Le render enregistre exactement ce que tu entends :\n"
    //     "effets, volume, tout compris.");
    // hint->setObjectName("hint");
    // root->addWidget(hint);
    root->addSpacing(8);

    // ── Paramètres ────────────────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);

    // Fichier
    auto* fileLbl = new QLabel("Fichier WAV");
    fileLbl->setObjectName("infoLabel");
    fileEdit_ = new QLineEdit("render.wav");
    auto* browseBtn = new QToolButton;
    browseBtn->setIcon(icons::icon(icons::FOLDER_OPEN, 14, "#AAAAAA"));
    browseBtn->setFixedSize(28, 28);
    connect(browseBtn, &QToolButton::clicked, this, &RenderPanel::onBrowse);

    auto* fileRow = new QHBoxLayout;
    fileRow->setSpacing(6);
    fileRow->addWidget(fileEdit_, 1);
    fileRow->addWidget(browseBtn);

    grid->addWidget(fileLbl, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(fileRow, 0, 1);

    // Boucles
    auto* loopsLbl = new QLabel("Boucles");
    loopsLbl->setObjectName("infoLabel");
    loopsSpin_ = new QSpinBox;
    loopsSpin_->setRange(1, 64);
    loopsSpin_->setValue(2);
    connect(loopsSpin_, &QSpinBox::valueChanged,
            this, &RenderPanel::updateDurationLabel);

    grid->addWidget(loopsLbl,  1, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(loopsSpin_,1, 1, Qt::AlignLeft);

    // Durée
    auto* durLbl = new QLabel("Durée estimée");
    durLbl->setObjectName("infoLabel");
    durationLbl_ = new QLabel("—");
    durationLbl_->setObjectName("durationLabel");

    grid->addWidget(durLbl,      2, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(durationLbl_,2, 1);

    root->addLayout(grid);
    root->addSpacing(16);

    // ── Progress bar ──────────────────────────────────────────────
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setFormat("%p%");
    progressBar_->setFixedHeight(18);
    root->addWidget(progressBar_);
    root->addSpacing(16);

    // ── Bouton + status ───────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);

    exportBtn_ = new QPushButton("  Exporter  ");
    exportBtn_->setObjectName("exportBtn");
    exportBtn_->setFixedHeight(38);
    connect(exportBtn_, &QPushButton::clicked, this, &RenderPanel::onExport);

    statusLbl_ = new QLabel("");
    statusLbl_->setObjectName("statusOk");

    btnRow->addStretch();
    btnRow->addWidget(exportBtn_);
    btnRow->addWidget(statusLbl_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    root->addStretch();

    // ── Timers ────────────────────────────────────────────────────
    recordTimer_ = new QTimer(this);
    recordTimer_->setSingleShot(true);
    connect(recordTimer_, &QTimer::timeout,
            this, &RenderPanel::onRecordingFinished);

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(PROGRESS_TICK);
    connect(progressTimer_, &QTimer::timeout,
            this, &RenderPanel::onProgressTick);

    updateDurationLabel();
}

// ──────────────────────────────────────────────────────────────────
void RenderPanel::onBrowse() {
    QString path = QFileDialog::getSaveFileName(
        this, "Choisir le fichier de sortie",
        fileEdit_->text().isEmpty() ? "render.wav" : fileEdit_->text(),
        "Fichier WAV (*.wav)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".wav", Qt::CaseInsensitive))
            path += ".wav";
        fileEdit_->setText(path);
    }
}

void RenderPanel::updateDurationLabel() {
    int loops     = loopsSpin_->value();
    int steps     = pattern_->patternLength * loops;
    int bpm       = pattern_->bpm;
    int msPerStep = seq::Pattern::stepIntervalMs(bpm);
    double totalSec = static_cast<double>(steps * msPerStep) / 1000.0;

    int    minutes = static_cast<int>(totalSec) / 60;
    double secs    = totalSec - minutes * 60;

    QString txt;
    if (minutes > 0)
        txt = QString("%1 min %2 s").arg(minutes).arg(secs, 0, 'f', 1);
    else
        txt = QString("%1 s").arg(secs, 0, 'f', 1);

    txt += QString("  ·  %1 steps  ·  %2 BPM").arg(steps).arg(bpm);
    durationLbl_->setText(txt);
}

// ──────────────────────────────────────────────────────────────────
void RenderPanel::onExport() {
    if (rendering_) return;

    QString path = fileEdit_->text().trimmed();
    if (path.isEmpty()) path = "render.wav";
    if (!path.endsWith(".wav", Qt::CaseInsensitive)) path += ".wav";

    int loops     = loopsSpin_->value();
    int steps     = pattern_->patternLength * loops;
    int msPerStep = seq::Pattern::stepIntervalMs(pattern_->bpm);
    totalDurationMs_ = steps * msPerStep;

    // Petite marge de sécurité (+1 step) pour capturer la queue des samples
    int extraMs    = msPerStep * 4;
    int totalMs    = totalDurationMs_ + extraMs;
    int totalFrames= static_cast<int>(
                         static_cast<double>(SAMPLE_RATE) * totalMs / 1000.0);

    rendering_ = true;
    exportBtn_->setEnabled(false);
    statusLbl_->setText("⏺ Enregistrement...");
    statusLbl_->setObjectName("statusOk");
    statusLbl_->style()->unpolish(statusLbl_);
    statusLbl_->style()->polish(statusLbl_);
    progressBar_->setValue(0);

    // 1. Pré-allouer le buffer d'enregistrement
    audio::Player::instance().startRecording(totalFrames);

    // 2. Lancer le séquenceur via MainWindow
    emit sequencerStartRequested();

    // 3. Stopper après la durée du pattern (sans la marge)
    recordStartMs_ = QDateTime::currentMSecsSinceEpoch();
    recordTimer_->start(totalDurationMs_);
    progressTimer_->start();

    // Stocker le path pour l'utiliser dans onRecordingFinished
    fileEdit_->setProperty("pendingPath", path);
}

void RenderPanel::onProgressTick() {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - recordStartMs_;
    int pct = static_cast<int>(
                  std::min(100.0, elapsed * 100.0 / totalDurationMs_));
    progressBar_->setValue(pct);
}

void RenderPanel::onRecordingFinished() {
    progressTimer_->stop();
    progressBar_->setValue(100);

    // Stopper le séquenceur
    emit sequencerStopRequested();

    // Laisser quelques ms pour que les derniers samples se terminent
    // puis écrire le WAV
    QTimer::singleShot(200, this, [this]() {
        QString path = fileEdit_->property("pendingPath").toString();
        bool ok = audio::Player::instance().stopRecording(path.toStdString());

        rendering_ = false;
        exportBtn_->setEnabled(true);
        setStatus(ok);
    });
}

void RenderPanel::setStatus(bool ok, bool /*clipping*/, const QString& msg) {
    if (!ok) {
        statusLbl_->setObjectName("statusError");
        statusLbl_->setText("✗  Erreur" + (msg.isEmpty() ? "" : " : " + msg));
    } else {
        statusLbl_->setObjectName("statusOk");
        statusLbl_->setText("✓  Exporté");
    }
    statusLbl_->style()->unpolish(statusLbl_);
    statusLbl_->style()->polish(statusLbl_);
}

} // namespace wako::ui