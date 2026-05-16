#pragma once

#include <memory>

#include <QMainWindow>

namespace autopilot::recorder { class IRecorder; }
namespace autopilot::player { class IPlayer; }
namespace autopilot::storage {
class IMacroRepository;
class IOcrResultRepository;
}

namespace autopilot::gui {

/**
 * Tabbed shell: [Macros] [OCR] [Web] [Image]
 * Owns recorder/player/repositories; passes references into tabs
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(std::unique_ptr<recorder::IRecorder> rec,
               std::unique_ptr<player::IPlayer> ply,
               std::unique_ptr<storage::IMacroRepository> macroRepo,
               std::unique_ptr<storage::IOcrResultRepository> ocrRepo,
               QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    std::unique_ptr<recorder::IRecorder> recorder_;
    std::unique_ptr<player::IPlayer> player_;
    std::unique_ptr<storage::IMacroRepository> macroRepo_;
    std::unique_ptr<storage::IOcrResultRepository> ocrRepo_;
};

}  // namespace autopilot::gui
