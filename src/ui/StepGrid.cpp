#include "StepGrid.h"
#include <QPainter>
#include <QMouseEvent>
#include <array>
#include <cmath>

namespace wako::ui {

static const std::array<QColor, 25> PITCH_COLORS = {{
    { 55,  15,   5}, { 70,  20,   5}, { 90,  25,   5},
    {115,  30,   5}, {140,  35,   5}, {160,  48,  10},
    {170,  60,  15}, {175,  80,  20}, {168, 100,  30},
    {155, 115,  55}, {140, 125,  80}, {128, 122,  98},
    {118, 116, 104},
    {140, 132,  98}, {168, 148,  75}, {195, 158,  45},
    {215, 152,  12}, {230, 138,   0}, {238, 118,   0},
    {240,  95,   0}, {235,  72,   0}, {240, 100,  18},
    {248, 155,  42}, {252, 210,  95}, {255, 242, 180},
}};

static const QColor BG        {60,  63,  65};
static const QColor FG        {187, 187, 187};
static const QColor FG_MUTED  {100, 100, 100};
static const QColor OUT_ZONE  { 40,  40,  40};
static const QColor BLACK     {  0,   0,   0};
static const QColor LEN_COL   { 80, 130, 200};
static const QColor RED_HL    {220,  40,  40};
static const QColor BTN_NORMAL{ 65,  65,  65};
static const QColor BTN_GATE  { 50,  80, 160};   // bleu = gate actif
static const QColor BTN_MUTE  {180,  50,  50};   // rouge = muté
static const QColor BTN_SOLO  {190, 150,   0};   // jaune = solo
static const QColor BTN_FG    {220, 220, 220};

// ── Layout :  [  LABEL_W  ][  BTN_W  ][  BTN_W  ][ grille ... ]
// Les boutons M et S sont APRÈS le label
static constexpr int LABEL_W  = 65;
static constexpr int BTN_W    = 18;
static constexpr int TOTAL_LW = LABEL_W + BTN_W * 2;  // offset avant grille
static constexpr int HEADER_H = 22;

static constexpr int DRAG_THRESHOLD = 5;
static constexpr int PITCH_PX       = 18;
static constexpr float VOL_PX       = 200.0f;

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

// ── Positions ─────────────────────────────────────────────────────
int StepGrid::stepX0(int s) const {
    return TOTAL_LW + s * (width() - TOTAL_LW) / seq::MAX_STEPS;
}
int StepGrid::stepX1(int s) const {
    return TOTAL_LW + (s+1) * (width() - TOTAL_LW) / seq::MAX_STEPS;
}
int StepGrid::padY0(int p) const {
    return HEADER_H + p * (height() - HEADER_H) / seq::MAX_PADS;
}
int StepGrid::padY1(int p) const {
    return HEADER_H + (p+1) * (height() - HEADER_H) / seq::MAX_PADS;
}
int StepGrid::padAtY(int y) const {
    if (y < HEADER_H) return -1;
    int p = (y - HEADER_H) * seq::MAX_PADS / (height() - HEADER_H);
    return (p >= 0 && p < seq::MAX_PADS) ? p : -1;
}
// Retourne : >= 0 = step, -2 = zone G, -3 = zone M, -4 = zone S, -1 = ailleurs
int StepGrid::stepAtX(int x) const {
    if (x >= LABEL_W && x < LABEL_W + BTN_W)               return -2; // G
    if (x >= LABEL_W + BTN_W && x < LABEL_W + BTN_W*2)    return -3; // M
    if (x >= LABEL_W + BTN_W*2 && x < TOTAL_LW)           return -4; // S
    if (x < TOTAL_LW) return -1;
    int s = (x - TOTAL_LW) * seq::MAX_STEPS / (width() - TOTAL_LW);
    return (s >= 0 && s < seq::MAX_STEPS) ? s : -1;
}

// ── Couleurs ──────────────────────────────────────────────────────
QColor StepGrid::stepColor(int pitch, float volume, bool active) const {
    if (!active) return QColor{50, 50, 50};
    int idx = std::clamp(pitch + 12, 0, 24);
    QColor b = PITCH_COLORS[idx];
    return QColor(int(b.red()*volume), int(b.green()*volume), int(b.blue()*volume));
}
QColor StepGrid::textColor(const QColor& bg) const {
    float l = (0.299f*bg.red() + 0.587f*bg.green() + 0.114f*bg.blue()) / 255.0f;
    return l > 0.45f ? QColor(20,20,20) : QColor(255,255,255);
}

// ── paintEvent ────────────────────────────────────────────────────
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
            p.drawText(QRect(x-8, 0, 16, HEADER_H),
                       Qt::AlignCenter, QString::number(s+1));
        }
    }

    for (int pad = 0; pad < seq::MAX_PADS; ++pad) {
        int  y0       = padY0(pad);
        int  y1       = padY1(pad);
        int  ch       = y1 - y0;
        int  trackLen = pattern_->trackLengths[pad];
        int  curStep  = currentSteps_[pad];
        bool isMuted  = pattern_->muted[pad];
        bool isSoloed = pattern_->soloed[pad];

        // ── Label ────────────────────────────────────────────────
        p.setPen(isMuted ? FG_MUTED : FG);
        p.setFont(QFont("Dialog", 10));
        p.drawText(QRect(2, y0, LABEL_W - 2, ch),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(padNames_[pad]));

        // ── Boutons G M S ─────────────────────────────────────────
        int btnH  = std::min(ch - 4, 16);
        int btnY  = y0 + (ch - btnH) / 2;
        bool isGate = pattern_->trackGate[pad];

        p.setFont(QFont("Dialog", 8, QFont::Bold));

        // G
        p.fillRect(LABEL_W,           btnY, BTN_W-1, btnH,
                   isGate   ? BTN_GATE   : BTN_NORMAL);
        p.setPen(BTN_FG);
        p.drawText(QRect(LABEL_W, btnY, BTN_W-1, btnH), Qt::AlignCenter, "G");

        // M
        p.fillRect(LABEL_W + BTN_W,   btnY, BTN_W-1, btnH,
                   isMuted  ? BTN_MUTE  : BTN_NORMAL);
        p.setPen(BTN_FG);
        p.drawText(QRect(LABEL_W+BTN_W, btnY, BTN_W-1, btnH), Qt::AlignCenter, "M");

        // S
        p.fillRect(LABEL_W + BTN_W*2, btnY, BTN_W-1, btnH,
                   isSoloed ? BTN_SOLO  : BTN_NORMAL);
        p.setPen(BTN_FG);
        p.drawText(QRect(LABEL_W+BTN_W*2, btnY, BTN_W-1, btnH), Qt::AlignCenter, "S");

        // ── Cellules ─────────────────────────────────────────────
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
                QColor base = stepColor(sd.pitch, sd.volume, sd.active);
                fill = isMuted
                    ? QColor(base.red()/2, base.green()/2, base.blue()/2)
                    : base;
            }

            p.fillRect(x0+1, y0+1, cw-2, ch-2, fill);
            p.setPen(BLACK);
            p.drawRect(x0, y0, cw, ch);

            // Triangle gate
            if (sd.active && sd.gate && inRange && !current) {
                int ts = std::min(cw, ch) / 3;
                p.setBrush(QColor(255,255,255,180));
                p.setPen(Qt::NoPen);
                QPolygon tri;
                tri << QPoint(x1-1, y1-ts-1)
                    << QPoint(x1-1, y1-1)
                    << QPoint(x1-ts-1, y1-1);
                p.drawPolygon(tri);
                p.setBrush(Qt::NoBrush);
                p.setPen(BLACK);
            }

            // Texte pendant drag
            if (drag_.active && drag_.hasMoved &&
                pad == drag_.pad && s == drag_.step) {
                QString txt;
                if (drag_.axis == Axis::Horizontal)
                    txt = (sd.pitch >= 0 ? "+" : "") + QString::number(sd.pitch);
                else if (drag_.axis == Axis::Vertical)
                    txt = QString::number(int(sd.volume * 100)) + "%";
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
        p.drawLine(markerX, y0+2, markerX, y1-2);
    }
}

