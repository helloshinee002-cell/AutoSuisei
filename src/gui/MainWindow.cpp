#include "MainWindow.h"

#include <chrono>

#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMetaObject>
#include <QStatusBar>
#include <QStringListModel>
#include <QToolBar>

#include "player/IPlayer.h"
#include "recorder/IRecorder.h"
#include "storage/IMacroRepository.h"

namespace autopilot::gui {

namespace {

std::int64_t nowUnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

MainWindow::MainWindow(std::unique_ptr<recorder::IRecorder> rec,
                       std::unique_ptr<player::IPlayer> ply,
                       std::unique_ptr<storage::IMacroRepository> repo,
                       QWidget* parent)
    : QMainWindow(parent),
      recorder_(std::move(rec)),
      player_(std::move(ply)),
      repo_(std::move(repo)) {
    setWindowTitle("AutoPilot");
    resize(720, 480);

    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    recordAction_ = toolbar->addAction("● Record");
    stopAction_ = toolbar->addAction("■ Stop");
    toolbar->addSeparator();
    playAction_ = toolbar->addAction("▶ Play");
    deleteAction_ = toolbar->addAction("✕ Delete");

    stopAction_->setEnabled(false);

    connect(recordAction_, &QAction::triggered, this, &MainWindow::onRecordClicked);
    connect(stopAction_, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(playAction_, &QAction::triggered, this, &MainWindow::onPlayClicked);
    connect(deleteAction_, &QAction::triggered, this, &MainWindow::onDeleteClicked);

    listModel_ = new QStringListModel(this);
    listView_ = new QListView(this);
    listView_->setModel(listModel_);
    listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    setCentralWidget(listView_);

    statusLabel_ = new QLabel("Idle");
    statusBar()->addPermanentWidget(statusLabel_);

    refreshMacroList();
}

MainWindow::~MainWindow() {
    if (recorder_ && recorder_->isRecording()) {
        recorder_->stop();
    }
    if (player_) {
        player_->requestStop();
    }
}

void MainWindow::onRecorderAction(const core::Action& action) {
    pending_.actions.push_back(action);
    const int count = ++captureCount_;
    QMetaObject::invokeMethod(this, [this, count] {
        setStatus(QString("Recording — %1 actions").arg(count));
    }, Qt::QueuedConnection);
}

void MainWindow::onRecordClicked() {
    if (recorder_->isRecording()) return;

    pending_ = core::Macro{};
    captureCount_ = 0;
    pending_.createdAtUnixMs = nowUnixMs();

    recorder_->start([this](const core::Action& a) { onRecorderAction(a); });
    recordAction_->setEnabled(false);
    stopAction_->setEnabled(true);
    playAction_->setEnabled(false);
    setStatus("Recording — 0 actions");
}

void MainWindow::onStopClicked() {
    if (!recorder_->isRecording()) return;
    recorder_->stop();

    recordAction_->setEnabled(true);
    stopAction_->setEnabled(false);
    playAction_->setEnabled(true);

    if (pending_.actions.empty()) {
        setStatus("Idle (nothing recorded)");
        return;
    }

    bool ok = false;
    const auto name = QInputDialog::getText(this, "Save macro", "Name:",
                                            QLineEdit::Normal,
                                            QString("Macro %1").arg(macros_.size() + 1), &ok);
    if (!ok || name.isEmpty()) {
        setStatus("Idle (discarded)");
        return;
    }
    pending_.name = name.toStdString();
    pending_.updatedAtUnixMs = nowUnixMs();

    try {
        repo_->save(pending_);
        refreshMacroList();
        setStatus(QString("Saved '%1' (%2 actions)").arg(name).arg(pending_.actions.size()));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", QString::fromStdString(e.what()));
        setStatus("Idle (save failed)");
    }
}

void MainWindow::onPlayClicked() {
    const auto idx = listView_->currentIndex();
    if (!idx.isValid() || idx.row() < 0 ||
        idx.row() >= static_cast<int>(macros_.size())) {
        QMessageBox::information(this, "Play", "Select a macro first.");
        return;
    }
    const auto& macro = macros_.at(idx.row());

    setStatus(QString("Playing '%1'…").arg(QString::fromStdString(macro.name)));
    QApplication::processEvents();

    try {
        player_->play(macro, player::PlaybackOptions{});
        setStatus("Idle (playback done)");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Playback error", QString::fromStdString(e.what()));
        setStatus("Idle (playback error)");
    }
}

void MainWindow::onDeleteClicked() {
    const auto idx = listView_->currentIndex();
    if (!idx.isValid() || idx.row() < 0 ||
        idx.row() >= static_cast<int>(macros_.size())) {
        return;
    }
    const auto& macro = macros_.at(idx.row());

    if (QMessageBox::question(this, "Delete",
                              QString("Delete '%1'?").arg(QString::fromStdString(macro.name))) !=
        QMessageBox::Yes) {
        return;
    }
    repo_->remove(macro.id);
    refreshMacroList();
    setStatus("Idle");
}

void MainWindow::refreshMacroList() {
    macros_ = repo_->findAll();
    QStringList items;
    items.reserve(static_cast<int>(macros_.size()));
    for (const auto& m : macros_) {
        items << QString("%1  (%2 actions)")
                     .arg(QString::fromStdString(m.name))
                     .arg(m.actions.size());
    }
    listModel_->setStringList(items);
}

void MainWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace autopilot::gui
