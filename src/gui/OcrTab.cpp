#include "OcrTab.h"

#include <filesystem>


#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include "storage/IOcrResultRepository.h"

#ifndef AUTOPILOT_SCRIPTS_DIR
#define AUTOPILOT_SCRIPTS_DIR ""
#endif

namespace autopilot::gui {

namespace {

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

const char* categoryFlag(AssetCategory c) {
    switch (c) {
        case AssetCategory::PcLaptop:  return "pc";
        case AssetCategory::Monitor:   return "monitor";
        case AssetCategory::Accessory: return "accessory";
        case AssetCategory::Donate:    return "donate";
    }
    return "pc";
}

const char* categoryLabel(AssetCategory c) {
    switch (c) {
        case AssetCategory::PcLaptop:  return "PC&Laptop";
        case AssetCategory::Monitor:   return "Monitor";
        case AssetCategory::Accessory: return "Accessory";
        case AssetCategory::Donate:    return "Donate";
    }
    return "PC&Laptop";
}

}  // namespace

OcrTab::OcrTab(storage::IOcrResultRepository& repo, QWidget* parent)
    : QWidget(parent),
      repo_(repo),
      engine_(ocr::OcrOptions{.languages = "eng+tha"}),
      extractor_(engine_) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(12);

    // ----- Header section -----
    auto* title = new QLabel("OCR Single Image");
    title->setObjectName("tabTitle");
    auto* subtitle = new QLabel("Bulk extract No. and Serial by category");
    subtitle->setObjectName("tabSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    // ----- Table -----
    table_ = new QTableWidget(0, 7);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rebuildHeaders();
    root->addWidget(table_, 1);

    // ----- Button row -----
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    pcLaptopBtn_ = new QPushButton("PC&&Laptop");  // && → literal &
    pcLaptopBtn_->setToolTip("เลือกโฟลเดอร์ภาพ PC/Laptop — ใช้ Dell Service Tag parser");
    monitorBtn_ = new QPushButton("Monitor");
    monitorBtn_->setToolTip("Default: C:/Users/hello/Downloads/Train Monitor — S/N CN-...-A00 parser");
    accessoryBtn_ = new QPushButton("Accessory");
    accessoryBtn_->setToolTip("Default: C:/Users/hello/Downloads/Train Accessory — flexible parser");
    donateBtn_ = new QPushButton("Donate");
    donateBtn_->setToolTip("Default: C:/Users/hello/Downloads/Train Donate — No. + Service Tag "
                           "+ ชื่อโรงเรียน/สถานที่ (Thai via Tesseract)");
    stopBtn_ = new QPushButton("Stop");
    stopBtn_->setEnabled(false);
    stopBtn_->setToolTip("ยกเลิกการประมวลผล — ผลที่ทำไปแล้วยังอยู่ในตาราง");
    sendToReviewBtn_ = new QPushButton("Send to Review");
    sendToReviewBtn_->setObjectName("primaryButton");
    sendToReviewBtn_->setEnabled(false);
    sendToReviewBtn_->setToolTip("เปิดผล bulk extract ใน Review tab เพื่อตรวจสอบ");
    clearBtn_ = new QPushButton("Clear");
    btnRow->addWidget(pcLaptopBtn_);
    btnRow->addWidget(monitorBtn_);
    btnRow->addWidget(accessoryBtn_);
    btnRow->addWidget(donateBtn_);
    btnRow->addSpacing(20);
    btnRow->addWidget(stopBtn_);
    btnRow->addStretch();
    btnRow->addWidget(sendToReviewBtn_);
    btnRow->addWidget(clearBtn_);
    root->addLayout(btnRow);

    setStatus("Ready — เลือก category เพื่อเริ่ม bulk extract");

    connect(pcLaptopBtn_, &QPushButton::clicked, this, &OcrTab::onPcLaptop);
    connect(monitorBtn_, &QPushButton::clicked, this, &OcrTab::onMonitor);
    connect(accessoryBtn_, &QPushButton::clicked, this, &OcrTab::onAccessory);
    connect(donateBtn_, &QPushButton::clicked, this, &OcrTab::onDonate);
    connect(stopBtn_, &QPushButton::clicked, this, &OcrTab::onStop);
    connect(sendToReviewBtn_, &QPushButton::clicked, this, &OcrTab::onSendToReview);
    connect(clearBtn_, &QPushButton::clicked, this, &OcrTab::onClear);
}

void OcrTab::rebuildHeaders() {
    table_->setRowCount(0);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {"#", "File", "No.", "Serial", "Batch", "Date"});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setColumnWidth(0, 50);
    table_->setColumnWidth(2, 110);
    table_->setColumnWidth(3, 200);
    table_->setColumnWidth(4, 90);
    table_->setColumnWidth(5, 110);
}

