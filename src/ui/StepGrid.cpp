#include "StepGrid.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

namespace wako::ui {

static const QColor BG      {60,  63,  65};
static const QColor FG      {187, 187, 187};
static const QColor ORANGE  {255, 165, 0};
static const QColor RED     {200, 50,  50};
static const QColor DARK    {50,  50,  50};
static const QColor BLACK   {0,   0,   0};

StepGrid::StepGrid(std::shared_ptr<seq::Pattern> pattern, QWidget* parent)
    : QWidget(parent), pattern_(std::move(pattern))
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    padNames_.resize(seq::MAX_PADS);
    for (int i = 0; i < seq::MAX_PADS; ++i)
        padNames_[i] = "Pad " + std::to_string(i + 1);
}

void StepGrid::setCurrentStep(int step) {
    currentStep_ = step;
    update();   // repaint() thread-safe en Qt
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
// paintEvent — dessin direct, même logique que le proxy Clojure
// ──────────────────────────────────────────────────────────────────
void StepGrid::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);  // pixel-perfect pour les grilles

    const float cw = cellW();
    const float ch = cellH();
    const int w = width(), h = height();

    // Fond
    p.fillRect(0, 0, w, h, BG);

    // Headers numéros de steps
    p.setPen(FG);
    p.setFont(QFont("Dialog", 9));
    for (int s = 0; s < seq::MAX_STEPS; ++s) {
        int x = int(LABEL_W + s * cw);
        p.drawText(QRect(x, 0, int(cw), HEADER_H),
                   Qt::AlignCenter, QString::number(s + 1));
    }

    // Cellules
    for (int pad = 0; pad < seq::MAX_PADS; ++pad) {
        int y = int(HEADER_H + pad * ch);

        // Étiquette pad
        p.setPen(FG);
        p.setFont(QFont("Dialog", 10));
        p.drawText(QRect(2, y, LABEL_W - 4, int(ch)),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(padNames_[pad]));

        for (int s = 0; s < seq::MAX_STEPS; ++s) {
            int x  = int(LABEL_W + s * cw);
            bool active  = pattern_->get(pad, s);
            bool current = (s == currentStep_);

            QColor fill = current ? RED : (active ? ORANGE : DARK);
            p.fillRect(x + 1, y + 1, int(cw) - 2, int(ch) - 2, fill);

            p.setPen(BLACK);
            p.drawRect(x, y, int(cw), int(ch));
        }
    }
}

// ──────────────────────────────────────────────────────────────────
// mousePressEvent — toggle step
// ──────────────────────────────────────────────────────────────────
void StepGrid::mousePressEvent(QMouseEvent* ev) {
    int x = ev->position().x(), y = ev->position().y();
    if (x < LABEL_W || y < HEADER_H) return;

    int s = int((x - LABEL_W) / cellW());
    int p = int((y - HEADER_H) / cellH());

    if (s >= 0 && s < seq::MAX_STEPS && p >= 0 && p < seq::MAX_PADS)
        emit stepToggled(p, s);
}

} // namespace wako::ui
