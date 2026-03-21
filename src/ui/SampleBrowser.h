#pragma once
#include <QWidget>
#include <QFileSystemModel>
#include <QTreeView>
#include <QListWidget>
#include <QLabel>

namespace wako::ui {

class SampleBrowser : public QWidget {
    Q_OBJECT
public:
    explicit SampleBrowser(QWidget* parent = nullptr);

    void setRootPath(const QString& path);

signals:
    void samplePreviewRequested(const QString& filePath);

private slots:
    void onDirChanged(const QModelIndex& current, const QModelIndex&);
    void onFileActivated(QListWidgetItem* item);
    void onBrowse();

private:
    void populateFileList(const QString& dirPath);
    static bool isAudioFile(const QString& fname);

    QFileSystemModel* dirModel_  = nullptr;
    QTreeView*        dirTree_   = nullptr;
    QListWidget*      fileList_  = nullptr;
    QLabel*           pathLabel_ = nullptr;
};

} // namespace wako::ui