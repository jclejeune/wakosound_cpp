#pragma once
#include <QWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QRadioButton>
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
    void modeChanged(bool gate);   // true = gate
    void saveClicked();
    void loadClicked();

private:
    QPushButton* playBtn_   = nullptr;
    QLabel*      stepLcd_   = nullptr;
};

} // namespace wako::ui
