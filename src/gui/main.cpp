#include <filesystem>
#include <memory>

#include <QApplication>
#include <QStandardPaths>

#include "MainWindow.h"
#include "player/WindowsPlayer.h"
#include "recorder/WindowsRecorder.h"
#include "storage/SqliteMacroRepository.h"

namespace {

std::string resolveDbPath() {
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const std::filesystem::path dir = root.toStdString();
    std::filesystem::create_directories(dir);
    return (dir / "macros.sqlite").string();
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName("AutoPilot");
    QApplication::setApplicationName("AutoPilot");
    QApplication app(argc, argv);

    auto recorder = std::make_unique<autopilot::recorder::WindowsRecorder>();
    auto player = std::make_unique<autopilot::player::WindowsPlayer>();
    auto repo = std::make_unique<autopilot::storage::SqliteMacroRepository>(resolveDbPath());

    autopilot::gui::MainWindow window(std::move(recorder), std::move(player), std::move(repo));
    window.show();
    return app.exec();
}
