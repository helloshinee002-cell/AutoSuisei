#include "MainWindow.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QStatusBar>
#include <QWidget>

#include "OcrTab.h"
#include "ReviewTab.h"
#include "SidebarNav.h"
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
    resize(1080, 760);

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
    stack->insertWidget(kOcrIdx, ocrTab);
    stack->insertWidget(kWatchIdx, watchTab);
    stack->insertWidget(kReviewIdx, reviewTab);

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
}

MainWindow::~MainWindow() {
    if (recorder_ && recorder_->isRecording()) recorder_->stop();
    if (player_) player_->requestStop();
}

}  // namespace autopilot::gui
