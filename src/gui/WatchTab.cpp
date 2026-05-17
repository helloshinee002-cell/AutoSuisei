#include "WatchTab.h"

#include <filesystem>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#ifndef AUTOPILOT_SCRIPTS_DIR
#define AUTOPILOT_SCRIPTS_DIR ""
#endif

namespace autopilot::gui {

namespace {

constexpr int kCols = 5;

bool looksLikeImage(const QString& path) {
    static const QStringList exts = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp"};
    for (const auto& e : exts)
        if (path.endsWith(e, Qt::CaseInsensitive)) return true;
    return false;
}

QString scriptsDir() {
    auto env = qEnvironmentVariable("AUTOPILOT_SCRIPTS_DIR");
    return env.isEmpty() ? QStringLiteral(AUTOPILOT_SCRIPTS_DIR) : env;
}

}  // namespace

WatchTab::WatchTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    chooseBtn_ = new QPushButton("Choose folder…");
    watchBtn_ = new QPushButton("Start watching");
    watchBtn_->setEnabled(false);
    clearBtn_ = new QPushButton("Clear");
    sendBtn_ = new QPushButton("Send to Review →");
    sendBtn_->setEnabled(false);
    topRow->addWidget(chooseBtn_);
    topRow->addWidget(watchBtn_);
    topRow->addStretch();
    topRow->addWidget(clearBtn_);
    topRow->addWidget(sendBtn_);
    root->addLayout(topRow);

    folderLabel_ = new QLabel("Folder: (none)");
    root->addWidget(folderLabel_);

