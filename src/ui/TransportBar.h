#pragma once
#include <QWidget>
#include <QToolButton>
#include <QLabel>

namespace wako::ui {

class TransportBar : public QWidget {
    Q_OBJECT
public:
    explicit TransportBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setStep(int step);

signals:
    void playStopClicked();
    void clearClicked();
    void bpmChanged(int bpm);
    void lengthChanged(int length);
    void saveClicked();
    void loadClicked();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QToolButton* playBtn_     = nullptr;
    QLabel*      stepLcd_     = nullptr;
    QWidget*     stepSection_  = nullptr;
    QWidget*     bpmSection_   = nullptr;
    QWidget*     stepsSection_ = nullptr;
    bool         compact_      = false;
};

} // namespace wako::ui