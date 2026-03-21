#include "MainWindow.h"
#include "PadGrid.h"
#include "StepGrid.h"
#include "MixerPanel.h"
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

static const int NUMPAD_KEYS[] = {
    Qt::Key_7, Qt::Key_8, Qt::Key_9,
    Qt::Key_4, Qt::Key_5, Qt::Key_6,
    Qt::Key_1, Qt::Key_2, Qt::Key_3
};

// Icône console de mixage
static constexpr const char* MIXER_SVG = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10
    16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14
    4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"/>
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

    // ── Layout racine ─────────────────────────────────────────────
    auto* central    = new QWidget(this);
    setCentralWidget(central);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    transportBar_ = new TransportBar(this);
    rootLayout->addWidget(transportBar_);
    refreshKitCombo();

    // ── Splitter : sidebar | contenu ─────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal, central);
    splitter_->setHandleWidth(3);
    splitter_->setStyleSheet(
        "QSplitter::handle { background: #2A2A2D; }"
        "QSplitter::handle:hover { background: #4A6A9A; }");

    sampleBrowser_ = new SampleBrowser(splitter_);
    sampleBrowser_->setMinimumWidth(SIDEBAR_MIN);
    sampleBrowser_->setMaximumWidth(SIDEBAR_MAX);
    splitter_->addWidget(sampleBrowser_);

    // ── Tabs ──────────────────────────────────────────────────────
    padGrid_    = new PadGrid(splitter_);
    stepGrid_   = new StepGrid(pattern_, splitter_);
    mixerPanel_ = new MixerPanel(pattern_, splitter_);

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
    tabs_->addTab(samplerTab,  icons::icon(icons::GRID,       14), "Sampler");
    tabs_->addTab(seqTab,      icons::icon(icons::MUSIC_NOTE, 14), "Séquenceur");
    tabs_->addTab(mixerPanel_, icons::icon(MIXER_SVG,         14), "Mixage");
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
    });

    connect(padGrid_, &PadGrid::padTriggered, this, [this](int idx) {
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        const auto* pad = kit->pad(idx);
        if (pad && pad->enabled && !pad->filePath.empty())
            audio::Player::instance().play(pad->filePath, pad->volume,
                                           0, false, idx);
    });

    connect(transportBar_, &TransportBar::playStopClicked, this, &MainWindow::onPlayStop);
    connect(transportBar_, &TransportBar::clearClicked,    this, &MainWindow::onClear);
    connect(transportBar_, &TransportBar::bpmChanged,      this, &MainWindow::onBpmChanged);
    connect(transportBar_, &TransportBar::lengthChanged,   this, &MainWindow::onLengthChanged);

    connect(transportBar_, &TransportBar::saveClicked, this, [this] {
        QString path = QFileDialog::getSaveFileName(
            this, "Sauvegarder le pattern", "pattern.json", "Pattern JSON (*.json)");
        if (path.isEmpty()) return;
        if (!pattern_->saveToFile(path.toStdString()))
            QMessageBox::warning(this, "Erreur", "Impossible de sauvegarder.");
    });

    connect(transportBar_, &TransportBar::loadClicked, this, [this] {
        QString path = QFileDialog::getOpenFileName(
            this, "Charger un pattern", "", "Pattern JSON (*.json)");
        if (path.isEmpty()) return;
        auto loaded = seq::Pattern::loadFromFile(path.toStdString());
        if (!loaded) { QMessageBox::warning(this, "Erreur", "Fichier invalide."); return; }
        *pattern_ = *loaded;
        stepGrid_->updatePattern(pattern_.get());
        // Sync master volume Player ← pattern chargé
        audio::Player::instance().setMasterVolume(pattern_->masterVolume);
    });

    connect(stepGrid_, &StepGrid::stepToggled, this, [this](int pad, int step) {
        pattern_->toggle(pad, step);
        stepGrid_->updatePattern(pattern_.get());
    });
    connect(stepGrid_, &StepGrid::stepGateToggled,   this, [this](int,int)               { stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::stepDataChanged,   this, [this](int,int,seq::StepData) { stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackGateToggled,  this, [this](int pad) { pattern_->toggleTrackGate(pad); stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackMuteToggled,  this, [this](int pad) { pattern_->toggleMute(pad);      stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackSoloToggled,  this, [this](int pad) { pattern_->toggleSolo(pad);      stepGrid_->update(); });
    connect(stepGrid_, &StepGrid::trackLengthChanged, this, [](int,int){});

    connect(sampleBrowser_, &SampleBrowser::samplePreviewRequested,
            this, [this](const QString& path) {
                audio::Player::instance().play(path.toStdString(), 1.0f);
            });

    connect(padGrid_, &PadGrid::padFileDropped,
            this, &MainWindow::onPadFileDropped);
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

void MainWindow::onClear() {
    if (engine_->isRunning()) { engine_->stop(); transportBar_->setPlaying(false); }
    pattern_->clearAll();
    pattern_->setLength(16);
    pattern_->trackSteps.fill(0);
    stepGrid_->setCurrentStep(-1);
    stepGrid_->updatePattern(pattern_.get());
    transportBar_->setStep(0);
    // Resync master volume
    audio::Player::instance().setMasterVolume(1.0f);
    mixerPanel_->resetAll();
}

void MainWindow::onBpmChanged(int bpm)    { pattern_->setBpm(bpm); }
void MainWindow::onLengthChanged(int len) { pattern_->setLength(len); stepGrid_->updatePattern(pattern_.get()); }

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

void MainWindow::keyPressEvent(QKeyEvent* event) {
    for (int i = 0; i < 9; ++i) {
        if (event->key() == NUMPAD_KEYS[i]) {
            emit padGrid_->padTriggered(i);
            padGrid_->flashPad(i);
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace wako::ui