#include "MainWindow.h"
#include "PadGrid.h"
#include "StepGrid.h"
#include "MixerPanel.h"
#include "RenderPanel.h"
#include "TransportBar.h"
#include "SampleBrowser.h"
#include "SvgIcons.h"
#include "../audio/AudioCache.h"
#include "../audio/Player.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMetaObject>
#include <QFileDialog>
#include <QMessageBox>
#include <iostream>

// ──────────────────────────────────────────────────────────────────
// Mapping clavier AZERTY → pads
//
//  A Z E      pad 0 1 2   (ligne haute)
//  Q S D      pad 3 4 5   (ligne milieu)
//  W X C      pad 6 7 8   (ligne basse)
// ──────────────────────────────────────────────────────────────────
static const Qt::Key PAD_KEYS[] = {
    Qt::Key_A, Qt::Key_Z, Qt::Key_E,
    Qt::Key_Q, Qt::Key_S, Qt::Key_D,
    Qt::Key_W, Qt::Key_X, Qt::Key_C
};

static constexpr const char* MIXER_SVG = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10
    16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14
    4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"/>
</svg>)";

static constexpr const char* RENDER_SVG = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z"/>
</svg>)";

namespace wako::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WakoSound");
    resize(1020, 640);

    kitManager_ = std::make_shared<model::KitManager>();
    pattern_    = std::make_shared<seq::Pattern>();
    engine_     = std::make_unique<seq::Engine>();

    if (!kitManager_->loadFactory("kits.json"))
        std::cerr << "[MainWindow] kits.json introuvable\n";

    audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());

    auto* central    = new QWidget(this);
    setCentralWidget(central);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    transportBar_ = new TransportBar(this);
    rootLayout->addWidget(transportBar_);
    refreshKitCombo();

    splitter_ = new QSplitter(Qt::Horizontal, central);
    splitter_->setHandleWidth(3);
    splitter_->setStyleSheet(
        "QSplitter::handle { background: #2A2A2D; }"
        "QSplitter::handle:hover { background: #4A6A9A; }");

    sampleBrowser_ = new SampleBrowser(splitter_);
    sampleBrowser_->setMinimumWidth(SIDEBAR_MIN);
    sampleBrowser_->setMaximumWidth(SIDEBAR_MAX);
    splitter_->addWidget(sampleBrowser_);

    padGrid_     = new PadGrid(splitter_);
    stepGrid_    = new StepGrid(pattern_, splitter_);
    mixerPanel_  = new MixerPanel(pattern_, splitter_);
    renderPanel_ = new RenderPanel(pattern_, kitManager_, splitter_);

    stepGrid_->setKit(kitManager_->currentKit());
    mixerPanel_->setKit(kitManager_->currentKit());

    auto* samplerTab = new QWidget;
    auto* samplerLay = new QVBoxLayout(samplerTab);
    samplerLay->setContentsMargins(0, 0, 0, 0);
    samplerLay->addWidget(padGrid_, 1);

    auto* seqTab = new QWidget;
    auto* seqLay = new QVBoxLayout(seqTab);
    seqLay->setContentsMargins(0, 0, 0, 0);
    seqLay->addWidget(stepGrid_, 1);

    tabs_ = new QTabWidget(splitter_);
    tabs_->setStyleSheet("QTabBar::tab { padding: 6px 14px; font-size: 12px; }");
    tabs_->addTab(samplerTab,   icons::icon(icons::GRID,       14), "Sampler");
    tabs_->addTab(seqTab,       icons::icon(icons::MUSIC_NOTE, 14), "Séquenceur");
    tabs_->addTab(mixerPanel_,  icons::icon(MIXER_SVG,         14), "Mixage");
    tabs_->addTab(renderPanel_, icons::icon(RENDER_SVG,        14), "Render");
    splitter_->addWidget(tabs_);

    splitter_->setCollapsible(0, false);
    splitter_->setCollapsible(1, false);
    splitter_->setSizes({SIDEBAR_DEFAULT, 1020 - SIDEBAR_DEFAULT});

    rootLayout->addWidget(splitter_, 1);

    padGrid_->refresh(kitManager_->currentKit());

    // ── Connexions ────────────────────────────────────────────────
    connect(transportBar_, &TransportBar::kitChanged, this, [this](int idx) {
        kitManager_->switchTo(idx);
        audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());
        padGrid_->refresh(kitManager_->currentKit());
        stepGrid_->setKit(kitManager_->currentKit());
        mixerPanel_->setKit(kitManager_->currentKit());
        setFocus();
    });

    connect(padGrid_, &PadGrid::padTriggered, this, [this](int idx) {
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        const auto* pad = kit->pad(idx);
        if (pad && pad->enabled && !pad->filePath.empty())
            audio::Player::instance().play(pad->filePath, pad->volume, 0, false, idx);
    });

    connect(transportBar_, &TransportBar::playStopClicked, this, &MainWindow::onPlayStop);
    connect(transportBar_, &TransportBar::clearClicked,    this, &MainWindow::onClear);
    connect(transportBar_, &TransportBar::bpmChanged,      this, &MainWindow::onBpmChanged);
    connect(transportBar_, &TransportBar::lengthChanged,   this, &MainWindow::onLengthChanged);

    connect(transportBar_, &TransportBar::saveClicked, this, [this] {
        QString path = QFileDialog::getSaveFileName(
            this, "Sauvegarder le pattern", "pattern.json", "Pattern JSON (*.json)");
        if (path.isEmpty()) { setFocus(); return; }
        if (!pattern_->saveToFile(path.toStdString()))
            QMessageBox::warning(this, "Erreur", "Impossible de sauvegarder.");
        setFocus();
    });

    connect(transportBar_, &TransportBar::loadClicked, this, [this] {
        QString path = QFileDialog::getOpenFileName(
            this, "Charger un pattern", "", "Pattern JSON (*.json)");
        if (path.isEmpty()) { setFocus(); return; }
        auto loaded = seq::Pattern::loadFromFile(path.toStdString());
        if (!loaded) {
            QMessageBox::warning(this, "Erreur", "Fichier invalide.");
            setFocus();
            return;
        }
        *pattern_ = *loaded;
        stepGrid_->updatePattern(pattern_.get());
        audio::Player::instance().setMasterVolume(pattern_->masterVolume);
        setFocus();
    });

    connect(stepGrid_, &StepGrid::stepToggled, this, [this](int pad, int step) {
        pattern_->toggle(pad, step);
        stepGrid_->updatePattern(pattern_.get());
    });
    connect(stepGrid_, &StepGrid::stepGateToggled,    this, [this](int,int)               { stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::stepDataChanged,    this, [this](int,int,seq::StepData) { stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackGateToggled,   this, [this](int pad) { pattern_->toggleTrackGate(pad); stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackLengthChanged, this, [](int,int){});

    connect(stepGrid_, &StepGrid::trackMuteToggled, this, [this](int pad) {
        pattern_->toggleMute(pad); stepGrid_->update();
    });
    connect(stepGrid_, &StepGrid::trackSoloToggled, this, [this](int pad) {
        pattern_->toggleSolo(pad); stepGrid_->update();
    });
    connect(mixerPanel_, &MixerPanel::trackMuteToggled, this, [this](int pad) {
        pattern_->toggleMute(pad); stepGrid_->update();
    });
    connect(mixerPanel_, &MixerPanel::trackSoloToggled, this, [this](int pad) {
        pattern_->toggleSolo(pad); stepGrid_->update();
    });

    connect(sampleBrowser_, &SampleBrowser::samplePreviewRequested,
            this, [this](const QString& path) {
                audio::Player::instance().play(path.toStdString(), 1.0f);
            });

    connect(padGrid_, &PadGrid::padFileDropped, this, &MainWindow::onPadFileDropped);

    connect(renderPanel_, &RenderPanel::sequencerStartRequested, this, &MainWindow::onRenderStart);
    connect(renderPanel_, &RenderPanel::sequencerStopRequested,  this, &MainWindow::onRenderStop);
}

MainWindow::~MainWindow() {
    if (engine_) engine_->stop();
}

// ──────────────────────────────────────────────────────────────────
void MainWindow::refreshKitCombo() {
    QStringList names;
    for (const auto& k : kitManager_->kits())
        names << QString::fromStdString(k.name);
    transportBar_->setKits(names, kitManager_->currentIndex());
}

void MainWindow::onPlayStop() {
    if (engine_->isRunning()) {
        stopSequencer();
    } else {
        pattern_->trackSteps.fill(0);
        engine_->start(pattern_, kitManager_,
            [this](const seq::TrackSteps& steps) { onSequencerStep(steps); });
        transportBar_->setPlaying(true);
    }
}

void MainWindow::stopSequencer() {
    engine_->stop();
    pattern_->trackSteps.fill(0);
    transportBar_->setPlaying(false);
    transportBar_->setStep(0);
    stepGrid_->setCurrentStep(-1);
}

void MainWindow::onRenderStart() {
    if (engine_->isRunning()) engine_->stop();
    pattern_->trackSteps.fill(0);
    engine_->start(pattern_, kitManager_,
        [this](const seq::TrackSteps& steps) { onSequencerStep(steps); });
    transportBar_->setPlaying(true);
}

void MainWindow::onRenderStop() {
    stopSequencer();
}

void MainWindow::onClear() {
    if (engine_->isRunning()) {
        engine_->stop();
        transportBar_->setPlaying(false);
    }
    pattern_->clearAll();
    pattern_->setLength(16);
    pattern_->trackSteps.fill(0);
    stepGrid_->setCurrentStep(-1);
    stepGrid_->updatePattern(pattern_.get());
    transportBar_->setStep(0);
    mixerPanel_->resetAll();
    setFocus();
}

void MainWindow::onBpmChanged(int bpm)    { pattern_->setBpm(bpm); }
void MainWindow::onLengthChanged(int len) {
    pattern_->setLength(len);
    stepGrid_->updatePattern(pattern_.get());
}

void MainWindow::onPadFileDropped(int padIdx, const QString& filePath) {
    const auto* cur = kitManager_->currentKit();
    if (cur && cur->isFactory) {
        model::Kit copy = *cur;
        copy.name      += " (custom)";
        copy.id         = "";
        copy.isFactory  = false;
        int idx = kitManager_->upsertUserKit(std::move(copy));
        kitManager_->switchTo(idx);
        refreshKitCombo();
    }
    if (!kitManager_->setPadFile(padIdx, filePath.toStdString())) return;
    audio::AudioCache::instance().preload({filePath.toStdString()});
    padGrid_->refresh(kitManager_->currentKit());
    stepGrid_->setKit(kitManager_->currentKit());
    mixerPanel_->setKit(kitManager_->currentKit());
    tabs_->setCurrentIndex(0);
}

void MainWindow::onSequencerStep(const seq::TrackSteps& steps) {
    QMetaObject::invokeMethod(this, [this, steps] {
        stepGrid_->setCurrentSteps(steps);
        transportBar_->setStep(steps[0]);
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        for (int p = 0; p < seq::MAX_PADS; ++p) {
            if (!pattern_->shouldPlay(p)) continue;
            if (pattern_->grid[p][steps[p]].active)
                padGrid_->flashPad(p);
        }
    }, Qt::QueuedConnection);
}

// ──────────────────────────────────────────────────────────────────
// Navigation pas-à-pas (J/K)
// ──────────────────────────────────────────────────────────────────
void MainWindow::playCurrentStep() {
    const auto* kit = kitManager_->currentKit();
    if (!kit) return;
    auto& player = audio::Player::instance();

    for (int p = 0; p < seq::MAX_PADS; ++p) {
        if (!pattern_->shouldPlay(p)) continue;
        int step = pattern_->trackSteps[p];
        const seq::StepData& sd = pattern_->grid[p][step];
        if (!sd.active) continue;
        const model::Pad* pad = kit->pad(p);
        if (!pad || !pad->enabled || pad->filePath.empty()) continue;
        float vol = pad->volume * sd.volume * pattern_->trackVolumes[p];
        player.play(pad->filePath, vol, sd.pitch, sd.gate || pattern_->trackGate[p], p);
        padGrid_->flashPad(p);
    }

    stepGrid_->setCurrentSteps(pattern_->trackSteps);
    transportBar_->setStep(pattern_->trackSteps[0]);
}

void MainWindow::stepForward() {
    if (engine_->isRunning()) return;
    // Avancer chaque track d'un step
    for (int p = 0; p < seq::MAX_PADS; ++p)
        pattern_->trackSteps[p] = (pattern_->trackSteps[p] + 1) % pattern_->trackLengths[p];
    playCurrentStep();
}

void MainWindow::stepBackward() {
    if (engine_->isRunning()) return;
    // Reculer chaque track d'un step
    for (int p = 0; p < seq::MAX_PADS; ++p) {
        pattern_->trackSteps[p] =
            (pattern_->trackSteps[p] - 1 + pattern_->trackLengths[p])
            % pattern_->trackLengths[p];
    }
    playCurrentStep();
}

// ──────────────────────────────────────────────────────────────────
// keyPressEvent
// ──────────────────────────────────────────────────────────────────
void MainWindow::keyPressEvent(QKeyEvent* event) {
    // Ignorer les répétitions auto (touche maintenue)
    if (event->isAutoRepeat()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    const Qt::Key key = static_cast<Qt::Key>(event->key());

    // ── Space / L → Play/Stop ─────────────────────────────────────
    if (key == Qt::Key_Space || key == Qt::Key_L) {
        onPlayStop();
        return;
    }

    // ── K → step forward ─────────────────────────────────────────
    if (key == Qt::Key_K) {
        stepForward();
        return;
    }

    // ── J → step backward ────────────────────────────────────────
    if (key == Qt::Key_J) {
        stepBackward();
        return;
    }

    // ── A Z E / Q S D / W X C → pads ─────────────────────────────
    for (int i = 0; i < 9; ++i) {
        if (key == PAD_KEYS[i]) {
            emit padGrid_->padTriggered(i);
            padGrid_->flashPad(i);
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

} // namespace wako::ui