#include "WatchTab.h"

#include <filesystem>

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
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

constexpr int kCols = 6;

bool looksLikeImage(const QString& path) {
    static const QStringList exts = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp"};
    for (const auto& e : exts)
        if (path.endsWith(e, Qt::CaseInsensitive)) return true;
    return false;
}

QString scriptsDir() {
    auto fromEnv = qEnvironmentVariable("AUTOPILOT_SCRIPTS_DIR");
    if (!fromEnv.isEmpty() && QDir(fromEnv).exists()) return fromEnv;
    const auto baked = QStringLiteral(AUTOPILOT_SCRIPTS_DIR);
    if (!baked.isEmpty() && QDir(baked).exists()) return baked;
    return QDir(QCoreApplication::applicationDirPath()).filePath("scripts");
}

QString pythonExe() {
    const QString bundled = QDir(QCoreApplication::applicationDirPath())
                                .filePath("python/python.exe");
    if (QFileInfo::exists(bundled)) return bundled;
    return QStringLiteral("python");
}

/** Helper — สร้าง KPI card 1 ใบ (label เล็กด้านบน + value ใหญ่ด้านล่าง) */
QFrame* makeKpiCard(const QString& labelText, QLabel*& valueLabel,
                     const QString& valueObjectName = "kpiValue") {
    auto* card = new QFrame();
    card->setObjectName("kpiCard");
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(4);
    auto* lbl = new QLabel(labelText);
    lbl->setObjectName("kpiLabel");
    valueLabel = new QLabel("0");
    valueLabel->setObjectName(valueObjectName);
    lay->addWidget(lbl);
    lay->addWidget(valueLabel);
    return card;
}

}  // namespace