// ── Mouse ─────────────────────────────────────────────────────────
void StepGrid::mousePressEvent(QMouseEvent* ev) {
    int mx  = ev->position().x();
    int my  = ev->position().y();
    int pad = padAtY(my);
    if (pad < 0) return;

    int zone = stepAtX(mx);

    if (ev->button() == Qt::LeftButton) {
        if (zone == -2) { emit trackGateToggled(pad);  return; }
        if (zone == -3) { emit trackMuteToggled(pad);  return; }
        if (zone == -4) { emit trackSoloToggled(pad);  return; }
    }

    int s = zone;
    if (s < 0) return;

    if (ev->button() == Qt::RightButton) {
        pattern_->setTrackLength(pad, s + 1);
        emit trackLengthChanged(pad, s + 1);
        update();
        return;
    }

    if (ev->button() == Qt::LeftButton) {
        const seq::StepData& sd = pattern_->grid[pad][s];
            drag_ = { true, pad, s, mx, my, sd.volume, sd.pitch, false, Axis::None };
    }

    if (ev->button() == Qt::RightButton && s >= 0) {
        rdrag_ = { true, pad, s, mx, false };
    }
}

void StepGrid::mouseMoveEvent(QMouseEvent* ev) {
    // ── Drag gauche : volume / pitch ─────────────────────────────
    if (drag_.active) {
        int dx = ev->position().x() - drag_.startX;
        int dy = ev->position().y() - drag_.startY;

        if (!drag_.hasMoved) {
            if (std::abs(dx) >= DRAG_THRESHOLD || std::abs(dy) >= DRAG_THRESHOLD) {
                drag_.hasMoved = true;
                drag_.axis = (std::abs(dx) >= std::abs(dy))
                             ? Axis::Horizontal : Axis::Vertical;
                pattern_->grid[drag_.pad][drag_.step].active = true;
            }
        }

        if (drag_.hasMoved) {
            seq::StepData& sd = pattern_->grid[drag_.pad][drag_.step];
            if (drag_.axis == Axis::Horizontal)
                sd.pitch = std::clamp(drag_.origPitch + dx / PITCH_PX, -12, 12);
            else
                sd.volume = std::clamp(drag_.origVol - float(dy) / VOL_PX, 0.0f, 1.0f);
            update();
        }
    }

    // ── Drag droit : longueur de track ───────────────────────────
    if (rdrag_.active) {
        int dx = ev->position().x() - rdrag_.startX;
        if (std::abs(dx) >= DRAG_THRESHOLD) {
            rdrag_.hasMoved = true;
            int s = stepAtX(ev->position().x());
            if (s >= 0) {
                pattern_->setTrackLength(rdrag_.pad, s + 1);
                emit trackLengthChanged(rdrag_.pad, s + 1);
                update();
            }
        }
    }
}

