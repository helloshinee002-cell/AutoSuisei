#pragma once

#include <vector>

#include <QStringList>
#include <QWidget>

#include "ocr/AssetExtractor.h"
#include "ocr/OcrEngine.h"

class QLabel;
class QProcess;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace autopilot::storage { class IOcrResultRepository; }

namespace autopilot::gui {

enum class AssetCategory { PcLaptop, Monitor, Accessory, Donate };

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

    /** v1.0.5: อัปเดตป้ายโหมด OCR/AI ตามค่า AI Settings ปัจจุบัน (เรียกหลังปิด dialog) */
    void refreshAiMode();

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
    void onDonate();
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
    /** ป้ายโหมด: ก่อนรัน = ตามค่า AI Settings, ระหว่าง/หลังรัน = ตามที่ Python ใช้จริง + นับ AI ตอบ/ล้มเหลว */
    void updateModeLabel();

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
    // v1.0.5 ป้ายโหมด + หลอดโหลด
    bool showRun_{false};    ///< true = ป้ายแสดงผลของรอบล่าสุด (ไม่ใช่ค่าที่ตั้งไว้)
    bool runAi_{false};      ///< Python ใช้ AI จริงในรอบนี้ (event start.ai)
    bool aiRejected_{false}; ///< GUI เปิด AI แต่ Python ปิด (key/URL ไม่ผ่าน)
    QString runModel_;
    int aiOk_{0};
    int aiFail_{0};
    QLabel* modeLabel_{};
    QProgressBar* progress_{};

    QString statusText_;

    QTableWidget* table_{};
    QPushButton* pcLaptopBtn_{};
    QPushButton* monitorBtn_{};
    QPushButton* accessoryBtn_{};
    QPushButton* donateBtn_{};
    QPushButton* stopBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* sendToReviewBtn_{};
};

}  // namespace autopilot::gui
