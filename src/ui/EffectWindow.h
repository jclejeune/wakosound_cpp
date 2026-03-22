#pragma once
#include "../audio/EffectChain.h"
#include <QWidget>
#include <QVBoxLayout>

namespace wako::ui {

class EffectWindow : public QWidget {
    Q_OBJECT
public:
    explicit EffectWindow(audio::EffectChain& chain,
                          const QString& channelName,
                          QWidget* parent = nullptr);

    void setChannelName(const QString& name);

private:
    void buildEQ    (QVBoxLayout* root);
    void buildSat   (QVBoxLayout* root);
    void buildReverb(QVBoxLayout* root);
    void buildDelay (QVBoxLayout* root);

    audio::EffectChain& chain_;
};

} // namespace wako::ui