void OcrTab::setStatus(const QString& text) {
    statusText_ = text;
    emit statusChanged(text);
}

void OcrTab::onPcLaptop() {
    runCategory(AssetCategory::PcLaptop, QString{});
}

void OcrTab::onMonitor() {
    runCategory(AssetCategory::Monitor,
                "C:/Users/hello/Downloads/Train Monitor");
}

void OcrTab::onAccessory() {
    runCategory(AssetCategory::Accessory,
                "C:/Users/hello/Downloads/Train Accessory");
}

void OcrTab::onDonate() {
    runCategory(AssetCategory::Donate,
                "C:/Users/hello/Downloads/Train Donate");
}

void OcrTab::runCategory(AssetCategory category, const QString& defaultFolder) {
    const QString folder = QFileDialog::getExistingDirectory(
        this, QString("Pick %1 image folder").arg(categoryLabel(category)),
        defaultFolder);
    if (folder.isEmpty()) return;
    processFolder(folder, category);
}

void OcrTab::processFolder(const QString& folder, AssetCategory category) {
    if (bulkProcess_ && bulkProcess_->state() != QProcess::NotRunning) {
        QMessageBox::information(this, "Busy",
                                 "ยังประมวลผลรอบก่อนหน้าอยู่ รอให้เสร็จก่อน");
        return;
    }

    const QString script = QDir(scriptsDir()).filePath("bulk_extract.py");
    if (script.isEmpty() || !QFileInfo::exists(script)) {
        QMessageBox::critical(
            this, "Script not found",
            QString("ไม่เจอ bulk_extract.py ที่ %1\n\nตั้ง AUTOPILOT_SCRIPTS_DIR env "
                    "ให้ชี้ไปที่โฟลเดอร์ scripts ของ AutoSuisei")
                .arg(script));
        return;
    }

    rebuildHeaders();
    bulkResults_.clear();
    bulkFolder_ = folder;
    bulkCategory_ = category;
    sendToReviewBtn_->setEnabled(false);
    pcLaptopBtn_->setEnabled(false);
    monitorBtn_->setEnabled(false);
    accessoryBtn_->setEnabled(false);
    donateBtn_->setEnabled(false);
    stopBtn_->setEnabled(true);
    bulkStdoutBuf_.clear();
    bulkExpected_ = 0;
    bulkProcessed_ = 0;

    namespace fs = std::filesystem;
    const auto tmpCsv = (fs::temp_directory_path() / "autopilot_bulk.csv").string();

    if (!bulkProcess_) {
        bulkProcess_ = new QProcess(this);
        bulkProcess_->setProcessChannelMode(QProcess::SeparateChannels);
        connect(bulkProcess_, &QProcess::readyReadStandardOutput,
                this, &OcrTab::onBulkStdout);
        connect(bulkProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) { onBulkFinished(code); });
    }

    setStatus(QString("[%1] กำลังโหลด PaddleOCR…").arg(categoryLabel(category)));
    const QString categoryArg = QString("--category=%1").arg(categoryFlag(category));
    bulkProcess_->start(pythonExe(), {script, folder,
                                       QString::fromStdString(tmpCsv),
                                       "--progress-json",
                                       categoryArg});
    if (!bulkProcess_->waitForStarted(5000)) {
        setStatus("ไม่สามารถ start python ได้ — เช็คว่ามี python ใน PATH "
                   "หรือ bundle ที่ <exeDir>/python/python.exe");
        pcLaptopBtn_->setEnabled(true);
        monitorBtn_->setEnabled(true);
        accessoryBtn_->setEnabled(true);
        donateBtn_->setEnabled(true);
    }
}

