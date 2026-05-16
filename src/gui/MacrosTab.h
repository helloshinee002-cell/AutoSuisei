#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <QWidget>

#include "core/Macro.h"

class QLabel;
class QListView;
class QPushButton;
class QStringListModel;

namespace autopilot::recorder { class IRecorder; }
namespace autopilot::player { class IPlayer; }
namespace autopilot::storage { class IMacroRepository; }

namespace autopilot::gui {

/**
 * Desktop macro recorder tab — record keyboard+mouse → save → replay
 */
class MacrosTab : public QWidget {
    Q_OBJECT

public:
    MacrosTab(recorder::IRecorder& rec, player::IPlayer& ply,
              storage::IMacroRepository& repo, QWidget* parent = nullptr);

private slots:
    void onRecord();
    void onStop();
    void onPlay();
    void onDelete();

private:
    void refreshList();
    void onAction(const core::Action& a);
    void setStatus(const QString& s);

    recorder::IRecorder& recorder_;
    player::IPlayer& player_;
    storage::IMacroRepository& repo_;

    QPushButton* recordBtn_{};
    QPushButton* stopBtn_{};
    QPushButton* playBtn_{};
    QPushButton* deleteBtn_{};
    QListView* listView_{};
    QStringListModel* listModel_{};
    QLabel* statusLabel_{};

    core::Macro pending_{};
    std::atomic<int> captureCount_{0};
    std::vector<core::Macro> macros_;
};

}  // namespace autopilot::gui
