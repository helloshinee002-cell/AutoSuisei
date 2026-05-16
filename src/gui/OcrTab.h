#pragma once

#include <vector>

#include <QStringList>
#include <QWidget>

#include "ocr/AssetExtractor.h"
#include "ocr/OcrEngine.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace autopilot::storage { class IOcrResultRepository; }

namespace autopilot::gui {

/**
 * OCR tab: 2 modes
 *   - Drop / Browse images → simple OCR + save to repo
 *   - Bulk Extract Folder → OCR + AssetExtractor regex → CSV (PC No., Serial, ...)
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
    void onBulkFolder();
    void onExport();
    void onClear();

private:
    void processFiles(const QStringList& paths);
    void processFolder(const QString& folder);
    void addRow(const ocr::OcrResult& r);
    void addAssetRow(const ocr::AssetInfo& info);
    void rebuildHeaders(bool assetMode);

    storage::IOcrResultRepository& repo_;
    ocr::OcrEngine engine_;
    ocr::AssetExtractor extractor_;
    bool assetMode_{false};  ///< true เมื่ออยู่ใน bulk mode (header เปลี่ยนเป็น PC No./Serial)

    QTableWidget* table_{};
    QLabel* status_{};
    QPushButton* browseBtn_{};
    QPushButton* bulkBtn_{};
    QPushButton* exportBtn_{};
    QPushButton* clearBtn_{};
};

}  // namespace autopilot::gui
