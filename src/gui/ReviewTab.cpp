#include "ReviewTab.h"

#include <filesystem>
#include <fstream>

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace autopilot::gui {

namespace {

constexpr int kCountColumns = 6;  // #, File, No., Serial, Date, OK

QTableWidgetItem* makeItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString sanitizeFilenameComponent(const QString& s) {
    QString out = s.trimmed();
    out.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    return out;
}

}  // namespace

ReviewTab::ReviewTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(12);

    // ----- Header -----
    auto* title = new QLabel("Review and Rename");
    title->setObjectName("tabTitle");
    auto* subtitle = new QLabel("Verify, edit, and rename extracted records");
    subtitle->setObjectName("tabSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    // ----- Top button row -----
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);
    loadCsvBtn_ = new QPushButton("Load CSV…");
    loadFolderBtn_ = new QPushButton("Images folder…");
    saveBtn_ = new QPushButton("Save ground truth");
    saveBtn_->setObjectName("primaryButton");
    saveBtn_->setEnabled(false);
    clearBtn_ = new QPushButton("Clear");
    clearBtn_->setToolTip("รีเซ็ตข้อมูลทั้งหมด — CSV, ภาพ, ฟอร์ม");
    topRow->addWidget(loadCsvBtn_);
    topRow->addWidget(loadFolderBtn_);
    topRow->addStretch();
    topRow->addWidget(saveBtn_);
    topRow->addWidget(clearBtn_);
    root->addLayout(topRow);

    // ----- CSV / Folder info -----
    auto* infoRow = new QHBoxLayout();
    auto* csvLbl = new QLabel("CSV:");
    csvLbl->setObjectName("dimLabel");
    csvLabel_ = new QLabel("(none)");
    auto* folderLbl = new QLabel("Folder:");
    folderLbl->setObjectName("dimLabel");
    folderLabel_ = new QLabel("(none)");
    infoRow->addWidget(csvLbl);
    infoRow->addWidget(csvLabel_, 1);
    infoRow->addSpacing(20);
    infoRow->addWidget(folderLbl);
    infoRow->addWidget(folderLabel_, 1);
    root->addLayout(infoRow);

    // ----- Progress bar -----
    progressLabel_ = new QLabel("Verified 0 / 0");
    progressLabel_->setObjectName("dimLabel");
    progressBar_ = new QProgressBar();
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(8);
    root->addWidget(progressLabel_);
    root->addWidget(progressBar_);

    // ----- Splitter: table left, preview+form right -----
    auto* split = new QSplitter(Qt::Horizontal, this);

    table_ = new QTableWidget(0, kCountColumns);
    table_->setHorizontalHeaderLabels(
        {"#", "File", "No.", "Serial", "Date", "OK"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setColumnWidth(0, 50);
    table_->setColumnWidth(2, 80);
    table_->setColumnWidth(3, 140);
    table_->setColumnWidth(4, 100);
    table_->setColumnWidth(5, 44);
    split->addWidget(table_);

    auto* right = new QWidget(this);
    auto* rlayout = new QVBoxLayout(right);
    rlayout->setContentsMargins(0, 0, 0, 0);
    rlayout->setSpacing(10);

    imageView_ = new QLabel("(no image selected)");
    imageView_->setObjectName("imageView");
    imageView_->setAlignment(Qt::AlignCenter);
    imageView_->setMinimumSize(400, 260);
    rlayout->addWidget(imageView_, 1);

    auto* form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pcEdit_ = new QLineEdit();
    serialEdit_ = new QLineEdit();
    batchEdit_ = new QLineEdit();
    batchEdit_->setReadOnly(true);
    batchEdit_->setToolTip("Batch ID (จาก OCR) — read only");
    dateEdit_ = new QLineEdit();
    dateEdit_->setReadOnly(true);
    dateEdit_->setToolTip("Photo date (จาก OCR) — read only");
    notesEdit_ = new QLineEdit();
    verifiedCheck_ = new QCheckBox("Verified");
    originalLabel_ = new QLabel("Original: (none)");
    originalLabel_->setObjectName("originalLabel");
    form->addRow("No.:", pcEdit_);
    form->addRow("Serial:", serialEdit_);
    form->addRow("Batch:", batchEdit_);
    form->addRow("Date:", dateEdit_);
    form->addRow("Notes:", notesEdit_);
    form->addRow("", verifiedCheck_);
    form->addRow("", originalLabel_);
    rlayout->addLayout(form);

    applyBtn_ = new QPushButton("Apply + Next unverified");
    applyBtn_->setEnabled(false);
    rlayout->addWidget(applyBtn_);

    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    // ----- Rename bar -----
    auto* renameRow = new QHBoxLayout();
    renameRow->setSpacing(10);
    auto* renameLabel = new QLabel("Rename by:");
    renameLabel->setObjectName("dimLabel");
    renamePcCheck_ = new QCheckBox("No.");
    renamePcCheck_->setChecked(true);
    renameSerialCheck_ = new QCheckBox("Serial");
    renameNotesCheck_ = new QCheckBox("Notes");
    renameBtn_ = new QPushButton("Rename images…");
    renameBtn_->setEnabled(false);
    renameBtn_->setToolTip(
        "เปลี่ยนชื่อไฟล์ภาพในโฟลเดอร์ ตามค่าที่ติ๊กไว้ (เชื่อมด้วย _)");
    renameRow->addWidget(renameLabel);
    renameRow->addWidget(renamePcCheck_);
    renameRow->addWidget(renameSerialCheck_);
    renameRow->addWidget(renameNotesCheck_);
    renameRow->addStretch();
    renameRow->addWidget(renameBtn_);
    root->addLayout(renameRow);

    setStatus("Ready — load CSV or send from OCR / Watch tab");

    connect(loadCsvBtn_, &QPushButton::clicked, this, &ReviewTab::onLoadCsv);
    connect(loadFolderBtn_, &QPushButton::clicked, this, &ReviewTab::onLoadFolder);
    connect(saveBtn_, &QPushButton::clicked, this, &ReviewTab::onSave);
    connect(clearBtn_, &QPushButton::clicked, this, &ReviewTab::onClear);
    connect(applyBtn_, &QPushButton::clicked, this, &ReviewTab::onApplyAndNext);
    connect(renameBtn_, &QPushButton::clicked, this, &ReviewTab::onRename);
    connect(table_, &QTableWidget::cellClicked, this, &ReviewTab::onTableRowClicked);
}

void ReviewTab::setStatus(const QString& text) {
    statusText_ = text;
    emit statusChanged(text);
}

void ReviewTab::loadFromExtraction(const std::vector<ocr::AssetInfo>& infos,
                                   const QString& folder) {
    model_.clear();
    sourceInfos_ = infos;  // keep for batch/date display
    namespace fs = std::filesystem;
    auto tempCsv = fs::temp_directory_path() / "autopilot_ocr_to_review.csv";
    {
        std::ofstream out(tempCsv, std::ios::trunc);
        out << "filename,pc_no,serial_no\n";
        for (const auto& info : infos) {
            // info.filename เป็น basename จาก Python อยู่แล้ว — เลี่ยง fs::path(narrow) round-trip ที่ทำ
            // ANSI corruption บนชื่อไทย
            const std::string& base = info.filename;
            out << ocr::ReviewModel::escapeCsv(base) << ','
                << ocr::ReviewModel::escapeCsv(info.pcNo) << ','
                << ocr::ReviewModel::escapeCsv(info.serialNo) << '\n';
        }
    }
    if (!model_.loadCsv(tempCsv.string())) {
        QMessageBox::warning(this, "Load failed",
                             "ไม่สามารถสร้าง review data จาก OCR results ได้");
        return;
    }
    csvPath_ = QString::fromStdString(tempCsv.string());
    csvLabel_->setText(QString("(from OCR bulk extract — %1 rows)").arg(infos.size()));
    imagesFolder_ = folder;
    folderLabel_->setText(folder);
    rebuildTable();
    saveBtn_->setEnabled(true);
    applyBtn_->setEnabled(true);
    renameBtn_->setEnabled(model_.size() > 0);
    auto next = model_.nextUnverified();
    selectRow(next ? static_cast<int>(*next) : 0);
}

void ReviewTab::onLoadCsv() {
    const auto path = QFileDialog::getOpenFileName(
        this, "Load review CSV", QString(), "CSV files (*.csv);;All files (*)");
    if (path.isEmpty()) return;

    if (!model_.loadCsv(path.toStdString())) {
        QMessageBox::warning(this, "Load failed",
                             "ไม่สามารถโหลด CSV ได้ — เช็คว่ามี header filename column");
        return;
    }
    sourceInfos_.clear();  // CSV path doesn't carry batch/date
    csvPath_ = path;
    csvLabel_->setText(QFileInfo(path).fileName());
    rebuildTable();
    saveBtn_->setEnabled(true);
    applyBtn_->setEnabled(true);
    renameBtn_->setEnabled(!imagesFolder_.isEmpty());
    auto next = model_.nextUnverified();
    selectRow(next ? static_cast<int>(*next) : 0);
}

void ReviewTab::onLoadFolder() {
    const auto path = QFileDialog::getExistingDirectory(
        this, "Choose images folder", QString());
    if (path.isEmpty()) return;
    imagesFolder_ = path;
    folderLabel_->setText(path);
    renameBtn_->setEnabled(model_.size() > 0);
    if (currentRow_ >= 0) loadImageForRow(currentRow_);
}

void ReviewTab::rebuildTable() {
    table_->setRowCount(static_cast<int>(model_.size()));
    for (std::size_t i = 0; i < model_.size(); ++i) {
        const auto row = *model_.at(i);
        const int r = static_cast<int>(i);
        const QString idxStr = QString("%1").arg(r + 1, 2, 10, QChar('0'));
        const QString dateStr = (i < sourceInfos_.size())
                                    ? QString::fromStdString(sourceInfos_[i].photoDate)
                                    : QString();
        table_->setItem(r, 0, makeItem(idxStr));
        table_->setItem(r, 1, makeItem(QString::fromStdString(row.filename)));
        table_->setItem(r, 2, makeItem(QString::fromStdString(row.pcNo)));
        table_->setItem(r, 3, makeItem(QString::fromStdString(row.serialNo)));
        table_->setItem(r, 4, makeItem(dateStr));
        table_->setItem(r, 5, makeItem(row.verified ? "OK" : ""));
    }
    updateStatus();
}

void ReviewTab::onTableRowClicked(int row, int /*column*/) { selectRow(row); }

void ReviewTab::selectRow(int row) {
    if (row < 0 || row >= static_cast<int>(model_.size())) {
        currentRow_ = -1;
        return;
    }
    currentRow_ = row;
    table_->selectRow(row);
    const auto r = *model_.at(static_cast<std::size_t>(row));
    pcEdit_->setText(QString::fromStdString(r.pcNo));
    serialEdit_->setText(QString::fromStdString(r.serialNo));
    if (static_cast<std::size_t>(row) < sourceInfos_.size()) {
        batchEdit_->setText(QString::fromStdString(sourceInfos_[row].batchId));
        dateEdit_->setText(QString::fromStdString(sourceInfos_[row].photoDate));
    } else {
        batchEdit_->clear();
        dateEdit_->clear();
    }
    notesEdit_->setText(QString::fromStdString(r.notes));
    verifiedCheck_->setChecked(r.verified);
    originalLabel_->setText(
        QString("Original: pc=%1  serial=%2")
            .arg(QString::fromStdString(r.originalPcNo),
                 QString::fromStdString(r.originalSerialNo)));
    loadImageForRow(row);
}

void ReviewTab::loadImageForRow(int row) {
    if (imagesFolder_.isEmpty()) {
        imageView_->setText("(set images folder first)");
        imageView_->setPixmap({});
        return;
    }
    const auto r = *model_.at(static_cast<std::size_t>(row));
    const QString full = QDir(imagesFolder_).filePath(
        QString::fromStdString(r.filename));
    QPixmap pix(full);
    if (pix.isNull()) {
        const QString name = r.filename.empty()
                                 ? QStringLiteral("<empty filename>")
                                 : QString::fromStdString(r.filename);
        imageView_->setText("(image not found:\n" + name + ")");
        imageView_->setPixmap({});
        return;
    }
    imageView_->setPixmap(pix.scaled(imageView_->size(), Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
}

void ReviewTab::onApplyAndNext() {
    if (currentRow_ < 0 || currentRow_ >= static_cast<int>(model_.size())) return;
    auto r = *model_.at(static_cast<std::size_t>(currentRow_));
    r.pcNo = pcEdit_->text().toStdString();
    r.serialNo = serialEdit_->text().toStdString();
    r.notes = notesEdit_->text().toStdString();
    r.verified = verifiedCheck_->isChecked();
    model_.setRow(static_cast<std::size_t>(currentRow_), r);

    table_->item(currentRow_, 2)->setText(QString::fromStdString(r.pcNo));
    table_->item(currentRow_, 3)->setText(QString::fromStdString(r.serialNo));
    table_->item(currentRow_, 5)->setText(r.verified ? "OK" : "");
    updateStatus();

    auto next = model_.nextUnverified(static_cast<std::size_t>(currentRow_ + 1));
    if (!next) next = model_.nextUnverified(0);
    if (next) selectRow(static_cast<int>(*next));
}

void ReviewTab::onSave() {
    if (model_.size() == 0) return;
    auto suggest = csvPath_.isEmpty()
                       ? QString("ground_truth.csv")
                       : QFileInfo(csvPath_).absoluteDir().filePath("ground_truth.csv");
    const auto path = QFileDialog::getSaveFileName(
        this, "Save ground truth CSV", suggest, "CSV files (*.csv)");
    if (path.isEmpty()) return;
    if (!model_.saveCsv(path.toStdString())) {
        QMessageBox::warning(this, "Save failed", "ไม่สามารถเขียน CSV ได้");
        return;
    }
    setStatus("Saved → " + path);
}

void ReviewTab::onClear() {
    model_.clear();
    sourceInfos_.clear();
    table_->setRowCount(0);
    csvPath_.clear();
    csvLabel_->setText("(none)");
    // imagesFolder_ ไม่ล้าง — ถือว่า user อาจจะ load CSV ใหม่ที่ folder เดิม
    pcEdit_->clear();
    serialEdit_->clear();
    batchEdit_->clear();
    dateEdit_->clear();
    notesEdit_->clear();
    verifiedCheck_->setChecked(false);
    originalLabel_->setText("Original: (none)");
    imageView_->setText("(no image selected)");
    imageView_->setPixmap({});
    currentRow_ = -1;
    saveBtn_->setEnabled(false);
    applyBtn_->setEnabled(false);
    renameBtn_->setEnabled(false);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    progressLabel_->setText("Verified 0 / 0");
    setStatus("Cleared");
}

void ReviewTab::updateStatus() {
    const auto total = model_.size();
    const auto done = model_.verifiedCount();
    progressBar_->setRange(0, static_cast<int>(total ? total : 1));
    progressBar_->setValue(static_cast<int>(done));
    progressLabel_->setText(QString("Verified %1 / %2").arg(done).arg(total));
    setStatus(QString("Review mode • %1/%2 verified • %3 pending")
                  .arg(done).arg(total).arg(total - done));
}

void ReviewTab::onRename() {
    if (imagesFolder_.isEmpty() || model_.size() == 0) return;

    const bool usePc = renamePcCheck_->isChecked();
    const bool useSerial = renameSerialCheck_->isChecked();
    const bool useNotes = renameNotesCheck_->isChecked();
    if (!usePc && !useSerial && !useNotes) {
        QMessageBox::information(this, "Rename",
                                 "ติ๊กอย่างน้อย 1 ฟิลด์ (No./Serial/Notes)");
        return;
    }

    int eligible = 0;
    for (std::size_t i = 0; i < model_.size(); ++i) {
        const auto r = *model_.at(i);
        if ((usePc && !r.pcNo.empty()) ||
            (useSerial && !r.serialNo.empty()) ||
            (useNotes && !r.notes.empty())) {
            ++eligible;
        }
    }
    if (eligible == 0) {
        QMessageBox::information(this, "Rename",
                                 "ไม่มี row ไหนมีค่าในฟิลด์ที่ติ๊กไว้");
        return;
    }

    namespace fs = std::filesystem;
    int renamed = 0, skipped = 0, failed = 0;
    QStringList failReasons;
    QDir dir(imagesFolder_);

    for (std::size_t i = 0; i < model_.size(); ++i) {
        auto r = *model_.at(i);
        QStringList pieces;
        if (usePc && !r.pcNo.empty())
            pieces << sanitizeFilenameComponent(QString::fromStdString(r.pcNo));
        if (useSerial && !r.serialNo.empty())
            pieces << sanitizeFilenameComponent(QString::fromStdString(r.serialNo));
        if (useNotes && !r.notes.empty())
            pieces << sanitizeFilenameComponent(QString::fromStdString(r.notes));
        if (pieces.isEmpty()) { ++skipped; continue; }
        const QString base = pieces.join("_");
        const QString oldName = QString::fromStdString(r.filename);
        const QString oldPath = dir.filePath(oldName);
        if (!QFileInfo::exists(oldPath)) {
            ++failed;
            if (failReasons.size() < 5)
                failReasons << QString("ไม่พบไฟล์: %1").arg(oldName);
            continue;
        }
        const QString ext = QFileInfo(oldName).suffix();
        QString newName = base + (ext.isEmpty() ? "" : "." + ext);
        QString newPath = dir.filePath(newName);
        if (newPath == oldPath) { ++skipped; continue; }
        int suffix = 2;
        while (QFileInfo::exists(newPath)) {
            newName = QString("%1-%2%3").arg(base).arg(suffix)
                          .arg(ext.isEmpty() ? "" : "." + ext);
            newPath = dir.filePath(newName);
            ++suffix;
            if (suffix > 999) break;
        }
        try {
            // u8path: toStdString() เป็น UTF-8 — fs::rename(narrow) ตีเป็น ANSI → path ไทยหาไม่เจอ
            fs::rename(fs::u8path(oldPath.toStdString()),
                       fs::u8path(newPath.toStdString()));
            r.filename = newName.toStdString();
            model_.setRow(i, r);
            if (auto* item = table_->item(static_cast<int>(i), 1)) {
                item->setText(newName);
            }
            ++renamed;
        } catch (const std::exception& e) {
            ++failed;
            if (failReasons.size() < 5)
                failReasons << QString("%1: %2").arg(oldName).arg(e.what());
        }
    }

    if (currentRow_ >= 0) loadImageForRow(currentRow_);

    QString msg = QString("เสร็จ — เปลี่ยนชื่อ %1 ภาพ, skip %2, fail %3")
                      .arg(renamed).arg(skipped).arg(failed);
    if (!failReasons.isEmpty()) msg += "\n\n" + failReasons.join("\n");
    setStatus(QString("Rename: %1 renamed, %2 skipped, %3 failed")
                  .arg(renamed).arg(skipped).arg(failed));
    QMessageBox::information(this, "Rename done", msg);
}

}  // namespace autopilot::gui
