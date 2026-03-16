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
    resize(760, 560);

    kitManager_ = std::make_shared<model::KitManager>();
    pattern_    = std::make_shared<seq::Pattern>();
    engine_     = std::make_unique<seq::Engine>();

    if (!kitManager_->loadFromFile("kits.json"))
        std::cerr << "[MainWindow] kits.json introuvable\n";

    audio::AudioCache::instance().preload(kitManager_->currentKitFilePaths());

    auto* central    = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    auto* topBar     = new QWidget;
    auto* topLayout  = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(4, 2, 4, 2);
    auto* kitLabel   = new QLabel("Kit:");
    auto* kitCombo   = new QComboBox;
    auto* statusLabel = new QLabel;

    for (const auto& k : kitManager_->kits())
        kitCombo->addItem(QString::fromStdString(k.name));
    kitCombo->setCurrentIndex(kitManager_->currentIndex());

    topLayout->addWidget(kitLabel);
    topLayout->addWidget(kitCombo);
    topLayout->addWidget(statusLabel);
    topLayout->addStretch();

    auto refreshStatus = [this, statusLabel] {
        const auto* kit = kitManager_->currentKit();
        if (kit)
            statusLabel->setText(
                QString("  %1 | %2/9 pads")
                    .arg(QString::fromStdString(kit->name))
                    .arg(kit->padCount()));
    };

    padGrid_      = new PadGrid(this);
    stepGrid_     = new StepGrid(pattern_, this);
    transportBar_ = new TransportBar(this);

    auto* samplerTab = new QWidget;
    auto* samplerLay = new QVBoxLayout(samplerTab);
    samplerLay->setContentsMargins(0, 0, 0, 0);
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

    padGrid_->refresh(kitManager_->currentKit());
    refreshStatus();

    connect(kitCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, kitCombo, refreshStatus](int idx) {
                kitManager_->switchTo(idx);
                audio::AudioCache::instance().preload(
                    kitManager_->currentKitFilePaths());
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

    connect(transportBar_, &TransportBar::playStopClicked, this, &MainWindow::onPlayStop);
    connect(transportBar_, &TransportBar::clearClicked,    this, &MainWindow::onClear);
    connect(transportBar_, &TransportBar::bpmChanged,      this, &MainWindow::onBpmChanged);
    connect(transportBar_, &TransportBar::lengthChanged,   this, &MainWindow::onLengthChanged);
    connect(transportBar_, &TransportBar::modeChanged,     this, &MainWindow::onModeChanged);

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

    // Toggle step (clic pur)
    connect(stepGrid_, &StepGrid::stepToggled, this, [this](int pad, int step) {
        pattern_->toggle(pad, step);
        stepGrid_->updatePattern(pattern_.get());
    });

    // StepData modifié par drag — pattern_ déjà mis à jour dans StepGrid
    // Rien à faire ici sauf rafraîchir l'affichage
    connect(stepGrid_, &StepGrid::stepDataChanged,
            this, [this](int /*pad*/, int /*step*/, seq::StepData /*data*/) {
                stepGrid_->update();
            });

    connect(stepGrid_, &StepGrid::trackLengthChanged,
            this, [](int, int) {});

    connect(tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 0 && engine_->isRunning())
            stopSequencer();
    });
}

MainWindow::~MainWindow() {
    if (engine_) engine_->stop();
}

void MainWindow::onPlayStop() {
    if (engine_->isRunning()) {
        stopSequencer();
    } else {
        pattern_->trackSteps.fill(0);
        engine_->setPlayMode(playMode_);
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
    stepGrid_->setCurrentStep(-1);
}

void MainWindow::onClear() {
    pattern_->clearAll();
    stepGrid_->updatePattern(pattern_.get());
}

void MainWindow::onBpmChanged(int bpm)    { pattern_->setBpm(bpm); }

void MainWindow::onLengthChanged(int len) {
    pattern_->setLength(len);
    stepGrid_->updatePattern(pattern_.get());
}

void MainWindow::onModeChanged(bool gate) {
    playMode_ = gate ? seq::PlayMode::Gate : seq::PlayMode::OneShot;
    engine_->setPlayMode(playMode_);
}

void MainWindow::onSequencerStep(const seq::TrackSteps& steps) {
    QMetaObject::invokeMethod(this, [this, steps] {
        stepGrid_->setCurrentSteps(steps);
        transportBar_->setStep(steps[0]);
    }, Qt::QueuedConnection);
}

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