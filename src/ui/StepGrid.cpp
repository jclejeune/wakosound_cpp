#include "StepGrid.h"
#include <QPainter>
#include <QMouseEvent>

namespace wako::ui {

static const QColor BG       {60,  63,  65};
static const QColor FG       {187, 187, 187};
static const QColor ORANGE   {255, 165,   0};
static const QColor RED      {200,  50,  50};
static const QColor DARK     { 50,  50,  50};
static const QColor OUT_ZONE { 40,  40,  40};
static const QColor BLACK    {  0,   0,   0};
static const QColor LEN_COL  { 80, 130, 200};

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

// ──────────────────────────────────────────────────────────────────
// Helpers — positions entières calculées depuis la largeur totale
// Pas d'accumulation float → pas de dérive
// ──────────────────────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────────────────────
// paintEvent
// ──────────────────────────────────────────────────────────────────
void StepGrid::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(0, 0, width(), height(), BG);

    // ── Headers — chiffre centré sur le bord gauche du step ──────
    p.setFont(QFont("Dialog", 9));
    p.setPen(FG);
    for (int s = 0; s < seq::MAX_STEPS; ++s) {
        if (s % 4 == 0) {
            int x = stepX0(s);
            p.drawText(QRect(x - 8, 0, 16, HEADER_H),
                       Qt::AlignCenter, QString::number(s + 1));
        }
    }

    // ── Cellules ─────────────────────────────────────────────────
    for (int pad = 0; pad < seq::MAX_PADS; ++pad) {
        int y0       = padY0(pad);
        int y1       = padY1(pad);
        int ch       = y1 - y0;
        int trackLen = pattern_->trackLengths[pad];
        int curStep  = currentSteps_[pad];

        // Label pad
        p.setPen(FG);
        p.setFont(QFont("Dialog", 10));
        p.drawText(QRect(2, y0, LABEL_W - 4, ch),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(padNames_[pad]));

        for (int s = 0; s < seq::MAX_STEPS; ++s) {
            int x0 = stepX0(s);
            int x1 = stepX1(s);
            int cw = x1 - x0;

            bool active  = pattern_->get(pad, s);
            bool current = (s == curStep);
            bool inRange = (s < trackLen);

            QColor fill;
            if (!inRange)     fill = OUT_ZONE;
            else if (current) fill = RED;
            else if (active)  fill = ORANGE;
            else              fill = DARK;

            p.fillRect(x0 + 1, y0 + 1, cw - 2, ch - 2, fill);
            p.setPen(BLACK);
            p.drawRect(x0, y0, cw, ch);
        }

        // Marqueur fin de boucle
        int markerX = stepX0(trackLen);
        p.setPen(QPen(LEN_COL, 2));
        p.drawLine(markerX, y0 + 2, markerX, y1 - 2);
    }
}

// ──────────────────────────────────────────────────────────────────
// mousePressEvent
// Clic gauche  → toggle step
// Clic droit   → longueur track = step + 1
// ──────────────────────────────────────────────────────────────────
void StepGrid::mousePressEvent(QMouseEvent* ev) {
    int mx = ev->position().x();
    int my = ev->position().y();

    if (mx < LABEL_W || my < HEADER_H) return;

    // Trouver step et pad depuis les positions entières
    int gridW = width() - LABEL_W;
    int gridH = height() - HEADER_H;
    int s = (mx - LABEL_W) * seq::MAX_STEPS / gridW;
    int p = (my - HEADER_H) * seq::MAX_PADS  / gridH;

    if (s < 0 || s >= seq::MAX_STEPS) return;
    if (p < 0 || p >= seq::MAX_PADS)  return;

    if (ev->button() == Qt::LeftButton) {
        emit stepToggled(p, s);
    } else if (ev->button() == Qt::RightButton) {
        pattern_->setTrackLength(p, s + 1);
        emit trackLengthChanged(p, s + 1);
        update();
    }
}

// Double-clic sur le label → reset longueur à patternLength global
void StepGrid::mouseDoubleClickEvent(QMouseEvent* ev) {
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