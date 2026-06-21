#pragma once

#include <vector>

#include <QString>
#include <QWidget>

#include "ocr/AssetExtractor.h"
#include "ocr/ReviewModel.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace autopilot::gui {

/**
 * Review tab — โหลด CSV จาก scripts/bulk_extract.py + โฟลเดอร์ภาพ
 * คลิก row ในตาราง → preview ภาพ → แก้ pc_no/serial_no/notes แล้วกด "Apply + Next"
 * เพื่อ verify + ข้ามไปแถวต่อไป — กด Save เพื่อ export ground_truth.csv
 */
class ReviewTab : public QWidget {
    Q_OBJECT

public:
    explicit ReviewTab(QWidget* parent = nullptr);

    QString statusText() const { return statusText_; }

    /** โหลดผล bulk extract เข้า ReviewModel ตรง ๆ (ไม่ต้อง save/load CSV) */
    void loadFromExtraction(const std::vector<ocr::AssetInfo>& infos,
                            const QString& folder);

signals:
    void statusChanged(const QString& text);

private slots:
    void onLoadCsv();
    void onLoadFolder();
    void onTableRowClicked(int row, int column);
    void onApplyAndNext();
    void onSave();
    void onRename();
    void onClear();

private:
    void rebuildTable();
    void selectRow(int row);
    void loadImageForRow(int row);
    void updateStatus();
    void setStatus(const QString& text);

    ocr::ReviewModel model_;
    /** Mirrors ReviewModel rows 1:1 when loaded via extraction — gives access
     *  to batch/date that aren't in ReviewModel itself. Empty when CSV-loaded. */
    std::vector<ocr::AssetInfo> sourceInfos_;
    QString csvPath_;
    QString imagesFolder_;
    int currentRow_{-1};
    QString statusText_;

    // top buttons
    QPushButton* loadCsvBtn_{};
    QPushButton* loadFolderBtn_{};
    QPushButton* saveBtn_{};
    QPushButton* clearBtn_{};
    QLabel* csvLabel_{};
    QLabel* folderLabel_{};

    // progress
    QProgressBar* progressBar_{};
    QLabel* progressLabel_{};

    // table
    QTableWidget* table_{};

    // right pane: image + form
    QLabel* imageView_{};
    QLineEdit* pcEdit_{};
    QLineEdit* serialEdit_{};
    QLineEdit* orgEdit_{};
    QLineEdit* batchEdit_{};
    QLineEdit* dateEdit_{};
    QLineEdit* notesEdit_{};
    QCheckBox* verifiedCheck_{};
    QLabel* originalLabel_{};
    QPushButton* applyBtn_{};

    // rename bar
    QCheckBox* renamePcCheck_{};
    QCheckBox* renameSerialCheck_{};
    QCheckBox* renameNotesCheck_{};
    QPushButton* renameBtn_{};
};

}  // namespace autopilot::gui
