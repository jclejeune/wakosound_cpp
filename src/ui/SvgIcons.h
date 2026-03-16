#pragma once
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QByteArray>
#include <QtSvg/QSvgRenderer>
#include <QString>

// ──────────────────────────────────────────────────────────────────
// SVG Material Design icons — inline, couleur configurable
// Tous issus de Material Icons (Apache 2.0)
// ──────────────────────────────────────────────────────────────────

namespace wako::ui::icons {

// Rend un SVG en QPixmap à la taille donnée
inline QPixmap render(const char* svg, int size = 20,
                      const QString& color = "#DCDCDC") {
    // Injecter la couleur dans le SVG
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

} // namespace wako::ui::icons