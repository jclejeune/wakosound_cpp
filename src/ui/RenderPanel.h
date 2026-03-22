#pragma once
#include "../sequencer/Pattern.h"
#include "../model/KitManager.h"
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <memory>

namespace wako::ui {

class RenderPanel : public QWidget {
    Q_OBJECT
public:
    explicit RenderPanel(std::shared_ptr<seq::Pattern>      pattern,
                         std::shared_ptr<model::KitManager> kitManager,
                         QWidget* parent = nullptr);

signals:
    // Demande à MainWindow de démarrer/stopper le séquenceur
    void sequencerStartRequested();
    void sequencerStopRequested();

private slots:
    void onBrowse();
    void onExport();
    void onRecordingFinished();
    void updateDurationLabel();
    void onProgressTick();

private:
    void setStatus(bool ok, bool clipping = false, const QString& msg = "");

    std::shared_ptr<seq::Pattern>      pattern_;
    std::shared_ptr<model::KitManager> kitManager_;

    QLineEdit*    fileEdit_     = nullptr;
    QSpinBox*     loopsSpin_    = nullptr;
    QLabel*       durationLbl_  = nullptr;
    QProgressBar* progressBar_  = nullptr;
    QPushButton*  exportBtn_    = nullptr;
    QLabel*       statusLbl_    = nullptr;

    // Timer de fin d'enregistrement
    QTimer* recordTimer_    = nullptr;
    // Timer de mise à jour de la progress bar
    QTimer* progressTimer_  = nullptr;

    int    totalDurationMs_ = 0;
    qint64 recordStartMs_   = 0;

    bool rendering_ = false;
};

} // namespace wako::ui