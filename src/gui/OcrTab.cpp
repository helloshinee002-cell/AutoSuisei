#include "OcrTab.h"

#include <chrono>
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
    : QWidget(parent), repo_(repo), engine_(ocr::OcrOptions{.languages = "eng+tha"}) {
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);

    auto* hint = new QLabel("◉ Drop image files here, or click Browse");
    hint->setStyleSheet("padding: 14px; border: 2px dashed #888;");
    hint->setAlignment(Qt::AlignCenter);
    root->addWidget(hint);

    table_ = new QTableWidget(0, 4);
    table_->setHorizontalHeaderLabels({"File", "Text", "Digits", "Confidence"});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_, 1);

    auto* btnRow = new QHBoxLayout();
    browseBtn_ = new QPushButton("Browse…");
    exportBtn_ = new QPushButton("Export JSON…");
    clearBtn_ = new QPushButton("Clear");
    btnRow->addWidget(browseBtn_);
    btnRow->addWidget(exportBtn_);
    btnRow->addWidget(clearBtn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    status_ = new QLabel("Ready (Tesseract eng+tha)");
    root->addWidget(status_);

    connect(browseBtn_, &QPushButton::clicked, this, &OcrTab::onBrowse);
    connect(exportBtn_, &QPushButton::clicked, this, &OcrTab::onExport);
    connect(clearBtn_, &QPushButton::clicked, this, &OcrTab::onClear);
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

void OcrTab::processFiles(const QStringList& paths) {
    status_->setText(QString("Processing %1 file(s)…").arg(paths.size()));
    QApplication::processEvents();

    int ok = 0;
    int fail = 0;
    for (const auto& p : paths) {
        try {
            auto result = engine_.recognize(p.toStdString());
            addRow(result);
            repo_.insert({0, result.filename, result.text, result.confidence, nowMs()});
            ++ok;
        } catch (const std::exception& e) {
            QTableWidgetItem* err = new QTableWidgetItem(QString("ERROR: %1").arg(e.what()));
            const int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(p));
            table_->setItem(row, 1, err);
            ++fail;
        }
        QApplication::processEvents();
    }
    status_->setText(QString("Done — %1 ok, %2 failed").arg(ok).arg(fail));
}

void OcrTab::addRow(const ocr::OcrResult& r) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.filename)));
    table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.text)));
    table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.digits)));
    table_->setItem(row, 3, new QTableWidgetItem(QString::number(r.confidence, 'f', 1)));
}

void OcrTab::onExport() {
    if (table_->rowCount() == 0) {
        QMessageBox::information(this, "Export", "Nothing to export.");
        return;
    }
    const auto path = QFileDialog::getSaveFileName(
        this, "Export OCR results", "ocr_results.json", "JSON (*.json)");
    if (path.isEmpty()) return;

    nlohmann::json arr = nlohmann::json::array();
    for (int r = 0; r < table_->rowCount(); ++r) {
        arr.push_back({
            {"filename", table_->item(r, 0)->text().toStdString()},
            {"text", table_->item(r, 1) ? table_->item(r, 1)->text().toStdString() : ""},
            {"digits", table_->item(r, 2) ? table_->item(r, 2)->text().toStdString() : ""},
            {"confidence", table_->item(r, 3) ? table_->item(r, 3)->text().toFloat() : 0.0f},
        });
    }
    std::ofstream out(path.toStdString());
    out << arr.dump(2);
    status_->setText(QString("Exported %1 rows → %2").arg(table_->rowCount()).arg(path));
}

void OcrTab::onClear() {
    table_->setRowCount(0);
    status_->setText("Cleared");
}

}  // namespace autopilot::gui
