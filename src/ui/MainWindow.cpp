#include "MainWindow.h"
#include "PadGrid.h"
#include "StepGrid.h"
#include "TransportBar.h"
#include "../audio/AudioCache.h"
#include "../audio/Player.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QKeyEvent>
#include <QMetaObject>
#include <iostream>
#include <QFileDialog>
#include <QMessageBox>

// Mapping numpad → pad index (disposition MPC 7 8 9 / 4 5 6 / 1 2 3)
static const int NUMPAD_KEYS[] = {
    Qt::Key_7, Qt::Key_8, Qt::Key_9,
    Qt::Key_4, Qt::Key_5, Qt::Key_6,
    Qt::Key_1, Qt::Key_2, Qt::Key_3
};

namespace wako::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WakoSound");
    resize(640, 540);

    kitManager_ = std::make_shared<model::KitManager>();
    pattern_    = std::make_shared<seq::Pattern>();
    engine_     = std::make_unique<seq::Engine>();

    // Charger kits
    if (!kitManager_->loadFromFile("kits.json")) {
        std::cerr << "[MainWindow] kits.json introuvable — kit vide\n";
    }

    // Précharger les sons du kit par défaut
    audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());

    // ── Build UI ──────────────────────────────────────────────────
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Kit selector bar
    auto* topBar    = new QWidget;
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(4, 2, 4, 2);

    auto* kitLabel = new QLabel("Kit:");
    auto* kitCombo = new QComboBox;
    for (const auto& k : kitManager_->kits())
        kitCombo->addItem(QString::fromStdString(k.name));
    kitCombo->setCurrentIndex(kitManager_->currentIndex());

    auto* statusLabel = new QLabel;
    topLayout->addWidget(kitLabel);
    topLayout->addWidget(kitCombo);
    topLayout->addWidget(statusLabel);
    topLayout->addStretch();

    auto refreshStatus = [this, statusLabel] {
        const auto* kit = kitManager_->currentKit();
        if (kit) statusLabel->setText(
            QString("  %1 | %2/9 pads")
                .arg(QString::fromStdString(kit->name))
                .arg(kit->padCount()));
    };

    // ── Tabs ──────────────────────────────────────────────────────
    padGrid_      = new PadGrid(this);
    stepGrid_     = new StepGrid(pattern_, this);
    transportBar_ = new TransportBar(this);

    auto* samplerTab   = new QWidget;
    auto* samplerLay   = new QVBoxLayout(samplerTab);
    samplerLay->addWidget(padGrid_, 1);

    auto* seqTab = new QWidget;
    auto* seqLay = new QVBoxLayout(seqTab);
    seqLay->addWidget(transportBar_);
    seqLay->addWidget(stepGrid_, 1);

    auto* tabs = new QTabWidget;
    tabs->addTab(samplerTab, "🥁 Sampler");
    tabs->addTab(seqTab,     "🎛 Séquenceur");

    mainLayout->addWidget(topBar);
    mainLayout->addWidget(tabs, 1);

    // ── Connexions ────────────────────────────────────────────────
    padGrid_->refresh(kitManager_->currentKit());
    refreshStatus();

    connect(kitCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, kitCombo, refreshStatus](int idx) {
                kitManager_->switchTo(idx);
                audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());
                padGrid_->refresh(kitManager_->currentKit());
                stepGrid_->setKit(kitManager_->currentKit());
                refreshStatus();
            });

    connect(padGrid_, &PadGrid::padTriggered, this, [this](int idx) {
        const auto* kit = kitManager_->currentKit();
        if (!kit) return;
        const auto* pad = kit->pad(idx);
        if (pad && pad->enabled && !pad->filePath.empty())
            audio::Player::instance().play(pad->filePath, pad->volume);
    });

    connect(transportBar_, &TransportBar::playStopClicked,  this, &MainWindow::onPlayStop);
    connect(transportBar_, &TransportBar::clearClicked,     this, &MainWindow::onClear);
    connect(transportBar_, &TransportBar::bpmChanged,       this, &MainWindow::onBpmChanged);
    connect(transportBar_, &TransportBar::lengthChanged,    this, &MainWindow::onLengthChanged);
    connect(transportBar_, &TransportBar::modeChanged,      this, &MainWindow::onModeChanged);

    connect(stepGrid_, &StepGrid::stepToggled, this, [this](int pad, int step) {
        pattern_->toggle(pad, step);
        stepGrid_->updatePattern(pattern_.get());
    });

    // Stopper le séquenceur au retour sur l'onglet Sampler
    connect(tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 0 && engine_->isRunning()) stopSequencer();
    });


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
        this, "Charger un pattern", "",
        "Pattern JSON (*.json)");
    if (path.isEmpty()) return;
    auto loaded = seq::Pattern::loadFromFile(path.toStdString());
    if (!loaded) {
        QMessageBox::warning(this, "Erreur", "Fichier invalide.");
        return;
    }
    *pattern_ = *loaded;
    stepGrid_->updatePattern(pattern_.get());
});

}

MainWindow::~MainWindow() {
    if (engine_) engine_->stop();
}

// ── Slots ─────────────────────────────────────────────────────────

void MainWindow::onPlayStop() {
    if (engine_->isRunning()) {
        stopSequencer();
    } else {
        pattern_->currentStep = 0;
        engine_->start(pattern_, kitManager_,
                       [this](int step) { onSequencerStep(step); },
                       playMode_);
        transportBar_->setPlaying(true);
    }
}

void MainWindow::stopSequencer() {
    engine_->stop();
    pattern_->currentStep = 0;
    transportBar_->setPlaying(false);
    stepGrid_->setCurrentStep(-1);
}

void MainWindow::onClear() {
    pattern_->clearAll();
    stepGrid_->updatePattern(pattern_.get());
}

void MainWindow::onBpmChanged(int bpm)    { pattern_->setBpm(bpm); }
void MainWindow::onLengthChanged(int len) { pattern_->setLength(len); }
void MainWindow::onModeChanged(bool gate) {
    playMode_ = gate ? seq::PlayMode::Gate : seq::PlayMode::OneShot;
}

// ── Callback séquenceur (thread RT) → Qt via invokeMethod ─────────
void MainWindow::onSequencerStep(int step) {
    QMetaObject::invokeMethod(this, [this, step] {
        stepGrid_->setCurrentStep(step);
        transportBar_->setStep(step);
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
