#pragma once

#include <vector>

#include <QStringList>
#include <QWidget>

#include "ocr/AssetExtractor.h"
#include "ocr/OcrEngine.h"

class QLabel;
class QProcess;
class QPushButton;
class QTableWidget;

namespace autopilot::storage { class IOcrResultRepository; }

namespace autopilot::gui {

enum class AssetCategory { PcLaptop, Monitor, Accessory };

/**
 * OCR tab: Category-aware bulk extraction
 *   PC&Laptop — open dialog, Dell Service Tag 7-char parser
 *   Monitor   — default folder C:/Users/hello/Downloads/Train Monitor, S/N CN-...-A00 parser
 *   Accessory — default folder C:/Users/hello/Downloads/Train Accessory, flexible parser
 */
class OcrTab : public QWidget {
    Q_OBJECT

public:
    explicit OcrTab(storage::IOcrResultRepository& repo, QWidget* parent = nullptr);

    /** Current status string — used by MainWindow when switching to this tab. */
    QString statusText() const { return statusText_; }

signals:
    /** ส่ง bulk extract results ไปให้ Review tab ตรวจสอบความถูกต้อง */
    void sendToReviewRequested(const std::vector<ocr::AssetInfo>& infos,
                                const QString& folder);

    /** ส่งข้อความสถานะไปแสดงที่ MainWindow status bar */
    void statusChanged(const QString& text);

private slots:
    void onPcLaptop();
    void onMonitor();
    void onAccessory();
    void onClear();
    void onSendToReview();
    void onStop();
    void onBulkStdout();
    void onBulkFinished(int exitCode);

private:
    void runCategory(AssetCategory category, const QString& defaultFolder);
    void processFolder(const QString& folder, AssetCategory category);
    void addAssetRow(const ocr::AssetInfo& info);
    void rebuildHeaders();
    void setStatus(const QString& text);

    storage::IOcrResultRepository& repo_;
    ocr::OcrEngine engine_;
    ocr::AssetExtractor extractor_;

    std::vector<ocr::AssetInfo> bulkResults_;
    QString bulkFolder_;
    AssetCategory bulkCategory_{AssetCategory::PcLaptop};
    QProcess* bulkProcess_{};
    QByteArray bulkStdoutBuf_;
    int bulkExpected_{0};
    int bulkProcessed_{0};

    QString statusText_;

    QTableWidget* table_{};
    QPushButton* pcLaptopBtn_{};
    QPushButton* monitorBtn_{};
    QPushButton* accessoryBtn_{};
    QPushButton* stopBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* sendToReviewBtn_{};
};

}  // namespace autopilot::gui
