#pragma once

#include <vector>

#include <QQueue>
#include <QSet>
#include <QString>
#include <QWidget>

#include "ocr/AssetExtractor.h"

class QFileSystemWatcher;
class QLabel;
class QProcess;
class QPushButton;
class QTableWidget;

namespace autopilot::gui {

/**
 * Watch tab — เลือกโฟลเดอร์ → ติดตามไฟล์ใหม่ → auto-OCR + extract → ขึ้นในตาราง
 *
 * ใช้ QFileSystemWatcher จับการเปลี่ยน + long-running Python worker
 * (`scripts/ocr_worker.py`) เพื่อไม่ต้องโหลด PaddleOCR ใหม่ทุกภาพ
 */
class WatchTab : public QWidget {
    Q_OBJECT

public:
    explicit WatchTab(QWidget* parent = nullptr);
    ~WatchTab() override;

signals:
    /** ส่งผลทั้งหมดที่เก็บได้ไปให้ Review tab */
    void sendToReviewRequested(const std::vector<ocr::AssetInfo>& infos,
                                const QString& folder);

private slots:
    void onChooseFolder();
    void onToggleWatch();
    void onClear();
    void onSendToReview();
    void onDirChanged(const QString& path);
    void onWorkerStdout();
    void onWorkerStderr();
    void onWorkerFinished(int exitCode);

private:
    void startWorker();
    void stopWorker();
    void scanForNew();
    void enqueue(const QString& path);
    void pumpQueue();
    void appendResult(const ocr::AssetInfo& info);

    QString folder_;
    bool watching_{false};
    QSet<QString> seenFiles_;
    QQueue<QString> pendingQueue_;
    bool workerBusy_{false};

    QProcess* worker_{};
    QFileSystemWatcher* fsWatcher_{};
    QByteArray stdoutBuf_;

    std::vector<ocr::AssetInfo> results_;

    // UI
    QPushButton* chooseBtn_{};
    QPushButton* watchBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* sendBtn_{};
    QLabel* folderLabel_{};
    QLabel* statusLabel_{};
    QTableWidget* table_{};
};

}  // namespace autopilot::gui
