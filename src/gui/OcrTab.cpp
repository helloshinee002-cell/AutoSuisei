#include "OcrTab.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
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

std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool looksLikeImage(const QString& path) {
    const QStringList exts = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp"};
    for (const auto& e : exts) {
        if (path.endsWith(e, Qt::CaseInsensitive)) return true;
    }
    return false;
}

QString scriptsDir() {
    auto fromEnv = qEnvironmentVariable("AUTOPILOT_SCRIPTS_DIR");
    if (!fromEnv.isEmpty()) return fromEnv;
    return QStringLiteral(AUTOPILOT_SCRIPTS_DIR);
}

}  // namespace

OcrTab::OcrTab(storage::IOcrResultRepository& repo, QWidget* parent)
    : QWidget(parent),
      repo_(repo),
      engine_(ocr::OcrOptions{.languages = "eng+tha"}),
      extractor_(engine_) {
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);

    auto* hint = new QLabel("◉ Drop image files here, or use buttons below");
    hint->setStyleSheet("padding: 14px; border: 2px dashed #888;");
    hint->setAlignment(Qt::AlignCenter);
    root->addWidget(hint);

    table_ = new QTableWidget(0, 4);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rebuildHeaders(false);
    root->addWidget(table_, 1);

    auto* btnRow = new QHBoxLayout();
    browseBtn_ = new QPushButton("Browse files…");
    bulkBtn_ = new QPushButton("Bulk Extract Folder (PC No. / Serial)");
    exportBtn_ = new QPushButton("Export JSON…");
    sendToReviewBtn_ = new QPushButton("Send to Review →");
    sendToReviewBtn_->setEnabled(false);
    sendToReviewBtn_->setToolTip("เปิดผล bulk extract ใน Review tab เพื่อตรวจสอบ");
    clearBtn_ = new QPushButton("Clear");
    btnRow->addWidget(browseBtn_);
    btnRow->addWidget(bulkBtn_);
    btnRow->addWidget(exportBtn_);
    btnRow->addWidget(sendToReviewBtn_);
    btnRow->addWidget(clearBtn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    status_ = new QLabel("Ready (Tesseract eng+tha)");
    root->addWidget(status_);

    connect(browseBtn_, &QPushButton::clicked, this, &OcrTab::onBrowse);
    connect(bulkBtn_, &QPushButton::clicked, this, &OcrTab::onBulkFolder);
    connect(exportBtn_, &QPushButton::clicked, this, &OcrTab::onExport);
    connect(sendToReviewBtn_, &QPushButton::clicked, this, &OcrTab::onSendToReview);
    connect(clearBtn_, &QPushButton::clicked, this, &OcrTab::onClear);
}

void OcrTab::rebuildHeaders(bool assetMode) {
    assetMode_ = assetMode;
    table_->setRowCount(0);
    if (assetMode) {
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels(
            {"#", "File", "PC No.", "Serial", "Batch", "Date"});
    } else {
        table_->setColumnCount(4);
        table_->setHorizontalHeaderLabels({"File", "Text", "Digits", "Confidence"});
    }
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(
        assetMode ? 1 : 1, QHeaderView::Stretch);
}

void OcrTab::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void OcrTab::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        const auto p = url.toLocalFile();
        if (looksLikeImage(p)) paths << p;
    }
    if (!paths.isEmpty()) processFiles(paths);
}

void OcrTab::onBrowse() {
    const auto paths = QFileDialog::getOpenFileNames(
        this, "Pick images", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp)");
    if (!paths.isEmpty()) processFiles(paths);
}

void OcrTab::onBulkFolder() {
    const auto folder = QFileDialog::getExistingDirectory(this, "Pick folder of images");
    if (folder.isEmpty()) return;
    processFolder(folder);
}

void OcrTab::processFiles(const QStringList& paths) {
    if (assetMode_) rebuildHeaders(false);
    status_->setText(QString("Processing %1 file(s)…").arg(paths.size()));
    QApplication::processEvents();

    int ok = 0, fail = 0;
    for (const auto& p : paths) {
        try {
            auto result = engine_.recognize(p.toStdString());
            addRow(result);
            repo_.insert({0, result.filename, result.text, result.confidence, nowMs()});
            ++ok;
        } catch (const std::exception& e) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(p));
            table_->setItem(row, 1, new QTableWidgetItem(QString("ERROR: %1").arg(e.what())));
            ++fail;
        }
        QApplication::processEvents();
    }
    status_->setText(QString("Done — %1 ok, %2 failed").arg(ok).arg(fail));
}

