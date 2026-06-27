#include <filesystem>
#include <memory>

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
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

void applyTheme(QApplication& app) {
    // 1. Bundled font (ADLaM Display) — Thai fallback handled by Qt's font substitution
    const int fid = QFontDatabase::addApplicationFont(
        ":/resources/fonts/ADLaMDisplay-Regular.ttf");
    if (fid >= 0) {
        const auto family = QFontDatabase::applicationFontFamilies(fid).value(0);
        QFont base(family.isEmpty() ? "ADLaM Display" : family, 11);
        base.setStyleStrategy(QFont::PreferAntialias);
        app.setFont(base);
    }
    // Substitute Thai glyphs (ADLaM Display lacks them) → Leelawadee UI / Tahoma
    QFont::insertSubstitutions("ADLaM Display",
                                {"Leelawadee UI", "Tahoma", "Sarabun", "Loma"});

    // 2. QSS stylesheet from Qt resource
    QFile qss(":/theme.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName("AutoSuisei");
    QApplication::setApplicationName("AutoSuisei");
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/AutoSuisei.ico"));  // titlebar (ซ้ายบน) + taskbar = exe icon
    applyTheme(app);

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
