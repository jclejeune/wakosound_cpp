#include "MainWindow.h"
#include "PadGrid.h"
#include "StepGrid.h"
#include "TransportBar.h"
#include "SvgIcons.h"
#include "../audio/AudioCache.h"
#include "../audio/Player.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
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

namespace wako::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WakoSound");
    resize(760, 600);

    kitManager_ = std::make_shared<model::KitManager>();
    pattern_    = std::make_shared<seq::Pattern>();
    engine_     = std::make_unique<seq::Engine>();

    if (!kitManager_->loadFromFile("kits.json"))
        std::cerr << "[MainWindow] kits.json introuvable\n";

    audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());

    // ── Widget central ────────────────────────────────────────────
    auto* central    = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Transport bar — toujours visible en haut ──────────────────
    transportBar_ = new TransportBar(this);
    mainLayout->addWidget(transportBar_);

    // ── Kits dans la transport bar ───────────────────────────────
    QStringList kitNames;
    for (const auto& k : kitManager_->kits())
        kitNames << QString::fromStdString(k.name);
    transportBar_->setKits(kitNames, kitManager_->currentIndex());

    // ── Tabs — icônes SVG ─────────────────────────────────────────
    padGrid_  = new PadGrid(this);
    stepGrid_ = new StepGrid(pattern_, this);

    auto* samplerTab = new QWidget;
    auto* samplerLay = new QVBoxLayout(samplerTab);
    samplerLay->setContentsMargins(0, 0, 0, 0);
    samplerLay->addWidget(padGrid_, 1);

    auto* seqTab = new QWidget;
    auto* seqLay = new QVBoxLayout(seqTab);
    seqLay->setContentsMargins(0, 0, 0, 0);
    seqLay->addWidget(stepGrid_, 1);

    auto* libTab = new QWidget;
    auto* libLay = new QVBoxLayout(libTab);
    auto* libLabel = new QLabel("Bibliothèque — à venir");
    libLabel->setAlignment(Qt::AlignCenter);
    libLabel->setStyleSheet("color: #666; font-size: 14px;");
    libLay->addWidget(libLabel);

    tabs_ = new QTabWidget;
    tabs_->setStyleSheet(
        "QTabBar::tab { padding: 6px 14px; font-size: 12px; }"
    );
    tabs_->addTab(samplerTab, icons::icon(icons::GRID,       14), "Sampler");
    tabs_->addTab(seqTab,     icons::icon(icons::MUSIC_NOTE, 14), "Séquenceur");
    tabs_->addTab(libTab,     icons::icon(icons::LIBRARY,    14), "Bibliothèque");

    mainLayout->addWidget(tabs_, 1);

    // ── Connexions ────────────────────────────────────────────────
    padGrid_->refresh(kitManager_->currentKit());

    connect(transportBar_, &TransportBar::kitChanged,
            this, [this](int idx) {
                kitManager_->switchTo(idx);
                audio::AudioCache::instance().preload(
                    kitManager_->currentKitFilePaths());
                padGrid_->refresh(kitManager_->currentKit());
                stepGrid_->setKit(kitManager_->currentKit());
            });

    connect(padGrid_, &PadGrid::padTriggered, this, [this](int idx) {
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        const auto* pad = kit->pad(idx);
        if (pad && pad->enabled && !pad->filePath.empty())
            audio::Player::instance().play(pad->filePath, pad->volume);
    });

    connect(transportBar_, &TransportBar::playStopClicked, this, &MainWindow::onPlayStop);
    connect(transportBar_, &TransportBar::clearClicked,    this, &MainWindow::onClear);
    connect(transportBar_, &TransportBar::bpmChanged,      this, &MainWindow::onBpmChanged);
    connect(transportBar_, &TransportBar::lengthChanged,   this, &MainWindow::onLengthChanged);

    connect(transportBar_, &TransportBar::saveClicked, this, [this] {
        QString path = QFileDialog::getSaveFileName(
            this, "Sauvegarder le pattern", "pattern.json",
            "Pattern JSON (*.json)");
        if (path.isEmpty()) return;
        if (!pattern_->saveToFile(path.toStdString()))
            QMessageBox::warning(this, "Erreur", "Impossible de sauvegarder.");
    });

    connect(transportBar_, &TransportBar::loadClicked, this, [this] {
        QString path = QFileDialog::getOpenFileName(
            this, "Charger un pattern", "", "Pattern JSON (*.json)");
        if (path.isEmpty()) return;
        auto loaded = seq::Pattern::loadFromFile(path.toStdString());
        if (!loaded) {
            QMessageBox::warning(this, "Erreur", "Fichier invalide.");
            return;
        }
        *pattern_ = *loaded;
        stepGrid_->updatePattern(pattern_.get());
    });

    connect(stepGrid_, &StepGrid::stepToggled, this, [this](int pad, int step) {
        pattern_->toggle(pad, step);
        stepGrid_->updatePattern(pattern_.get());
    });

    connect(stepGrid_, &StepGrid::stepGateToggled,
            this, [this](int, int) { stepGrid_->update(); });

    connect(stepGrid_, &StepGrid::stepDataChanged,
            this, [this](int, int, seq::StepData) { stepGrid_->update(); });

    connect(stepGrid_, &StepGrid::trackGateToggled,
            this, [this](int pad) {
                pattern_->toggleTrackGate(pad);
                stepGrid_->update();
            });

    connect(stepGrid_, &StepGrid::trackMuteToggled,
            this, [this](int pad) {
                pattern_->toggleMute(pad);
                stepGrid_->update();
            });

    connect(stepGrid_, &StepGrid::trackSoloToggled,
            this, [this](int pad) {
                pattern_->toggleSolo(pad);
                stepGrid_->update();
            });

    connect(stepGrid_, &StepGrid::trackLengthChanged, this, [](int, int) {});
}

MainWindow::~MainWindow() {
    if (engine_) engine_->stop();
}

// ── Slots ─────────────────────────────────────────────────────────

void MainWindow::onPlayStop() {
    if (engine_->isRunning()) {
        stopSequencer();
    } else {
        pattern_->trackSteps.fill(0);
        engine_->start(
            pattern_, kitManager_,
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
}

void MainWindow::onBpmChanged(int bpm)    { pattern_->setBpm(bpm); }

void MainWindow::onLengthChanged(int len) {
    pattern_->setLength(len);
    stepGrid_->updatePattern(pattern_.get());
}

// ── Callback séquenceur → flash pads + highlight grille ───────────
void MainWindow::onSequencerStep(const seq::TrackSteps& steps) {
    QMetaObject::invokeMethod(this, [this, steps] {
        stepGrid_->setCurrentSteps(steps);
        transportBar_->setStep(steps[0]);

        // Flash des pads actifs sur ce tick
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        for (int p = 0; p < seq::MAX_PADS; ++p) {
            if (!pattern_->shouldPlay(p)) continue;
            const seq::StepData& sd = pattern_->grid[p][steps[p]];
            if (sd.active)
                padGrid_->flashPad(p);
        }
    }, Qt::QueuedConnection);
}

// ── Numpad ────────────────────────────────────────────────────────
void MainWindow::keyPressEvent(QKeyEvent* event) {
    int key = event->key();
    for (int i = 0; i < 9; ++i) {
        if (key == NUMPAD_KEYS[i]) {
            emit padGrid_->padTriggered(i);
            padGrid_->flashPad(i);
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace wako::ui