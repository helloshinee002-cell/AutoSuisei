#include "MacrosTab.h"

#include <chrono>

#include <QApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QStringListModel>
#include <QVBoxLayout>

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

MacrosTab::MacrosTab(recorder::IRecorder& rec, player::IPlayer& ply,
                     storage::IMacroRepository& repo, QWidget* parent)
    : QWidget(parent), recorder_(rec), player_(ply), repo_(repo) {
    auto* root = new QVBoxLayout(this);

    auto* btnRow = new QHBoxLayout();
    recordBtn_ = new QPushButton("● Record");
    stopBtn_ = new QPushButton("■ Stop");
    playBtn_ = new QPushButton("▶ Play");
    deleteBtn_ = new QPushButton("✕ Delete");
    stopBtn_->setEnabled(false);
    btnRow->addWidget(recordBtn_);
    btnRow->addWidget(stopBtn_);
    btnRow->addSpacing(20);
    btnRow->addWidget(playBtn_);
    btnRow->addWidget(deleteBtn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    listModel_ = new QStringListModel(this);
    listView_ = new QListView();
    listView_->setModel(listModel_);
    listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(listView_, 1);

    statusLabel_ = new QLabel("Idle");
    root->addWidget(statusLabel_);

    connect(recordBtn_, &QPushButton::clicked, this, &MacrosTab::onRecord);
    connect(stopBtn_, &QPushButton::clicked, this, &MacrosTab::onStop);
    connect(playBtn_, &QPushButton::clicked, this, &MacrosTab::onPlay);
    connect(deleteBtn_, &QPushButton::clicked, this, &MacrosTab::onDelete);

    refreshList();
}

void MacrosTab::setStatus(const QString& s) { statusLabel_->setText(s); }

void MacrosTab::onAction(const core::Action& a) {
    pending_.actions.push_back(a);
    const int n = ++captureCount_;
    QMetaObject::invokeMethod(
        this, [this, n] { setStatus(QString("Recording — %1 actions").arg(n)); },
        Qt::QueuedConnection);
}

void MacrosTab::onRecord() {
    if (recorder_.isRecording()) return;
    pending_ = core::Macro{};
    pending_.createdAtUnixMs = nowUnixMs();
    captureCount_ = 0;
    recorder_.start([this](const core::Action& a) { onAction(a); });
    recordBtn_->setEnabled(false);
    stopBtn_->setEnabled(true);
    setStatus("Recording — 0 actions");
}

void MacrosTab::onStop() {
    if (!recorder_.isRecording()) return;
    recorder_.stop();
    recordBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);

    if (pending_.actions.empty()) {
        setStatus("Idle (nothing recorded)");
        return;
    }
    bool ok = false;
    const auto name = QInputDialog::getText(
        this, "Save macro", "Name:", QLineEdit::Normal,
        QString("Macro %1").arg(macros_.size() + 1), &ok);
    if (!ok || name.isEmpty()) {
        setStatus("Idle (discarded)");
        return;
    }
    pending_.name = name.toStdString();
    pending_.updatedAtUnixMs = nowUnixMs();
    try {
        repo_.save(pending_);
        refreshList();
        setStatus(QString("Saved '%1' (%2 actions)").arg(name).arg(pending_.actions.size()));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", QString::fromStdString(e.what()));
    }
}

void MacrosTab::onPlay() {
    const auto idx = listView_->currentIndex();
    if (!idx.isValid() || idx.row() < 0 ||
        idx.row() >= static_cast<int>(macros_.size())) {
        QMessageBox::information(this, "Play", "Select a macro first.");
        return;
    }
    const auto& m = macros_.at(idx.row());
    setStatus(QString("Playing '%1'…").arg(QString::fromStdString(m.name)));
    QApplication::processEvents();
    try {
        player_.play(m, player::PlaybackOptions{});
        setStatus("Idle (playback done)");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Playback error", QString::fromStdString(e.what()));
    }
}

void MacrosTab::onDelete() {
    const auto idx = listView_->currentIndex();
    if (!idx.isValid() || idx.row() < 0 ||
        idx.row() >= static_cast<int>(macros_.size())) return;
    const auto& m = macros_.at(idx.row());
    if (QMessageBox::question(
            this, "Delete",
            QString("Delete '%1'?").arg(QString::fromStdString(m.name))) != QMessageBox::Yes) {
        return;
    }
    repo_.remove(m.id);
    refreshList();
}

void MacrosTab::refreshList() {
    macros_ = repo_.findAll();
    QStringList items;
    items.reserve(static_cast<int>(macros_.size()));
    for (const auto& m : macros_) {
        items << QString("%1  (%2 actions)")
                     .arg(QString::fromStdString(m.name))
                     .arg(m.actions.size());
    }
    listModel_->setStringList(items);
}

}  // namespace autopilot::gui
