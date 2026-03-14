#include "ui/MainWindow.h"
#include "audio/Player.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <iostream>

static void applyDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window,          {60, 63, 65});
    p.setColor(QPalette::WindowText,      {187, 187, 187});
    p.setColor(QPalette::Base,            {43, 43, 43});
    p.setColor(QPalette::AlternateBase,   {60, 63, 65});
    p.setColor(QPalette::Text,            {187, 187, 187});
    p.setColor(QPalette::Button,          {77, 77, 77});
    p.setColor(QPalette::ButtonText,      {220, 220, 220});
    p.setColor(QPalette::Highlight,       {255, 165, 0});
    p.setColor(QPalette::HighlightedText, {30, 30, 30});
    p.setColor(QPalette::BrightText,      Qt::red);
    app.setPalette(p);
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WakoSound");
    app.setApplicationVersion("0.1.0");
    applyDarkTheme(app);

    // Démarrer PortAudio avant la fenêtre
    if (!wako::audio::Player::instance().init(44100, 256)) {
        std::cerr << "FATAL: impossible d'ouvrir le stream audio\n";
        return 1;
    }

    wako::ui::MainWindow win;
    win.show();

    int ret = app.exec();
    wako::audio::Player::instance().shutdown();
    return ret;
}
