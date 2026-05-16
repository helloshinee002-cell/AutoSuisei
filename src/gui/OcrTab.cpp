#include "OcrTab.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include "storage/IOcrResultRepository.h"

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
    clearBtn_ = new QPushButton("Clear");
    btnRow->addWidget(browseBtn_);
    btnRow->addWidget(bulkBtn_);
    btnRow->addWidget(exportBtn_);
    btnRow->addWidget(clearBtn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    status_ = new QLabel("Ready (Tesseract eng+tha)");
    root->addWidget(status_);

    connect(browseBtn_, &QPushButton::clicked, this, &OcrTab::onBrowse);
    connect(bulkBtn_, &QPushButton::clicked, this, &OcrTab::onBulkFolder);
    connect(exportBtn_, &QPushButton::clicked, this, &OcrTab::onExport);
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
    namespace fs = std::filesystem;
    std::vector<fs::path> images;
    try {
        for (const auto& entry : fs::directory_iterator(folder.toStdString())) {
            if (entry.is_regular_file() &&
                looksLikeImage(QString::fromStdString(entry.path().string()))) {
                images.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Folder error", e.what());
        return;
    }
    std::sort(images.begin(), images.end());
    if (images.empty()) {
        QMessageBox::information(this, "Bulk extract", "No images in folder.");
        return;
    }

    rebuildHeaders(true);
    QProgressDialog progress(QString("Processing %1 images…").arg(images.size()),
                              "Cancel", 0, static_cast<int>(images.size()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    int withPcNo = 0, withSerial = 0;
    int idx = 0;
    for (const auto& p : images) {
        if (progress.wasCanceled()) break;
        progress.setValue(idx);
        progress.setLabelText(
            QString("%1 / %2 — PC#=%3 Serial=%4")
                .arg(idx).arg(images.size()).arg(withPcNo).arg(withSerial));

        try {
            const auto info = extractor_.extract(p.string());
            addAssetRow(info);
            if (!info.pcNo.empty()) ++withPcNo;
            if (!info.serialNo.empty()) ++withSerial;
        } catch (const std::exception&) {
            // skip
        }
        QApplication::processEvents();
        ++idx;
    }
    progress.setValue(static_cast<int>(images.size()));

    const double pcRate = images.empty() ? 0.0 : 100.0 * withPcNo / images.size();
    const double snRate = images.empty() ? 0.0 : 100.0 * withSerial / images.size();
    status_->setText(QString("Done %1 photos — PC No. hit %2% • Serial hit %3%")
                         .arg(images.size())
                         .arg(pcRate, 0, 'f', 1)
                         .arg(snRate, 0, 'f', 1));
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
    status_->setText("Cleared");
}

}  // namespace autopilot::gui
