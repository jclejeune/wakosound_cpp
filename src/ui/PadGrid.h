#pragma once
#include "../model/KitManager.h"
#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <array>

namespace wako::ui {

class PadGrid : public QWidget {
    Q_OBJECT
public:
    explicit PadGrid(QWidget* parent = nullptr);

    void refresh(const model::Kit* kit);
    void flashPad(int idx);

signals:
    void padTriggered(int padIdx);

    // Émis quand un fichier audio est déposé sur un pad via drag & drop.
    void padFileDropped(int padIdx, const QString& filePath);

protected:
    void resizeEvent(QResizeEvent*) override;

    // Intercepte les événements drag/drop sur chaque QPushButton.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Retourne le style approprié pour le pad (normal / disabled / highlight).
    const char* padStyle(int idx) const;

    std::array<QPushButton*, 9> buttons_{};

    // Indique si le pad est actuellement survolé par un drag.
    std::array<bool, 9> dragOver_{};
};

} // namespace wako::ui