#pragma once
#include "../model/KitManager.h"
#include "../sequencer/Pattern.h"
#include "../sequencer/Engine.h"
#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <memory>

namespace wako::ui {

class PadGrid;
class StepGrid;
class TransportBar;
class SampleBrowser;
class MixerPanel;

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
    void onPadFileDropped(int padIdx, const QString& filePath);

private:
    void onSequencerStep(const seq::TrackSteps& steps);
    void stopSequencer();
    void refreshKitCombo();

    std::shared_ptr<model::KitManager> kitManager_;
    std::shared_ptr<seq::Pattern>      pattern_;
    std::unique_ptr<seq::Engine>       engine_;

    PadGrid*       padGrid_       = nullptr;
    StepGrid*      stepGrid_      = nullptr;
    MixerPanel*    mixerPanel_    = nullptr;
    TransportBar*  transportBar_  = nullptr;
    SampleBrowser* sampleBrowser_ = nullptr;
    QTabWidget*    tabs_          = nullptr;
    QSplitter*     splitter_      = nullptr;

    static constexpr int SIDEBAR_MIN     = 160;
    static constexpr int SIDEBAR_MAX     = 420;
    static constexpr int SIDEBAR_DEFAULT = 240;
};

} // namespace wako::ui