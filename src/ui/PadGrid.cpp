#include "PadGrid.h"
#include <QTimer>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>

namespace wako::ui {

static constexpr const char* STYLE_NORMAL =
    "QPushButton { background:#4D4D4D; color:#DCDCDC; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }"
    "QPushButton:pressed { background:#666; }";

static constexpr const char* STYLE_FLASH =
    "QPushButton { background:#FFA500; color:#1A1A1A; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }";

static constexpr const char* STYLE_DISABLED =
    "QPushButton { background:#323232; color:#666; border:2px solid black;"
    "font:bold 13px Dialog; border-radius:6px; }";

// Surbrillance verte quand un fichier audio passe au-dessus du pad
static constexpr const char* STYLE_DROP_HOVER =
    "QPushButton { background:#2A5C3A; color:#DCDCDC; border:2px solid #4CAF50;"
    "font:bold 13px Dialog; border-radius:6px; }";

static const QStringList AUDIO_EXTS = {
    "wav", "mp3", "flac", "aiff", "aif", "ogg"
};

static bool isAudioUrl(const QMimeData* mime) {
    if (!mime->hasUrls()) return false;
    const QString ext = QFileInfo(mime->urls().first().toLocalFile())
                        .suffix().toLower();
    return AUDIO_EXTS.contains(ext);
}

// ──────────────────────────────────────────────────────────────────
PadGrid::PadGrid(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dragOver_.fill(false);

    auto* grid = new QGridLayout(this);
    grid->setSpacing(6);
    grid->setContentsMargins(8, 8, 8, 8);

    const int rows[9] = {0,0,0, 1,1,1, 2,2,2};
    const int cols[9] = {0,1,2, 0,1,2, 0,1,2};

    for (int i = 0; i < 9; ++i) {
        auto* btn = new QPushButton(QString("Pad %1").arg(i + 1), this);
        btn->setFixedSize(100, 100);
        btn->setStyleSheet(STYLE_NORMAL);
        btn->setFocusPolicy(Qt::NoFocus);

        // Activer le drag & drop sur chaque bouton
        btn->setAcceptDrops(true);
        btn->installEventFilter(this);

        buttons_[i] = btn;
        grid->addWidget(btn, rows[i], cols[i]);

        connect(btn, &QPushButton::clicked, this, [this, i] {
            emit padTriggered(i);
            flashPad(i);
        });
    }
}

// ──────────────────────────────────────────────────────────────────
// eventFilter — gère les événements drag/drop sur les boutons
// ──────────────────────────────────────────────────────────────────
bool PadGrid::eventFilter(QObject* watched, QEvent* event) {
    for (int i = 0; i < 9; ++i) {
        if (watched != buttons_[i]) continue;

        switch (event->type()) {

        case QEvent::DragEnter: {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (isAudioUrl(e->mimeData())) {
                e->acceptProposedAction();
                dragOver_[i] = true;
                buttons_[i]->setStyleSheet(STYLE_DROP_HOVER);
            }
            return true;
        }

        case QEvent::DragLeave: {
            dragOver_[i] = false;
            buttons_[i]->setStyleSheet(padStyle(i));
            return true;
        }

        case QEvent::DragMove: {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (isAudioUrl(e->mimeData()))
                e->acceptProposedAction();
            return true;
        }

        case QEvent::Drop: {
            auto* e = static_cast<QDropEvent*>(event);
            dragOver_[i] = false;

            if (e->mimeData()->hasUrls()) {
                QString path = e->mimeData()->urls().first().toLocalFile();
                if (!path.isEmpty() &&
                    AUDIO_EXTS.contains(QFileInfo(path).suffix().toLower()))
                {
                    e->acceptProposedAction();
                    buttons_[i]->setStyleSheet(padStyle(i));
                    emit padFileDropped(i, path);
                    return true;
                }
            }
            buttons_[i]->setStyleSheet(padStyle(i));
            return true;
        }

        default:
            break;
        }
        break;
    }
    return QWidget::eventFilter(watched, event);
}

// ──────────────────────────────────────────────────────────────────
void PadGrid::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);

    const int spacing = 6;
    const int margin  = 16;
    int fromH = (ev->size().height() - margin * 2 - spacing * 2) / 3;
    int fromW = (ev->size().width()  - margin * 2 - spacing * 2) / 3;
    int padSize = std::max(60, std::min(fromH, fromW));

    for (auto* btn : buttons_)
        btn->setFixedSize(padSize, padSize);

    int gridSize = padSize * 3 + spacing * 2 + margin * 2;
    int offsetX  = std::max(0, (ev->size().width()  - gridSize) / 2);
    int offsetY  = std::max(0, (ev->size().height() - gridSize) / 2);
    layout()->setContentsMargins(offsetX, offsetY, offsetX, offsetY);
}

void PadGrid::refresh(const model::Kit* kit) {
    for (int i = 0; i < 9; ++i) {
        auto* btn = buttons_[i];
        const model::Pad* pad = kit ? kit->pad(i) : nullptr;
        if (pad) {
            btn->setText(QString::fromStdString(pad->displayName()));
            btn->setEnabled(pad->enabled);
            btn->setStyleSheet(pad->enabled ? STYLE_NORMAL : STYLE_DISABLED);
        } else {
            btn->setText("—");
            btn->setEnabled(false);
            btn->setStyleSheet(STYLE_DISABLED);
        }
    }
}

void PadGrid::flashPad(int idx) {
    if (idx < 0 || idx >= 9) return;
    auto* btn = buttons_[idx];
    btn->setStyleSheet(STYLE_FLASH);
    QTimer::singleShot(150, btn, [btn] {
        btn->setStyleSheet(STYLE_NORMAL);
    });
}

// ──────────────────────────────────────────────────────────────────
const char* PadGrid::padStyle(int idx) const {
    if (idx < 0 || idx >= 9) return STYLE_NORMAL;
    return buttons_[idx]->isEnabled() ? STYLE_NORMAL : STYLE_DISABLED;
}

} // namespace wako::ui