WatchTab::WatchTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(12);

    // ----- Header -----
    auto* title = new QLabel("Folder Watch");
    title->setObjectName("tabTitle");
    auto* subtitle = new QLabel("Auto-process new files in watched folder");
    subtitle->setObjectName("tabSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    // ----- Top button row -----
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);
    chooseBtn_ = new QPushButton("Choose folder…");
    watchBtn_ = new QPushButton("Start watching");
    watchBtn_->setObjectName("primaryButton");
    watchBtn_->setEnabled(false);
    clearBtn_ = new QPushButton("Clear");
    sendBtn_ = new QPushButton("Send to Review");
    sendBtn_->setEnabled(false);
    topRow->addWidget(chooseBtn_);
    topRow->addWidget(watchBtn_);
    topRow->addStretch();
    topRow->addWidget(sendBtn_);
    topRow->addWidget(clearBtn_);
    root->addLayout(topRow);

    // ----- Folder + LIVE indicator row -----
    auto* folderRow = new QHBoxLayout();
    folderRow->setSpacing(8);
    auto* folderLbl = new QLabel("Watching folder:");
    folderLbl->setObjectName("dimLabel");
    folderLabel_ = new QLabel("(none)");
    folderRow->addWidget(folderLbl);
    folderRow->addWidget(folderLabel_, 1);
    liveLabel_ = new QLabel("● Idle");
    liveLabel_->setStyleSheet("color: #6B7B78; font-size: 12px;");
    folderRow->addWidget(liveLabel_);
    root->addLayout(folderRow);

    // ----- KPI cards row -----
    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(12);
    kpiRow->addWidget(makeKpiCard("Total", kpiTotal_, "kpiValue"));
    kpiRow->addWidget(makeKpiCard("Today", kpiToday_, "kpiValue"));
    kpiRow->addWidget(makeKpiCard("Avg conf.", kpiAvgConf_, "kpiValueAccent"));
    kpiRow->addWidget(makeKpiCard("Pending rev.", kpiPending_, "kpiValueDanger"));
    root->addLayout(kpiRow);

    // ----- Table -----
    table_ = new QTableWidget(0, kCols, this);
    table_->setHorizontalHeaderLabels(
        {"#", "File", "No.", "Serial", "Confidence", "Date"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setColumnWidth(0, 50);
    table_->setColumnWidth(2, 110);
    table_->setColumnWidth(3, 220);
    table_->setColumnWidth(4, 100);
    table_->setColumnWidth(5, 100);
    root->addWidget(table_, 1);

    setStatus("Idle");
    refreshKpis();

    connect(chooseBtn_, &QPushButton::clicked, this, &WatchTab::onChooseFolder);
    connect(watchBtn_, &QPushButton::clicked, this, &WatchTab::onToggleWatch);
    connect(clearBtn_, &QPushButton::clicked, this, &WatchTab::onClear);
    connect(sendBtn_, &QPushButton::clicked, this, &WatchTab::onSendToReview);

    fsWatcher_ = new QFileSystemWatcher(this);
    connect(fsWatcher_, &QFileSystemWatcher::directoryChanged,
            this, &WatchTab::onDirChanged);
}

WatchTab::~WatchTab() { stopWorker(); }

void WatchTab::setStatus(const QString& text) {
    statusText_ = text;
    emit statusChanged(text);
}

void WatchTab::refreshKpis() {
    if (kpiTotal_) kpiTotal_->setText(QString::number(results_.size()));

    // "Today" = entries with photoDate == today (YYYY-MM-DD)
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    int todayCount = 0;
    double confSum = 0.0;
    int confCount = 0;
    int pending = 0;
    for (const auto& r : results_) {
        if (QString::fromStdString(r.photoDate) == today) ++todayCount;
        if (r.ocrConfidence > 0.0f) { confSum += r.ocrConfidence; ++confCount; }
        if (r.pcNo.empty() || r.serialNo.empty()) ++pending;
    }
    if (kpiToday_) kpiToday_->setText(QString::number(todayCount));
    if (kpiAvgConf_) {
        const double pct = confCount ? 100.0 * confSum / confCount : 0.0;
        kpiAvgConf_->setText(confCount ? QString::number(pct, 'f', 1) + "%" : "—");
    }
    if (kpiPending_) kpiPending_->setText(QString::number(pending));
}

void WatchTab::onChooseFolder() {
    const auto path = QFileDialog::getExistingDirectory(
        this, "Choose folder to watch", folder_);
    if (path.isEmpty()) return;
    if (watching_) stopWorker();
    folder_ = path;
    folderLabel_->setText(path);
    watchBtn_->setEnabled(true);
    seenFiles_.clear();
    QDir dir(folder_);
    for (const auto& entry : dir.entryInfoList(QDir::Files)) {
        if (looksLikeImage(entry.fileName()))
            seenFiles_.insert(entry.absoluteFilePath());
    }
    setStatus(QString("Folder set — %1 ไฟล์เดิม (จะข้าม)")
                  .arg(seenFiles_.size()));
}

void WatchTab::onToggleWatch() {
    if (watching_) {
        stopWorker();
        watching_ = false;
        watchBtn_->setText("Start watching");
        liveLabel_->setText("● Idle");
        liveLabel_->setStyleSheet("color: #6B7B78; font-size: 12px;");
        setStatus("Stopped");
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

    setStatus("กำลังโหลด PaddleOCR (1-2 วินาที)…");
    worker_->start(pythonExe(), {script});
    if (!worker_->waitForStarted(5000)) {
        QMessageBox::critical(this, "Python not found",
                              "ไม่สามารถ start python ได้ — เช็คว่ามี python ใน PATH "
                              "หรือ bundle ที่ <exeDir>/python/python.exe");
        delete worker_;
        worker_ = nullptr;
        return;
    }

    fsWatcher_->addPath(folder_);
    watching_ = true;
    watchBtn_->setText("Stop watching");
    liveLabel_->setText("● LIVE");
    liveLabel_->setStyleSheet("color: #10B981; font-size: 12px; font-weight: 600;");
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
    refreshKpis();
    setStatus(watching_ ? "Watching…" : "Idle");
}

void WatchTab::onSendToReview() {
    if (results_.empty() || folder_.isEmpty()) return;
    emit sendToReviewRequested(results_, folder_);
}

void WatchTab::onDirChanged(const QString& /*path*/) {
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
    setStatus(QString("Queue: %1 รอ / %2 เสร็จ")
                  .arg(pendingQueue_.size()).arg(results_.size()));
    pumpQueue();
}

void WatchTab::pumpQueue() {
    if (workerBusy_ || pendingQueue_.isEmpty()) return;
    if (!worker_ || worker_->state() != QProcess::Running) return;
    const QString next = pendingQueue_.dequeue();
    workerBusy_ = true;
    setStatus(QString("Processing %1").arg(QFileInfo(next).fileName()));
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
                setStatus("Watching (model loaded)…");
            } else if (event == "result" || event == "error") {
                ocr::AssetInfo info;
                info.filename = j.value("filename", "");
                info.pcNo = j.value("pc_no", "");
                info.serialNo = j.value("serial_no", "");
                info.serialSource = j.value("serial_source", "");
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
    if (worker_) worker_->readAllStandardError();
}

void WatchTab::onWorkerFinished(int exitCode) {
    if (watching_) {
        setStatus(QString("Python จบ exit=%1 — กด Start เพื่อเริ่มใหม่").arg(exitCode));
        watching_ = false;
        watchBtn_->setText("Start watching");
        liveLabel_->setText("● Idle");
        liveLabel_->setStyleSheet("color: #6B7B78; font-size: 12px;");
        if (fsWatcher_ && !folder_.isEmpty()) fsWatcher_->removePath(folder_);
    }
    workerBusy_ = false;
}

void WatchTab::appendResult(const ocr::AssetInfo& info) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    // # column = sequential row number (1, 2, 3, ...) เพื่อให้เรียงตามลำดับ
    // ที่ ภาพถูกประมวลผล ไม่ใช่ photoIndex จากชื่อไฟล์
    table_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    table_->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(
                    std::filesystem::path(info.filename).filename().string())));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(info.pcNo)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(info.serialNo)));
    table_->setItem(row, 4,
                    new QTableWidgetItem(QString::number(info.ocrConfidence, 'f', 2)));
    table_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(info.photoDate)));
    table_->scrollToBottom();
    results_.push_back(info);
    sendBtn_->setEnabled(true);
    refreshKpis();
    setStatus(QString("เสร็จ %1 ภาพ (queue เหลือ %2)")
                  .arg(results_.size()).arg(pendingQueue_.size()));
}

}  // namespace autopilot::gui
