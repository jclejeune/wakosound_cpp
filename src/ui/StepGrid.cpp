#include "StepGrid.h"
#include <QPainter>
#include <QMouseEvent>
#include <array>
#include <cmath>

namespace wako::ui {

// ──────────────────────────────────────────────────────────────────
// Palette pitch — 25 couleurs, index = pitch + 12
// Tons chauds : gris au centre, feu/rouge/marron vers +12, sombre vers -12
// ──────────────────────────────────────────────────────────────────
static const std::array<QColor, 25> PITCH_COLORS = {{
    { 55,  15,   5},  // -12 marron très sombre
    { 70,  20,   5},  // -11
    { 90,  25,   5},  // -10 rouge-brun sombre
    {115,  30,   5},  // -9
    {140,  35,   5},  // -8 rouge sombre
    {160,  48,  10},  // -7
    {170,  60,  15},  // -6 rouge
    {175,  80,  20},  // -5
    {168, 100,  30},  // -4 rouge-orangé
    {155, 115,  55},  // -3
    {140, 125,  80},  // -2 gris chaud
    {128, 122,  98},  // -1
    {118, 116, 104},  // 0  gris neutre ← pivot
    {140, 132,  98},  // +1
    {168, 148,  75},  // +2 doré
    {195, 158,  45},  // +3 or
    {215, 152,  12},  // +4 orange doré
    {230, 138,   0},  // +5 orange feu
    {238, 118,   0},  // +6 feu
    {240,  95,   0},  // +7 rouge-orange vif
    {235,  72,   0},  // +8 rouge feu
    {240, 100,  18},  // +9 orange-rouge clair
    {248, 155,  42},  // +10 orange lumineux
    {252, 210,  95},  // +11 jaune chaud
    {255, 242, 180},  // +12 blanc-jaune
}};

static const QColor BG      {60,  63,  65};
static const QColor FG      {187, 187, 187};
static const QColor OUT_ZONE{ 40,  40,  40};
static const QColor BLACK   {  0,   0,   0};
static const QColor LEN_COL { 80, 130, 200};
static const QColor RED_HL  {220,  40,  40};  // highlight step courant

// ──────────────────────────────────────────────────────────────────
StepGrid::StepGrid(std::shared_ptr<seq::Pattern> pattern, QWidget* parent)
    : QWidget(parent), pattern_(std::move(pattern))
{
    currentSteps_.fill(-1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    padNames_.resize(seq::MAX_PADS);
    for (int i = 0; i < seq::MAX_PADS; ++i)
        padNames_[i] = "Pad " + std::to_string(i + 1);
}

void StepGrid::setCurrentSteps(const seq::TrackSteps& steps) {
    currentSteps_ = steps;
    update();
}

void StepGrid::setCurrentStep(int step) {
    currentSteps_.fill(step);
    update();
}

void StepGrid::updatePattern(const seq::Pattern* pat) {
    if (pat) *pattern_ = *pat;
    update();
}

void StepGrid::setKit(const model::Kit* kit) {
    for (int i = 0; i < seq::MAX_PADS; ++i) {
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        padNames_[i] = pad ? pad->name : ("Pad " + std::to_string(i + 1));
    }
    update();
}

// ── Helpers de position ──────────────────────────────────────────

int StepGrid::stepX0(int s) const {
    return LABEL_W + s * (width() - LABEL_W) / seq::MAX_STEPS;
}
int StepGrid::stepX1(int s) const {
    return LABEL_W + (s + 1) * (width() - LABEL_W) / seq::MAX_STEPS;
}
int StepGrid::padY0(int p) const {
    return HEADER_H + p * (height() - HEADER_H) / seq::MAX_PADS;
}
int StepGrid::padY1(int p) const {
    return HEADER_H + (p + 1) * (height() - HEADER_H) / seq::MAX_PADS;
}

// ── Couleur d'une case ───────────────────────────────────────────

QColor StepGrid::stepColor(int pitch, float volume, bool active) const {
    if (!active) return QColor{50, 50, 50};

    int idx = std::clamp(pitch + 12, 0, 24);
    QColor base = PITCH_COLORS[idx];

    // Volume module la luminosité
    return QColor(
        int(base.red()   * volume),
        int(base.green() * volume),
        int(base.blue()  * volume)
    );
}

QColor StepGrid::textColor(const QColor& bg) const {
    float lum = (0.299f * bg.red() + 0.587f * bg.green() + 0.114f * bg.blue()) / 255.0f;
    return lum > 0.45f ? QColor(20, 20, 20) : QColor(255, 255, 255);
}

// ── paintEvent ───────────────────────────────────────────────────

void StepGrid::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(0, 0, width(), height(), BG);

    // Headers
    p.setFont(QFont("Dialog", 9));
    p.setPen(FG);
    for (int s = 0; s < seq::MAX_STEPS; ++s) {
        if (s % 4 == 0) {
            int x = stepX0(s);
            p.drawText(QRect(x - 8, 0, 16, HEADER_H),
                       Qt::AlignCenter, QString::number(s + 1));
        }
    }

    // Cellules
    for (int pad = 0; pad < seq::MAX_PADS; ++pad) {
        int y0       = padY0(pad);
        int y1       = padY1(pad);
        int ch       = y1 - y0;
        int trackLen = pattern_->trackLengths[pad];
        int curStep  = currentSteps_[pad];

        // Label
        p.setPen(FG);
        p.setFont(QFont("Dialog", 10));
        p.drawText(QRect(2, y0, LABEL_W - 4, ch),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(padNames_[pad]));

        for (int s = 0; s < seq::MAX_STEPS; ++s) {
            int x0 = stepX0(s);
            int x1 = stepX1(s);
            int cw = x1 - x0;

            const seq::StepData& sd      = pattern_->grid[pad][s];
            bool                  inRange = (s < trackLen);
            bool                  current = (s == curStep);

            QColor fill;
            if (!inRange) {
                fill = OUT_ZONE;
            } else if (current && sd.active) {
                fill = RED_HL;
            } else {
                fill = stepColor(sd.pitch, sd.volume, sd.active);
            }

            p.fillRect(x0 + 1, y0 + 1, cw - 2, ch - 2, fill);
            p.setPen(BLACK);
            p.drawRect(x0, y0, cw, ch);

            // Texte pendant le drag sur cette case
            if (drag_.active && drag_.hasMoved &&
                pad == drag_.pad && s == drag_.step)
            {
                QString txt;
                if (drag_.axis == Axis::Horizontal) {
                    txt = (sd.pitch >= 0 ? "+" : "") + QString::number(sd.pitch);
                } else if (drag_.axis == Axis::Vertical) {
                    txt = QString::number(int(sd.volume * 100)) + "%";
                }
                if (!txt.isEmpty()) {
                    p.setPen(textColor(fill));
                    p.setFont(QFont("Dialog", 8, QFont::Bold));
                    p.drawText(QRect(x0, y0, cw, ch), Qt::AlignCenter, txt);
                }
            }
        }

        // Marqueur fin de boucle
        int markerX = stepX0(trackLen);
        p.setPen(QPen(LEN_COL, 2));
        p.drawLine(markerX, y0 + 2, markerX, y1 - 2);
    }
}

// ── Mouse events ─────────────────────────────────────────────────

void StepGrid::mousePressEvent(QMouseEvent* ev) {
    int mx = ev->position().x();
    int my = ev->position().y();

    // Clic droit sur la grille → longueur de track
    if (ev->button() == Qt::RightButton && mx >= LABEL_W && my >= HEADER_H) {
        int gridW = width() - LABEL_W;
        int gridH = height() - HEADER_H;
        int s = (mx - LABEL_W) * seq::MAX_STEPS / gridW;
        int p = (my - HEADER_H) * seq::MAX_PADS  / gridH;
        if (s >= 0 && s < seq::MAX_STEPS && p >= 0 && p < seq::MAX_PADS) {
            pattern_->setTrackLength(p, s + 1);
            emit trackLengthChanged(p, s + 1);
            update();
        }
        return;
    }

    if (ev->button() != Qt::LeftButton) return;
    if (mx < LABEL_W || my < HEADER_H) return;

    int gridW = width() - LABEL_W;
    int gridH = height() - HEADER_H;
    int s = (mx - LABEL_W) * seq::MAX_STEPS / gridW;
    int p = (my - HEADER_H) * seq::MAX_PADS  / gridH;

    if (s < 0 || s >= seq::MAX_STEPS || p < 0 || p >= seq::MAX_PADS) return;

    const seq::StepData& sd = pattern_->grid[p][s];
    drag_.active    = true;
    drag_.pad       = p;
    drag_.step      = s;
    drag_.startX    = mx;
    drag_.startY    = my;
    drag_.origVol   = sd.volume;
    drag_.origPitch = sd.pitch;
    drag_.hasMoved  = false;
    drag_.axis      = Axis::None;
}

void StepGrid::mouseMoveEvent(QMouseEvent* ev) {
    if (!drag_.active) return;

    int dx = ev->position().x() - drag_.startX;
    int dy = ev->position().y() - drag_.startY;

    // Locker l'axe sur le premier mouvement significatif
    if (!drag_.hasMoved) {
        if (std::abs(dx) < DRAG_THRESHOLD && std::abs(dy) < DRAG_THRESHOLD)
            return;
        drag_.hasMoved = true;
        drag_.axis = (std::abs(dx) >= std::abs(dy)) ? Axis::Horizontal : Axis::Vertical;

        // Activer le step si pas encore actif
        if (!pattern_->grid[drag_.pad][drag_.step].active) {
            pattern_->grid[drag_.pad][drag_.step].active = true;
        }
    }

    seq::StepData& sd = pattern_->grid[drag_.pad][drag_.step];

    if (drag_.axis == Axis::Horizontal) {
        // Drag horizontal → pitch (-12 à +12), 1 semitone par PITCH_PX pixels
        int newPitch = drag_.origPitch + dx / PITCH_PX;
        sd.pitch = std::clamp(newPitch, -12, 12);
    } else {
        // Drag vertical → volume (drag vers le haut = plus fort)
        float newVol = drag_.origVol - float(dy) / VOL_PX;
        sd.volume = std::clamp(newVol, 0.0f, 1.0f);
    }

    update();
}

void StepGrid::mouseReleaseEvent(QMouseEvent* ev) {
    if (!drag_.active || ev->button() != Qt::LeftButton) return;

    if (!drag_.hasMoved) {
        // Clic pur → toggle
        emit stepToggled(drag_.pad, drag_.step);
    } else {
        // Drag terminé → émettre les nouvelles données
        emit stepDataChanged(drag_.pad, drag_.step,
                             pattern_->grid[drag_.pad][drag_.step]);
    }

    drag_.active   = false;
    drag_.hasMoved = false;
    drag_.axis     = Axis::None;
    update();
}

void StepGrid::mouseDoubleClickEvent(QMouseEvent* ev) {
    // Double-clic sur le label → reset longueur à patternLength global
    int mx = ev->position().x();
    int my = ev->position().y();
    if (mx >= LABEL_W || my < HEADER_H) return;

    int p = (my - HEADER_H) * seq::MAX_PADS / (height() - HEADER_H);
    if (p < 0 || p >= seq::MAX_PADS) return;

    pattern_->setTrackLength(p, pattern_->patternLength);
    emit trackLengthChanged(p, pattern_->patternLength);
    update();
}

} // namespace wako::ui