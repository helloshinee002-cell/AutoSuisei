#include "MainWindow.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QWidget>

#include "OcrTab.h"
#include "ReviewTab.h"
#include "SidebarNav.h"
#include "Updater.h"
#include "WatchTab.h"
#include "player/IPlayer.h"
#include "recorder/IRecorder.h"
#include "storage/IMacroRepository.h"
#include "storage/IOcrResultRepository.h"

namespace autopilot::gui {

namespace {
constexpr int kOcrIdx = 0;
constexpr int kWatchIdx = 1;
constexpr int kReviewIdx = 2;

// ห่อ tab ด้วย scroll area — จอเล็กกว่าขนาด natural ของ tab → scroll แทนตัด/ซ้อน (responsive ทุก resolution)
QWidget* scrollWrap(QWidget* inner) {
    auto* sa = new QScrollArea;
    sa->setWidget(inner);
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    return sa;
}
}  // namespace

MainWindow::MainWindow(std::unique_ptr<recorder::IRecorder> rec,
                       std::unique_ptr<player::IPlayer> ply,
                       std::unique_ptr<storage::IMacroRepository> macroRepo,
                       std::unique_ptr<storage::IOcrResultRepository> ocrRepo,
                       QWidget* parent)
    : QMainWindow(parent),
      recorder_(std::move(rec)),
      player_(std::move(ply)),
      macroRepo_(std::move(macroRepo)),
      ocrRepo_(std::move(ocrRepo)) {
    setWindowTitle("AutoSuisei");
    // responsive: ไม่เปิดใหญ่เกิน work area ของจอ (กันปุ่มล่าง/status bar หลุดจอบน 1366×768 / high-DPI)
    setMinimumSize(760, 520);
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1180, static_cast<int>(avail.width() * 0.92)),
           qMin(820, static_cast<int>(avail.height() * 0.92)));

    auto* root = new QWidget();
    root->setObjectName("centralRoot");
    auto* layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* sidebar = new SidebarNav();
    auto* stack = new QStackedWidget();

    auto* ocrTab = new OcrTab(*ocrRepo_);
    auto* watchTab = new WatchTab();
    auto* reviewTab = new ReviewTab();
    stack->insertWidget(kOcrIdx, scrollWrap(ocrTab));
    stack->insertWidget(kWatchIdx, scrollWrap(watchTab));
    stack->insertWidget(kReviewIdx, scrollWrap(reviewTab));

    sidebar->addItem("OCR", "OCR Single", "Bulk extract images");
    sidebar->addItem("WCH", "Folder Watch", "Auto folder watcher");
    sidebar->addItem("REV", "Review", "Verify and rename");

    layout->addWidget(sidebar);
    layout->addWidget(stack, 1);
    setCentralWidget(root);

    auto* statusBar = this->statusBar();
    statusBar->setSizeGripEnabled(false);
    statusBar->showMessage("Ready — เลือก category เพื่อเริ่ม bulk extract");

    // Sidebar → stack page switch + status bar text per page
    connect(sidebar, &SidebarNav::currentChanged, this,
            [this, stack, ocrTab, watchTab, reviewTab](int idx) {
                stack->setCurrentIndex(idx);
                if (idx == kOcrIdx) this->statusBar()->showMessage(ocrTab->statusText());
                else if (idx == kWatchIdx) this->statusBar()->showMessage(watchTab->statusText());
                else if (idx == kReviewIdx) this->statusBar()->showMessage(reviewTab->statusText());
            });

    // Per-tab status updates flow to the single status bar
    connect(ocrTab, &OcrTab::statusChanged, this, [this](const QString& s) {
        this->statusBar()->showMessage(s);
    });
    connect(watchTab, &WatchTab::statusChanged, this, [this](const QString& s) {
        this->statusBar()->showMessage(s);
    });
    connect(reviewTab, &ReviewTab::statusChanged, this, [this](const QString& s) {
        this->statusBar()->showMessage(s);
    });

    // Bulk extract / Watch → Review tab พร้อม preview ภาพทันที
    auto toReview = [stack, sidebar, reviewTab](
                        const std::vector<ocr::AssetInfo>& infos,
                        const QString& folder) {
        reviewTab->loadFromExtraction(infos, folder);
        sidebar->setCurrentIndex(kReviewIdx);
        stack->setCurrentIndex(kReviewIdx);
    };
    connect(ocrTab, &OcrTab::sendToReviewRequested, this, toReview);
    connect(watchTab, &WatchTab::sendToReviewRequested, this, toReview);

    // ----- Check Version / Update (GitHub Releases) -----
    auto* updater = new Updater(this);
    connect(sidebar, &SidebarNav::checkUpdateRequested, this, [this, updater]() {
        this->statusBar()->showMessage("กำลังเช็คอัปเดตจาก GitHub…");
        updater->checkLatest();
    });
    connect(updater, &Updater::upToDate, this, [this](const QString& v) {
        this->statusBar()->showMessage("เป็นเวอร์ชันล่าสุดแล้ว");
        QMessageBox::information(this, "Check for updates",
                                 QString("ใช้เวอร์ชันล่าสุดอยู่แล้ว (v%1)").arg(v));
    });
    connect(updater, &Updater::updateAvailable, this,
            [this, updater](const QString& tag, const QString& notes) {
                const QString body =
                    QString("มีเวอร์ชันใหม่: %1\nกำลังใช้: v%2\n\n%3\n\nต้องการอัปเดตเลยไหม?")
                        .arg(tag, Updater::currentVersion(), notes.left(600));
                if (QMessageBox::question(this, "Update available", body) == QMessageBox::Yes) {
                    this->statusBar()->showMessage("กำลังดาวน์โหลดอัปเดต…");
                    updater->downloadAndApply();
                }
            });
    connect(updater, &Updater::checkFailed, this, [this](const QString& msg) {
        this->statusBar()->showMessage("เช็คอัปเดตไม่สำเร็จ");
        QMessageBox::warning(this, "Check for updates", msg);
    });
    connect(updater, &Updater::downloadProgress, this, [this](qint64 r, qint64 t) {
        if (t > 0) this->statusBar()->showMessage(QString("กำลังดาวน์โหลดอัปเดต %1%").arg(100 * r / t));
    });
    connect(updater, &Updater::applyFailed, this, [this](const QString& msg) {
        QMessageBox::critical(this, "Update", msg);
    });
}

MainWindow::~MainWindow() {
    if (recorder_ && recorder_->isRecording()) recorder_->stop();
    if (player_) player_->requestStop();
}

}  // namespace autopilot::gui