void OcrTab::processFolder(const QString& folder) {
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
                    "ให้ชี้ไปที่โฟลเดอร์ scripts ของ AutoPilot")
                .arg(script));
        return;
    }

    rebuildHeaders(true);
    bulkResults_.clear();
    bulkFolder_ = folder;
    sendToReviewBtn_->setEnabled(false);
    bulkBtn_->setEnabled(false);
    browseBtn_->setEnabled(false);
    bulkStdoutBuf_.clear();
    bulkExpected_ = 0;
    bulkProcessed_ = 0;

    // ใช้ temp CSV file — Python ยังต้องเขียน (ไว้สำหรับ debug + audit)
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

    status_->setText("กำลังโหลด PaddleOCR…");
    bulkProcess_->start("python", {script, folder,
                                    QString::fromStdString(tmpCsv),
                                    "--progress-json"});
    if (!bulkProcess_->waitForStarted(5000)) {
        status_->setText("ไม่สามารถ start python ได้ — เช็คว่ามี python ใน PATH");
        bulkBtn_->setEnabled(true);
        browseBtn_->setEnabled(true);
    }
}

void OcrTab::onBulkStdout() {
    if (!bulkProcess_) return;
    bulkStdoutBuf_ += bulkProcess_->readAllStandardOutput();
    // process complete lines (split on '\n')
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
                status_->setText(QString("เจอ %1 ภาพ • กำลังโหลดโมเดล…")
                                     .arg(bulkExpected_));
            } else if (event == "ready") {
                status_->setText(QString("เริ่มประมวลผล %1 ภาพ…").arg(bulkExpected_));
            } else if (event == "row") {
                ocr::AssetInfo info;
                info.filename = j.value("filename", "");
                info.pcNo = j.value("pc_no", "");
                info.serialNo = j.value("serial_no", "");
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
                status_->setText(
                    QString("ประมวลผล %1/%2 • PC#=%3 Serial=%4")
                        .arg(bulkProcessed_).arg(bulkExpected_)
                        .arg(withPc).arg(withSn));
            } else if (event == "done") {
                const int withPc = j.value("with_pc", 0);
                const int withSn = j.value("with_sn", 0);
                const int total = j.value("total", bulkProcessed_);
                const double pcRate = total ? 100.0 * withPc / total : 0.0;
                const double snRate = total ? 100.0 * withSn / total : 0.0;
                status_->setText(
                    QString("เสร็จ %1 ภาพ — PC No. %2% • Serial %3%")
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
    // flush remaining buffer ในกรณีที่ line สุดท้ายไม่มี \n
    if (!bulkStdoutBuf_.isEmpty()) {
        bulkStdoutBuf_ += '\n';
        onBulkStdout();
    }
    bulkBtn_->setEnabled(true);
    browseBtn_->setEnabled(true);
    sendToReviewBtn_->setEnabled(!bulkResults_.empty());
    if (exitCode != 0) {
        const auto err = bulkProcess_ ? QString::fromUtf8(
                                            bulkProcess_->readAllStandardError())
                                       : QString();
        status_->setText(
            QString("Python จบแบบ exit code %1 — %2")
                .arg(exitCode)
                .arg(err.left(200)));
    }
}

void OcrTab::addRow(const ocr::OcrResult& r) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.filename)));
    table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.text)));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.digits)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::number(r.confidence, 'f', 1)));
}

void OcrTab::addAssetRow(const ocr::AssetInfo& info) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(QString::number(info.photoIndex)));
    table_->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(
                    std::filesystem::path(info.filename).filename().string())));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(info.pcNo)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(info.serialNo)));
    table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(info.batchId)));
    table_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(info.photoDate)));
}

void OcrTab::onExport() {
    if (table_->rowCount() == 0) {
        QMessageBox::information(this, "Export", "Nothing to export.");
        return;
    }
    const auto defaultName = assetMode_ ? "asset_extract.json" : "ocr_results.json";
    const auto path = QFileDialog::getSaveFileName(this, "Export", defaultName,
                                                    "JSON (*.json)");
    if (path.isEmpty()) return;

    nlohmann::json arr = nlohmann::json::array();
    const int cols = table_->columnCount();
    for (int r = 0; r < table_->rowCount(); ++r) {
        nlohmann::json row;
        for (int c = 0; c < cols; ++c) {
            const auto header =
                table_->horizontalHeaderItem(c)->text().toStdString();
            row[header] = table_->item(r, c) ? table_->item(r, c)->text().toStdString()
                                              : std::string{};
        }
        arr.push_back(row);
    }
    std::ofstream out(path.toStdString());
    out << arr.dump(2);
    status_->setText(QString("Exported %1 rows → %2").arg(table_->rowCount()).arg(path));
}

void OcrTab::onClear() {
    rebuildHeaders(assetMode_);
    bulkResults_.clear();
    bulkFolder_.clear();
    sendToReviewBtn_->setEnabled(false);
    status_->setText("Cleared");
}

void OcrTab::onSendToReview() {
    if (bulkResults_.empty() || bulkFolder_.isEmpty()) return;
    emit sendToReviewRequested(bulkResults_, bulkFolder_);
}

}  // namespace autopilot::gui
