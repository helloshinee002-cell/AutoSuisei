#include "MainWindow.h"

#include <QTabWidget>

#include "OcrTab.h"
#include "ReviewTab.h"
#include "WatchTab.h"
#include "player/IPlayer.h"
#include "recorder/IRecorder.h"
#include "storage/IMacroRepository.h"
#include "storage/IOcrResultRepository.h"

namespace autopilot::gui {

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
    setWindowTitle("AutoPilot");
    resize(900, 600);

    auto* tabs = new QTabWidget(this);
    auto* ocrTab = new OcrTab(*ocrRepo_);
    auto* watchTab = new WatchTab();
    auto* reviewTab = new ReviewTab();
    tabs->addTab(ocrTab, "OCR");
    tabs->addTab(watchTab, "Watch");
    const int reviewIdx = tabs->addTab(reviewTab, "Review");
    setCentralWidget(tabs);

    // OCR bulk extract / Watch → Review tab พร้อม preview ภาพทันที
    auto toReview = [tabs, reviewTab, reviewIdx](
                        const std::vector<ocr::AssetInfo>& infos,
                        const QString& folder) {
        reviewTab->loadFromExtraction(infos, folder);
        tabs->setCurrentIndex(reviewIdx);
    };
    connect(ocrTab, &OcrTab::sendToReviewRequested, this, toReview);
    connect(watchTab, &WatchTab::sendToReviewRequested, this, toReview);
}

MainWindow::~MainWindow() {
    if (recorder_ && recorder_->isRecording()) recorder_->stop();
    if (player_) player_->requestStop();
}

}  // namespace autopilot::gui
