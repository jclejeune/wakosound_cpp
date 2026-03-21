#pragma once
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QByteArray>
#include <QFontDatabase>
#include <QFont>
#include <QtSvg/QSvgRenderer>
#include <QString>

namespace wako::ui::icons {

// ── Rendu SVG ─────────────────────────────────────────────────────

inline QPixmap render(const char* svg, int size = 20,
                      const QString& color = "#DCDCDC") {
    QString src = QString(svg).replace("%%COLOR%%", color);
    QSvgRenderer renderer(src.toUtf8());
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p);
    return pm;
}

inline QIcon icon(const char* svg, int size = 20,
                  const QString& color = "#DCDCDC") {
    return QIcon(render(svg, size, color));
}

// ── Font LCD ──────────────────────────────────────────────────────

inline QFont lcdFont(int size = 20) {
    static int fontId = -1;
    if (fontId < 0) {
        fontId = QFontDatabase::addApplicationFont(":/resources/fonts/lcd.ttf");
        if (fontId < 0)
            return QFont("Monospace", size, QFont::Bold);
    }
    QString family = QFontDatabase::applicationFontFamilies(fontId).first();
    return QFont(family, size);
}

// ── Icônes ────────────────────────────────────────────────────────

inline constexpr const char* PLAY = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M8 5v14l11-7z"/>
</svg>)";

inline constexpr const char* STOP = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M6 6h12v12H6z"/>
</svg>)";

inline constexpr const char* PAUSE = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/>
</svg>)";

inline constexpr const char* CLEAR = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M17.65 6.35A7.958 7.958 0 0012 4c-4.42
    0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55
    7.73-6h-2.08A5.99 5.99 0 0112 18c-3.31
    0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22
    1.78L13 11h7V4l-2.35 2.35z"/>
</svg>)";

inline constexpr const char* SAVE = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M17 3H5a2 2 0 00-2 2v14a2 2 0 002
    2h14a2 2 0 002-2V7l-4-4zm2 16H5V5h11.17L19
    7.83V19zm-7-7a3 3 0 100 6 3 3 0 000-6zm-5-5h8v4H7V7z"/>
</svg>)";

inline constexpr const char* FOLDER_OPEN = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M20 6h-8l-2-2H4a2 2 0 00-2
    2v12a2 2 0 002 2h16a2 2 0 002-2V8a2 2 0
    00-2-2zm0 12H4V8h16v10z"/>
</svg>)";

inline constexpr const char* MUSIC_NOTE = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M12 3v10.55A4 4 0 1014 17V7h4V3h-6z"/>
</svg>)";

inline constexpr const char* GRID = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M3 3h7v7H3zm0 11h7v7H3zm11-11h7v7h-7zm0 11h7v7h-7z"/>
</svg>)";

inline constexpr const char* LIBRARY = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M4 6H2v14a2 2 0 002 2h14v-2H4V6zm16-4H8a2 2 0
    00-2 2v12a2 2 0 002 2h12a2 2 0 002-2V4a2 2 0
    00-2-2zm-1 9H9V9h10v2zm-4 4H9v-2h6v2zm4-8H9V5h10v2z"/>
</svg>)";

// Panel latéral — deux colonnes verticales dont une plus large (sidebar)
inline constexpr const char* SIDEBAR = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="%%COLOR%%" d="M3 3h18a1 1 0 011 1v16a1 1 0 01-1
    1H3a1 1 0 01-1-1V4a1 1 0 011-1zm1 2v14h6V5H4zm8
    0v14h8V5h-8z"/>
</svg>)";

} // namespace wako::ui::icons