    table_ = new QTableWidget(0, kCols, this);
    table_->setHorizontalHeaderLabels(
        {"#", "File", "PC No.", "Serial", "Confidence"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    root->addWidget(table_, 1);

    statusLabel_ = new QLabel("Idle");
    root->addWidget(statusLabel_);

    connect(chooseBtn_, &QPushButton::clicked, this, &WatchTab::onChooseFolder);
    connect(watchBtn_, &QPushButton::clicked, this, &WatchTab::onToggleWatch);
    connect(clearBtn_, &QPushButton::clicked, this, &WatchTab::onClear);
    connect(sendBtn_, &QPushButton::clicked, this, &WatchTab::onSendToReview);

    fsWatcher_ = new QFileSystemWatcher(this);
    connect(fsWatcher_, &QFileSystemWatcher::directoryChanged,
            this, &WatchTab::onDirChanged);
}

WatchTab::~WatchTab() { stopWorker(); }

void WatchTab::onChooseFolder() {
    const auto path = QFileDialog::getExistingDirectory(
        this, "Choose folder to watch", folder_);
    if (path.isEmpty()) return;
    if (watching_) stopWorker();
    folder_ = path;
    folderLabel_->setText("Folder: " + path);
    watchBtn_->setEnabled(true);
    // เก็บ snapshot ของไฟล์ที่มีอยู่แล้ว เพื่อให้ "Start watching" ถือว่า
    // ไฟล์เหล่านี้ "เห็นแล้ว" — เฉพาะไฟล์ที่มาใหม่หลัง Start เท่านั้นที่ประมวลผล
    seenFiles_.clear();
    QDir dir(folder_);
    for (const auto& entry : dir.entryInfoList(QDir::Files)) {
        if (looksLikeImage(entry.fileName()))
            seenFiles_.insert(entry.absoluteFilePath());
    }
    statusLabel_->setText(QString("Folder set — %1 ไฟล์เดิม (จะข้าม)")
                              .arg(seenFiles_.size()));
}

void WatchTab::onToggleWatch() {
    if (watching_) {
        stopWorker();
        watching_ = false;
        watchBtn_->setText("Start watching");
        statusLabel_->setText("Stopped");
    } else {
        if (folder_.isEmpty()) return;
        startWorker();
    }
}

void WatchTab::startWorker() {
    const QString script = QDir(scriptsDir()).filePath("ocr_worker.py");
    if (!QFileInfo::exists(script)) {
        QMessageBox::critical(this, "Script not found",
                              "ไม่เจอ ocr_worker.py ที่: " + script);
        return;
    }

    worker_ = new QProcess(this);
    worker_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(worker_, &QProcess::readyReadStandardOutput,
            this, &WatchTab::onWorkerStdout);
    connect(worker_, &QProcess::readyReadStandardError,
            this, &WatchTab::onWorkerStderr);
    connect(worker_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                onWorkerFinished(code);
            });

    statusLabel_->setText("กำลังโหลด PaddleOCR (1-2 วินาที)…");
    worker_->start("python", {script});
    if (!worker_->waitForStarted(5000)) {
        QMessageBox::critical(this, "Python not found",
                              "ไม่สามารถ start python ได้ — เช็คว่ามี python ใน PATH");
        delete worker_;
        worker_ = nullptr;
        return;
    }

    fsWatcher_->addPath(folder_);
    watching_ = true;
    watchBtn_->setText("Stop watching");
    // ตรวจหาไฟล์ใหม่ทันที (เผื่อมีของที่มาก่อนเริ่มจริง)
    QTimer::singleShot(500, this, &WatchTab::scanForNew);
}

void WatchTab::stopWorker() {
    if (worker_) {
        if (worker_->state() == QProcess::Running) {
            worker_->write("QUIT\n");
            worker_->closeWriteChannel();
            if (!worker_->waitForFinished(2000)) worker_->kill();
        }
        worker_->deleteLater();
        worker_ = nullptr;
    }
    if (fsWatcher_ && !folder_.isEmpty()) fsWatcher_->removePath(folder_);
    workerBusy_ = false;
    pendingQueue_.clear();
}

void WatchTab::onClear() {
    table_->setRowCount(0);
    results_.clear();
    sendBtn_->setEnabled(false);
    statusLabel_->setText(watching_ ? "Watching…" : "Idle");
}

void WatchTab::onSendToReview() {
    if (results_.empty() || folder_.isEmpty()) return;
    emit sendToReviewRequested(results_, folder_);
}

void WatchTab::onDirChanged(const QString& /*path*/) {
    // ดี-bounce: QFileSystemWatcher ส่งซิกแนล "directoryChanged" หลายรอบขณะ
    // copy ไฟล์ใหญ่ → รอ 700ms ก่อน scan เพื่อหวังว่า file write เสร็จ
    QTimer::singleShot(700, this, &WatchTab::scanForNew);
}

void WatchTab::scanForNew() {
    if (folder_.isEmpty()) return;
    QDir dir(folder_);
    for (const auto& entry : dir.entryInfoList(QDir::Files)) {
        if (!looksLikeImage(entry.fileName())) continue;
        const auto abs = entry.absoluteFilePath();
        if (seenFiles_.contains(abs)) continue;
        seenFiles_.insert(abs);
        enqueue(abs);
    }
}

void WatchTab::enqueue(const QString& path) {
    pendingQueue_.enqueue(path);
    statusLabel_->setText(QString("Queue: %1 รอ / %2 เสร็จ")
                              .arg(pendingQueue_.size())
                              .arg(results_.size()));
    pumpQueue();
}

void WatchTab::pumpQueue() {
    if (workerBusy_ || pendingQueue_.isEmpty()) return;
    if (!worker_ || worker_->state() != QProcess::Running) return;
    const QString next = pendingQueue_.dequeue();
    workerBusy_ = true;
    statusLabel_->setText(QString("Processing %1").arg(QFileInfo(next).fileName()));
    worker_->write((next + "\n").toUtf8());
}

void WatchTab::onWorkerStdout() {
    if (!worker_) return;
    stdoutBuf_ += worker_->readAllStandardOutput();
    while (true) {
        const int nl = stdoutBuf_.indexOf('\n');
        if (nl < 0) break;
        const auto line = stdoutBuf_.left(nl).trimmed();
        stdoutBuf_.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        try {
            auto j = nlohmann::json::parse(line.toStdString());
            const auto event = j.value("event", std::string{});
            if (event == "ready") {
                statusLabel_->setText("Watching (model loaded)…");
            } else if (event == "result" || event == "error") {
                ocr::AssetInfo info;
                info.filename = j.value("filename", "");
                info.pcNo = j.value("pc_no", "");
                info.serialNo = j.value("serial_no", "");
                info.batchId = j.value("batch_id", "");
                info.photoDate = j.value("photo_date", "");
                info.photoIndex = j.value("photo_index", 0);
                info.pcRange = j.value("pc_range", "");
                if (j.contains("mean_confidence"))
                    info.ocrConfidence = j["mean_confidence"].get<float>();
                if (j.contains("error"))
                    info.warnings.push_back(j["error"].get<std::string>());
                appendResult(info);
                workerBusy_ = false;
                pumpQueue();
            }
        } catch (const std::exception&) {
            // bad JSON line — ignore
        }
    }
}

void WatchTab::onWorkerStderr() {
    // โยน stderr ทิ้ง (Python progress msgs) — ป้องกัน pipe เต็ม
    if (worker_) worker_->readAllStandardError();
}

void WatchTab::onWorkerFinished(int exitCode) {
    if (watching_) {
        statusLabel_->setText(
            QString("Python จบ exit=%1 — กด Start เพื่อเริ่มใหม่").arg(exitCode));
        watching_ = false;
        watchBtn_->setText("Start watching");
        if (fsWatcher_ && !folder_.isEmpty()) fsWatcher_->removePath(folder_);
    }
    workerBusy_ = false;
}

void WatchTab::appendResult(const ocr::AssetInfo& info) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(QString::number(info.photoIndex)));
    table_->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(
                    std::filesystem::path(info.filename).filename().string())));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(info.pcNo)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(info.serialNo)));
    table_->setItem(row, 4,
                    new QTableWidgetItem(QString::number(info.ocrConfidence, 'f', 2)));
    table_->scrollToBottom();
    results_.push_back(info);
    sendBtn_->setEnabled(true);
    statusLabel_->setText(
        QString("เสร็จ %1 ภาพ (queue เหลือ %2)")
            .arg(results_.size()).arg(pendingQueue_.size()));
}

}  // namespace autopilot::gui
