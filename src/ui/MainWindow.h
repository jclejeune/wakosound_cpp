#pragma once
#include "../model/KitManager.h"
#include "../sequencer/Pattern.h"
#include "../sequencer/Engine.h"
#include <QMainWindow>
#include <memory>

namespace wako::ui {

class PadGrid;
class StepGrid;
class TransportBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onPlayStop();
    void onClear();
    void onBpmChanged(int bpm);
    void onLengthChanged(int length);
    void onModeChanged(bool gate);

private:
    void onSequencerStep(const seq::TrackSteps& steps);
    void stopSequencer();

    std::shared_ptr<model::KitManager> kitManager_;
    std::shared_ptr<seq::Pattern>      pattern_;
    std::unique_ptr<seq::Engine>       engine_;
    seq::PlayMode                      playMode_ = seq::PlayMode::OneShot;

    PadGrid*      padGrid_      = nullptr;
    StepGrid*     stepGrid_     = nullptr;
    TransportBar* transportBar_ = nullptr;
};

} // namespace wako::ui