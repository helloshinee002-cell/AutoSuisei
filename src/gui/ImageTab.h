#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace autopilot::gui {

/**
 * Image-match tab: หา needle (template) ใน haystack (เช่น screenshot)
 * แสดงผล: ตำแหน่ง + confidence + ปุ่ม "Copy click coords"
 */
class ImageTab : public QWidget {
    Q_OBJECT

public:
    explicit ImageTab(QWidget* parent = nullptr);

private slots:
    void onPickHaystack();
    void onPickNeedle();
    void onMatch();
    void onCopyCoords();

private:
    QLineEdit* haystackPath_{};
    QLineEdit* needlePath_{};
    QPushButton* matchBtn_{};
    QPushButton* copyBtn_{};
    QLabel* resultLabel_{};

    int lastCenterX_{-1};
    int lastCenterY_{-1};
};

}  // namespace autopilot::gui
