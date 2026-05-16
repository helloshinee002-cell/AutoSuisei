#pragma once

#include <memory>

#include <QMainWindow>
#include <QStringListModel>

#include "core/Macro.h"

class QAction;
class QLabel;
class QListView;
class QStatusBar;

namespace autopilot::recorder { class IRecorder; }
namespace autopilot::player { class IPlayer; }
namespace autopilot::storage { class IMacroRepository; }

namespace autopilot::gui {

/**
 * หน้าจอหลักของ AutoPilot
 *
 * Toolbar: [Record] [Stop] [Play] [Delete]
 * Center : QListView ของ macro ที่ save ไว้ใน SQLite
 * Status : "Idle" / "Recording — N actions captured" / "Playing back"
 *
 * Lifecycle: GUI ถือ ownership ของ recorder/player/repo (DI ผ่าน ctor)
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(std::unique_ptr<recorder::IRecorder> rec,
               std::unique_ptr<player::IPlayer> ply,
               std::unique_ptr<storage::IMacroRepository> repo,
               QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRecordClicked();
    void onStopClicked();
    void onPlayClicked();
    void onDeleteClicked();

private:
    void refreshMacroList();
    void onRecorderAction(const core::Action& action);
    void setStatus(const QString& text);

    std::unique_ptr<recorder::IRecorder> recorder_;
    std::unique_ptr<player::IPlayer> player_;
    std::unique_ptr<storage::IMacroRepository> repo_;

    QListView* listView_{nullptr};
    QLabel* statusLabel_{nullptr};
    QAction* recordAction_{nullptr};
    QAction* stopAction_{nullptr};
    QAction* playAction_{nullptr};
    QAction* deleteAction_{nullptr};

    QStringListModel* listModel_{nullptr};
    std::vector<core::Macro> macros_;

    core::Macro pending_{};  ///< buffer ระหว่างกำลังบันทึก
    std::atomic<int> captureCount_{0};
};

}  // namespace autopilot::gui
