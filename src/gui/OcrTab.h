#pragma once

#include <vector>

#include <QStringList>
#include <QWidget>

#include "ocr/OcrEngine.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace autopilot::storage { class IOcrResultRepository; }

namespace autopilot::gui {

/**
 * OCR tab: drag-drop images → batch OCR → save + table
 * เคสตรงตามที่ user ขอ: ภาพชื่อแปลก → อ่านเลข → จับคู่ filename ↔ content
 */
class OcrTab : public QWidget {
    Q_OBJECT

public:
    explicit OcrTab(storage::IOcrResultRepository& repo, QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onBrowse();
    void onExport();
    void onClear();

private:
    void processFiles(const QStringList& paths);
    void addRow(const ocr::OcrResult& r);

    storage::IOcrResultRepository& repo_;
    ocr::OcrEngine engine_;

    QTableWidget* table_{};
    QLabel* status_{};
    QPushButton* browseBtn_{};
    QPushButton* exportBtn_{};
    QPushButton* clearBtn_{};
};

}  // namespace autopilot::gui
