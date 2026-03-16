#pragma once
#include <QWidget>
#include <QToolButton>
#include <QLabel>
#include <QComboBox>

namespace wako::ui {

class TransportBar : public QWidget {
    Q_OBJECT
public:
    explicit TransportBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setStep(int step);
    void setKits(const QStringList& names, int currentIndex);

signals:
    void playStopClicked();
    void clearClicked();
    void bpmChanged(int bpm);
    void lengthChanged(int length);
    void saveClicked();
    void loadClicked();
    void kitChanged(int index);

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QToolButton* playBtn_     = nullptr;
    QLabel*      stepLcd_     = nullptr;
    QComboBox*   kitCombo_    = nullptr;
    QWidget*     stepSection_  = nullptr;
    QWidget*     bpmSection_   = nullptr;
    QWidget*     stepsSection_ = nullptr;
    bool         compact_      = false;
};

} // namespace wako::ui