void OcrTab::onBulkStdout() {
    if (!bulkProcess_) return;
    bulkStdoutBuf_ += bulkProcess_->readAllStandardOutput();
    while (true) {
        const int nl = bulkStdoutBuf_.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = bulkStdoutBuf_.left(nl).trimmed();
        bulkStdoutBuf_.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        try {
            auto j = nlohmann::json::parse(line.toStdString());
            const std::string event = j.value("event", "");
            if (event == "start") {
                bulkExpected_ = j.value("total", 0);
                setStatus(QString("[%1] เจอ %2 ภาพ • กำลังโหลดโมเดล…")
                              .arg(categoryLabel(bulkCategory_))
                              .arg(bulkExpected_));
            } else if (event == "ready") {
                setStatus(QString("[%1] เริ่มประมวลผล %2 ภาพ…")
                              .arg(categoryLabel(bulkCategory_))
                              .arg(bulkExpected_));
            } else if (event == "row") {
                ocr::AssetInfo info;
                info.filename = j.value("filename", "");
                info.pcNo = j.value("pc_no", "");
                info.serialNo = j.value("serial_no", "");
                info.serialSource = j.value("serial_source", "");
                info.batchId = j.value("batch_id", "");
                info.photoDate = j.value("photo_date", "");
                info.photoIndex = j.value("photo_index", 0);
                info.pcRange = j.value("pc_range", "");
                if (j.contains("mean_confidence")) {
                    info.ocrConfidence = j["mean_confidence"].get<float>();
                }
                if (j.contains("error")) {
                    info.warnings.push_back(j["error"].get<std::string>());
                }
                addAssetRow(info);
                bulkResults_.push_back(std::move(info));
                bulkProcessed_ = j.value("i", bulkProcessed_ + 1);
                const int withPc = j.value("with_pc", 0);
                const int withSn = j.value("with_sn", 0);
                setStatus(QString("[%1] ประมวลผล %2/%3 • No#=%4 Serial=%5")
                              .arg(categoryLabel(bulkCategory_))
                              .arg(bulkProcessed_).arg(bulkExpected_)
                              .arg(withPc).arg(withSn));
            } else if (event == "done") {
                const int withPc = j.value("with_pc", 0);
                const int withSn = j.value("with_sn", 0);
                const int total = j.value("total", bulkProcessed_);
                const double pcRate = total ? 100.0 * withPc / total : 0.0;
                const double snRate = total ? 100.0 * withSn / total : 0.0;
                setStatus(QString("[%1] เสร็จ %2 ภาพ — No. %3% • Serial %4%")
                              .arg(categoryLabel(bulkCategory_))
                              .arg(total)
                              .arg(pcRate, 0, 'f', 1)
                              .arg(snRate, 0, 'f', 1));
            }
        } catch (const std::exception&) {
            // ignore bad JSON (probably stderr leaked)
        }
    }
}

void OcrTab::onBulkFinished(int exitCode) {
    if (!bulkStdoutBuf_.isEmpty()) {
        bulkStdoutBuf_ += '\n';
        onBulkStdout();
    }
    pcLaptopBtn_->setEnabled(true);
    monitorBtn_->setEnabled(true);
    accessoryBtn_->setEnabled(true);
    donateBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);
    sendToReviewBtn_->setEnabled(!bulkResults_.empty());
    if (exitCode != 0) {
        const auto err = bulkProcess_ ? QString::fromUtf8(
                                            bulkProcess_->readAllStandardError())
                                       : QString();
        setStatus(QString("Python จบแบบ exit code %1 — %2")
                      .arg(exitCode)
                      .arg(err.left(200)));
    }
}

void OcrTab::addAssetRow(const ocr::AssetInfo& info) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    // # column = sequential row number (1, 2, 3, ...) — not the filename's
    // photo_index (which sorts alphabetically: 1, 10, 100, 101, ...).
    table_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    table_->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(
                    std::filesystem::path(info.filename).filename().string())));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(info.pcNo)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(info.serialNo)));
    table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(info.batchId)));
    table_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(info.photoDate)));
}

void OcrTab::onClear() {
    rebuildHeaders();
    bulkResults_.clear();
    bulkFolder_.clear();
    sendToReviewBtn_->setEnabled(false);
    setStatus("Cleared");
}

void OcrTab::onSendToReview() {
    if (bulkResults_.empty() || bulkFolder_.isEmpty()) return;
    emit sendToReviewRequested(bulkResults_, bulkFolder_);
}

void OcrTab::onStop() {
    if (!bulkProcess_) return;
    bulkProcess_->kill();
    setStatus(QString("[%1] ยกเลิกแล้วที่ %2/%3 — ผล %4 ภาพ เก็บไว้ในตาราง")
                  .arg(categoryLabel(bulkCategory_))
                  .arg(bulkProcessed_).arg(bulkExpected_)
                  .arg(bulkResults_.size()));
}

}  // namespace autopilot::gui
