#include "MainWindow.h"

#include <QTabWidget>

#include "ImageTab.h"
#include "MacrosTab.h"
#include "OcrTab.h"
#include "WebTab.h"
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
    tabs->addTab(new MacrosTab(*recorder_, *player_, *macroRepo_), "Macros");
    tabs->addTab(new OcrTab(*ocrRepo_), "OCR");
    tabs->addTab(new WebTab(*macroRepo_), "Web");
    tabs->addTab(new ImageTab(), "Image Click");
    setCentralWidget(tabs);
}

MainWindow::~MainWindow() {
    if (recorder_ && recorder_->isRecording()) recorder_->stop();
    if (player_) player_->requestStop();
}

}  // namespace autopilot::gui
