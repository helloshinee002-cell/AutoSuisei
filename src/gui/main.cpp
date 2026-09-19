#include <filesystem>
#include <memory>

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>
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
    // 0. บังคับธีมมืดเสมอ (v1.0.5) — theme.qss ออกแบบเป็น light-on-dark แต่ส่วนที่ qss ไม่ได้ระบุพื้น (tab ที่ห่อ
    //    QScrollArea, QDialog/QMessageBox) ใช้ system palette → Windows light mode = พื้นสว่าง ตัวอักษรอ่านไม่ออก.
    //    Fusion + palette สีเดียวกับ theme.qss = หน้าตาเหมือนกันทุกเครื่อง ไม่ขึ้นกับ light/dark ของ Windows
    app.setStyle("Fusion");
    const QColor bg("#0B0F0E"), surface("#13181B"), input("#0F1416"), text("#E5F4F0"),
        dim("#6B7B78"), accent("#10B981"), border("#1F2628");
    QPalette p;
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, input);
    p.setColor(QPalette::AlternateBase, surface);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, surface);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, surface);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, dim);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, bg);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::Mid, border);
    p.setColor(QPalette::Dark, border);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, dim);
    app.setPalette(p);

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
