#pragma once
#include "../audio/EffectChain.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QCloseEvent>

namespace wako::ui {

class EffectWindow : public QWidget {
    Q_OBJECT
public:
    explicit EffectWindow(audio::EffectChain& chain,
                          const QString& channelName,
                          QWidget* parent = nullptr);

    void setChannelName(const QString& name);

signals:
    // Émis quand l'utilisateur ferme la fenêtre via la croix
    void closed();

protected:
    void closeEvent(QCloseEvent* event) override {
        emit closed();
        event->accept();   // cache (WA_DeleteOnClose=false), ne détruit pas
    }

private:
    void buildEQ    (QVBoxLayout* root);
    void buildSat   (QVBoxLayout* root);
    void buildReverb(QVBoxLayout* root);
    void buildDelay (QVBoxLayout* root);

    audio::EffectChain& chain_;
};

} // namespace wako::ui