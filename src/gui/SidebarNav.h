#pragma once

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace autopilot::gui {

/**
 * Custom 240-px sidebar that replaces QTabWidget.
 * Brand header + 3 nav items (OCR / Folder Watch / Review), each with a
 * count badge on the right. Emits `currentChanged(int)` like QTabWidget.
 */
class SidebarNav : public QWidget {
    Q_OBJECT

public:
    explicit SidebarNav(QWidget* parent = nullptr);

    /** Add a navigation item. Returns its index. */
    int addItem(const QString& code,      ///< short badge text e.g. "OCR"
                const QString& label,     ///< e.g. "OCR Single"
                const QString& subtitle); ///< e.g. "Bulk extract images"

    /** Set count badge shown on the right of item `idx`. Empty string hides it. */
    void setBadge(int idx, const QString& text);

    /** Current selected index. */
    int currentIndex() const;
    void setCurrentIndex(int idx);

    /** ป้ายสถานะ AI ใต้ปุ่ม AI Settings (v1.0.5). */
    void setAiStatus(bool on);

signals:
    void currentChanged(int idx);
    /** ผู้ใช้กดปุ่ม "Check for updates" ที่มุมซ้ายล่าง. */
    void checkUpdateRequested();
    /** ผู้ใช้กดปุ่ม "AI Settings" ที่มุมซ้ายล่าง. */
    void aiSettingsRequested();

private:
    QListWidget* list_{};
    QList<QListWidgetItem*> items_;
    QPushButton* aiBtn_{};
};

}  // namespace autopilot::gui
