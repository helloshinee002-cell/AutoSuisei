#pragma once

#include <QString>
#include <QWidget>

#include "ocr/ReviewModel.h"

class QCheckBox;
class QLabel;
class QLineEdit;
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

private slots:
    void onLoadCsv();
    void onLoadFolder();
    void onTableRowClicked(int row, int column);
    void onApplyAndNext();
    void onSave();

private:
    void rebuildTable();
    void selectRow(int row);
    void loadImageForRow(int row);
    void updateStatus();

    ocr::ReviewModel model_;
    QString csvPath_;
    QString imagesFolder_;
    int currentRow_{-1};

    // left pane
    QPushButton* loadCsvBtn_{};
    QPushButton* loadFolderBtn_{};
    QPushButton* saveBtn_{};
    QLabel* csvLabel_{};
    QLabel* folderLabel_{};
    QTableWidget* table_{};
    QLabel* status_{};

    // right pane
    QLabel* imageView_{};
    QLineEdit* pcEdit_{};
    QLineEdit* serialEdit_{};
    QLineEdit* notesEdit_{};
    QCheckBox* verifiedCheck_{};
    QLabel* originalLabel_{};
    QPushButton* applyBtn_{};
};

}  // namespace autopilot::gui