void StepGrid::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton && drag_.active) {
        if (!drag_.hasMoved)
            emit stepToggled(drag_.pad, drag_.step);
        else
            emit stepDataChanged(drag_.pad, drag_.step,
                                 pattern_->grid[drag_.pad][drag_.step]);
        drag_.active   = false;
        drag_.hasMoved = false;
        drag_.axis     = Axis::None;
        update();
    }

    if (ev->button() == Qt::RightButton && rdrag_.active) {
        if (!rdrag_.hasMoved) {
            // Clic droit pur → toggle gate
            pattern_->toggleGate(rdrag_.pad, rdrag_.step);
            emit stepGateToggled(rdrag_.pad, rdrag_.step);
        }
        rdrag_.active   = false;
        rdrag_.hasMoved = false;
        update();
    }
}

void StepGrid::mouseDoubleClickEvent(QMouseEvent* ev) {
    int mx  = ev->position().x();
    int my  = ev->position().y();
    int pad = padAtY(my);
    if (pad < 0) return;

    // Double-clic gauche sur label → reset longueur
    if (ev->button() == Qt::LeftButton && mx >= 0 && mx < LABEL_W) {
        pattern_->setTrackLength(pad, pattern_->patternLength);
        emit trackLengthChanged(pad, pattern_->patternLength);
        update();
    }
}

} // namespace wako::ui