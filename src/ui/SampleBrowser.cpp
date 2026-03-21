#include "SampleBrowser.h"
#include "SvgIcons.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QToolButton>

namespace wako::ui {

// ──────────────────────────────────────────────────────────────────
// DraggableFileList
// ──────────────────────────────────────────────────────────────────
class DraggableFileList : public QListWidget {
public:
    explicit DraggableFileList(QWidget* p = nullptr) : QListWidget(p) {
        setDragEnabled(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDragDropMode(QAbstractItemView::DragOnly);
    }
protected:
    void startDrag(Qt::DropActions) override {
        QListWidgetItem* item = currentItem();
        if (!item) return;
        QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;
        auto* mime = new QMimeData;
        mime->setUrls({QUrl::fromLocalFile(path)});
        QPixmap pm = icons::render(icons::MUSIC_NOTE, 24, "#FFA500");
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(pm);
        drag->setHotSpot({12, 12});
        drag->exec(Qt::CopyAction);
    }
};

// ──────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────
static const QStringList AUDIO_EXTS = {"wav","mp3","flac","aiff","aif","ogg"};

bool SampleBrowser::isAudioFile(const QString& fname) {
    return AUDIO_EXTS.contains(QFileInfo(fname).suffix().toLower());
}

static const QString STYLE = R"(
QTreeView, QListWidget {
    background: #252527;
    color: #BBBBBB;
    border: none;
    font-size: 12px;
    outline: none;
}
QTreeView::item:selected, QListWidget::item:selected {
    background: #3D4A5C;
    color: #FFFFFF;
}
QTreeView::item:hover, QListWidget::item:hover {
    background: #303035;
}
QHeaderView::section {
    background: #252527; color: #666;
    border: none; font-size: 10px;
}
QToolButton {
    background: transparent; border: none;
}
QToolButton:hover { background: #3C3C40; border-radius: 3px; }
QSplitter::handle { background: #333; }
)";

// ──────────────────────────────────────────────────────────────────
// SampleBrowser
// ──────────────────────────────────────────────────────────────────
SampleBrowser::SampleBrowser(QWidget* parent) : QWidget(parent) {
    setStyleSheet(STYLE);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Header ────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(28);
    header->setStyleSheet("QWidget { background: #2A2A2D; }");
    auto* headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(10, 0, 6, 0);

    auto* titleLbl = new QLabel("Browser");
    titleLbl->setStyleSheet(
        "QLabel { color: #AAAAAA; font-size: 11px; font-weight: bold; }");

    // Bouton browse — taille explicite pour éviter l'écrasement CSS
    auto* browseBtn = new QToolButton;
    browseBtn->setIcon(icons::icon(icons::FOLDER_OPEN, 16, "#BBBBBB"));
    browseBtn->setIconSize({16, 16});
    browseBtn->setFixedSize(26, 26);
    browseBtn->setToolTip("Changer de dossier");
    connect(browseBtn, &QToolButton::clicked, this, &SampleBrowser::onBrowse);

    headerLay->addWidget(titleLbl, 1);
    headerLay->addWidget(browseBtn);
    root->addWidget(header);

    // ── Chemin courant ────────────────────────────────────────────
    pathLabel_ = new QLabel("—");
    pathLabel_->setStyleSheet(
        "QLabel { color: #666; font-size: 10px; padding: 2px 8px; "
        "background: #1E1E20; }");
    pathLabel_->setWordWrap(false);
    root->addWidget(pathLabel_);

    // ── Splitter : arbre dossiers | liste fichiers ────────────────
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->setHandleWidth(3);

    dirModel_ = new QFileSystemModel(this);
    dirModel_->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);

    dirTree_ = new QTreeView;
    dirTree_->setModel(dirModel_);
    dirTree_->setHeaderHidden(true);
    dirTree_->hideColumn(1); dirTree_->hideColumn(2); dirTree_->hideColumn(3);
    dirTree_->setIndentation(12);
    dirTree_->setAnimated(false);
    connect(dirTree_->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SampleBrowser::onDirChanged);

    fileList_ = new DraggableFileList;
    fileList_->setIconSize({13, 13});
    fileList_->setSpacing(0);
    connect(fileList_, &QListWidget::itemActivated,
            this, &SampleBrowser::onFileActivated);

    splitter->addWidget(dirTree_);
    splitter->addWidget(fileList_);
    splitter->setSizes({150, 200});
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);

    root->addWidget(splitter, 1);

    // ── Hint ──────────────────────────────────────────────────────
    auto* hint = new QLabel("dbl-clic → écoute  ·  glisser → pad");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(
        "QLabel { color: #555; font-size: 9px; padding: 4px; "
        "background: #1E1E20; }");
    root->addWidget(hint);

    // Dossier par défaut
    QString def = QDir::currentPath() + "/sounds";
    if (!QDir(def).exists()) def = QDir::currentPath();
    setRootPath(def);
}

void SampleBrowser::setRootPath(const QString& path) {
    QModelIndex root = dirModel_->setRootPath(path);
    dirTree_->setRootIndex(root);
    dirTree_->setCurrentIndex(root);
    pathLabel_->setText(path);
    pathLabel_->setToolTip(path);
    populateFileList(path);
}

void SampleBrowser::onDirChanged(const QModelIndex& current, const QModelIndex&) {
    if (!current.isValid()) return;
    QString path = dirModel_->filePath(current);
    pathLabel_->setText(path);
    pathLabel_->setToolTip(path);
    populateFileList(path);
}

void SampleBrowser::populateFileList(const QString& dirPath) {
    fileList_->clear();
    QDir dir(dirPath);
    if (!dir.exists()) return;

    QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot,
                                        QDir::Name | QDir::IgnoreCase);
    QIcon noteIcon = icons::icon(icons::MUSIC_NOTE, 13, "#5B8DB8");
    int count = 0;
    for (const QString& fname : entries) {
        if (!isAudioFile(fname)) continue;
        auto* item = new QListWidgetItem(noteIcon, fname);
        item->setData(Qt::UserRole, dir.absoluteFilePath(fname));
        item->setToolTip(dir.absoluteFilePath(fname));
        fileList_->addItem(item);
        ++count;
    }
    if (count == 0) {
        auto* ph = new QListWidgetItem("  aucun fichier audio");
        ph->setFlags(Qt::NoItemFlags);
        ph->setForeground(QColor(70, 70, 70));
        fileList_->addItem(ph);
    }
}

void SampleBrowser::onFileActivated(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) emit samplePreviewRequested(path);
}

void SampleBrowser::onBrowse() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Choisir un dossier de samples",
        pathLabel_->toolTip().isEmpty() ? QDir::currentPath() : pathLabel_->toolTip());
    if (!dir.isEmpty()) setRootPath(dir);
}

} // namespace wako::ui