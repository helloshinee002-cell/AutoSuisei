#include <filesystem>
#include <memory>

#include <QApplication>
#include <QStandardPaths>

#include "MainWindow.h"
#include "player/WindowsPlayer.h"
#include "recorder/WindowsRecorder.h"
#include "storage/SqliteMacroRepository.h"
#include "storage/SqliteOcrResultRepository.h"

namespace {

std::filesystem::path dataDir() {
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const std::filesystem::path dir = root.toStdString();
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName("AutoPilot");
    QApplication::setApplicationName("AutoPilot");
    QApplication app(argc, argv);

    const auto dir = dataDir();
    auto recorder = std::make_unique<autopilot::recorder::WindowsRecorder>();
    auto player = std::make_unique<autopilot::player::WindowsPlayer>();
    auto macroRepo = std::make_unique<autopilot::storage::SqliteMacroRepository>(
        (dir / "macros.sqlite").string());
    auto ocrRepo = std::make_unique<autopilot::storage::SqliteOcrResultRepository>(
        (dir / "ocr.sqlite").string());

    autopilot::gui::MainWindow window(std::move(recorder), std::move(player),
                                       std::move(macroRepo), std::move(ocrRepo));
    window.show();
    return app.exec();
}
