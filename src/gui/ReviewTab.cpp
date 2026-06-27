#include "ReviewTab.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
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
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QWheelEvent>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace autopilot::gui {

namespace {

constexpr int kCountColumns = 7;  // #, File, No., Serial, Src, Date, OK

QTableWidgetItem* makeItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

// remark ที่มาของ serial: barcode/ocr → "Barcode"/"OCR" (ว่างถ้าไม่มี serial)
QString srcLabel(const ocr::ReviewRow& r) {
    if (r.serialNo.empty()) return "";
    if (r.serialSource == "barcode") return "Barcode";
    if (r.serialSource == "ocr") return "OCR";
    return QString::fromStdString(r.serialSource);
}

QString sanitizeFilenameComponent(const QString& s) {
    QString out = s.trimmed();
    out.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    return out;
}

}  // namespace

ReviewTab::ReviewTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 12);
    root->setSpacing(8);

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
    saveBtn_ = new QPushButton("Save ground truth");
    saveBtn_->setObjectName("primaryButton");
    saveBtn_->setEnabled(false);
    clearBtn_ = new QPushButton("Clear");
    clearBtn_->setToolTip("รีเซ็ตข้อมูลทั้งหมด — ภาพ, ฟอร์ม");
    topRow->addStretch();
    topRow->addWidget(saveBtn_);
    topRow->addWidget(clearBtn_);
    root->addLayout(topRow);

    // ----- Folder info -----
    auto* infoRow = new QHBoxLayout();
    auto* folderLbl = new QLabel("Folder:");
    folderLbl->setObjectName("dimLabel");
    folderLabel_ = new QLabel("(none)");
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
        {"#", "File", "No.", "Serial", "Src", "Date", "OK"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setColumnWidth(0, 50);
    table_->setColumnWidth(2, 80);
    table_->setColumnWidth(3, 140);
    table_->setColumnWidth(4, 70);   // Src (Barcode/OCR)
    table_->setColumnWidth(5, 100);  // Date
    table_->setColumnWidth(6, 44);   // OK
    split->addWidget(table_);

    auto* right = new QWidget(this);
    auto* rlayout = new QVBoxLayout(right);
    rlayout->setContentsMargins(0, 0, 0, 0);
    rlayout->setSpacing(10);

    imageView_ = new QLabel("(no image selected)");
    imageView_->setObjectName("imageView");
    imageView_->setAlignment(Qt::AlignCenter);
    imageView_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageScroll_ = new QScrollArea();
    imageScroll_->setObjectName("imageScroll");
    imageScroll_->setWidget(imageView_);
    imageScroll_->setWidgetResizable(false);
    imageScroll_->setAlignment(Qt::AlignCenter);
    imageScroll_->setMinimumSize(220, 160);  // เล็กพอให้ splitter ยุบ image pane บนจอแคบ
    // Ctrl+wheel zoom / dbl-click reset-fit / re-fit on viewport resize
    imageScroll_->viewport()->installEventFilter(this);
    rlayout->addWidget(imageScroll_, 1);

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
    // เอนน้ำหนักไปฝั่งรูป (table:right ≈ 2:3) → image pane กว้างขึ้น ~60% ทุก resolution
    // โดย imageScroll_ min ยังต่ำ (220×160) กัน low-res ซ้อนทับ
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setSizes({320, 760});
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

    setStatus("Ready — send from OCR / Watch tab");

    connect(saveBtn_, &QPushButton::clicked, this, &ReviewTab::onSave);
    connect(clearBtn_, &QPushButton::clicked, this, &ReviewTab::onClear);
    connect(applyBtn_, &QPushButton::clicked, this, &ReviewTab::onApplyAndNext);
    connect(renameBtn_, &QPushButton::clicked, this, &ReviewTab::onRename);
    // currentCellChanged ครอบทั้งคลิกเมาส์ + ลูกศรในตาราง (cellClicked เดิมจับแค่คลิก)
    connect(table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
                if (row >= 0 && row != currentRow_) selectRow(row);
            });

    // ----- keyboard-driven verify loop -----
    // Enter ในช่องใดก็ได้ = Apply + Next; Tab ข้าม read-only Batch/Date
    batchEdit_->setFocusPolicy(Qt::NoFocus);
    dateEdit_->setFocusPolicy(Qt::NoFocus);
    setTabOrder(pcEdit_, serialEdit_);
    setTabOrder(serialEdit_, notesEdit_);
    setTabOrder(notesEdit_, verifiedCheck_);
    setTabOrder(verifiedCheck_, applyBtn_);
    connect(pcEdit_, &QLineEdit::returnPressed, this, &ReviewTab::onApplyAndNext);
    connect(serialEdit_, &QLineEdit::returnPressed, this, &ReviewTab::onApplyAndNext);
    connect(notesEdit_, &QLineEdit::returnPressed, this, &ReviewTab::onApplyAndNext);
    // ติ๊ก/untick เอง → อัปเดต model + OK column + progress ทันที (Verified สอดคล้องสองทาง)
    connect(verifiedCheck_, &QCheckBox::toggled, this, [this](bool on) {
        if (currentRow_ < 0 || currentRow_ >= static_cast<int>(model_.size())) return;
        auto r = *model_.at(static_cast<std::size_t>(currentRow_));
        if (r.verified == on) return;
        r.verified = on;
        model_.setRow(static_cast<std::size_t>(currentRow_), r);
        if (auto* it = table_->item(currentRow_, 6)) it->setText(on ? "OK" : "");
        updateStatus();
    });
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
        out << "filename,pc_no,serial_no,serial_source\n";
        for (const auto& info : infos) {
            // info.filename เป็น basename จาก Python อยู่แล้ว — เลี่ยง fs::path(narrow) round-trip ที่ทำ
            // ANSI corruption บนชื่อไทย
            const std::string& base = info.filename;
            out << ocr::ReviewModel::escapeCsv(base) << ','
                << ocr::ReviewModel::escapeCsv(info.pcNo) << ','
                << ocr::ReviewModel::escapeCsv(info.serialNo) << ','
                << ocr::ReviewModel::escapeCsv(info.serialSource) << '\n';
        }
    }
    if (!model_.loadCsv(tempCsv.string())) {
        QMessageBox::warning(this, "Load failed",
                             "ไม่สามารถสร้าง review data จาก OCR results ได้");
        return;
    }
    csvPath_ = QString::fromStdString(tempCsv.string());
    imagesFolder_ = folder;
    folderLabel_->setText(folder);
    rebuildTable();
    saveBtn_->setEnabled(true);
    applyBtn_->setEnabled(true);
    renameBtn_->setEnabled(model_.size() > 0);
    auto next = model_.nextUnverified();
    selectRow(next ? static_cast<int>(*next) : 0);
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
        table_->setItem(r, 4, makeItem(srcLabel(row)));
        table_->setItem(r, 5, makeItem(dateStr));
        table_->setItem(r, 6, makeItem(row.verified ? "OK" : ""));
    }
    updateStatus();
}

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
    verifiedCheck_->blockSignals(true);  // กัน toggled handler เขียนกลับตอนโหลดแถว
    verifiedCheck_->setChecked(r.verified);
    verifiedCheck_->blockSignals(false);
    originalLabel_->setText(
        QString("Original: pc=%1  serial=%2")
            .arg(QString::fromStdString(r.originalPcNo),
                 QString::fromStdString(r.originalSerialNo)));
    loadImageForRow(row);
}

void ReviewTab::loadImageForRow(int row) {
    imageZoom_ = 0.0;  // reset เป็น fit ทุกครั้งที่เปลี่ยนรูป
    auto showText = [this](const QString& t) {
        fullPixmap_ = {};
        imageView_->setPixmap({});
        imageView_->setText(t);
        imageView_->resize(imageScroll_->viewport()->size());
    };
    if (imagesFolder_.isEmpty()) {
        showText("(set images folder first)");
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
        showText("(image not found:\n" + name + ")");
        return;
    }
    fullPixmap_ = pix;
    renderImage();
}

// fit-to-pane (imageZoom_<=0) หรือ scale ตาม imageZoom_ แล้ว resize label
// ให้เท่าภาพ — ใหญ่กว่า viewport เมื่อไหร่ QScrollArea โชว์ scrollbar เอง
void ReviewTab::renderImage() {
    if (fullPixmap_.isNull()) return;
    const QSize vp = imageScroll_->viewport()->size();
    QSize target = (imageZoom_ <= 0.0)
                       ? fullPixmap_.size().scaled(vp, Qt::KeepAspectRatio)
                       : QSize(static_cast<int>(fullPixmap_.width() * imageZoom_),
                               static_cast<int>(fullPixmap_.height() * imageZoom_));
    if (target.isEmpty()) target = fullPixmap_.size();
    const QPixmap scaled =
        fullPixmap_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    imageView_->setPixmap(scaled);
    imageView_->resize(scaled.size());
}

void ReviewTab::onApplyAndNext() {
    if (currentRow_ < 0 || currentRow_ >= static_cast<int>(model_.size())) return;
    auto r = *model_.at(static_cast<std::size_t>(currentRow_));
    r.pcNo = pcEdit_->text().toStdString();
    r.serialNo = serialEdit_->text().toStdString();
    r.notes = notesEdit_->text().toStdString();
    r.verified = true;  // Apply = verify เสมอ — loop "Next unverified" จึงเดินหน้า ไม่วนแถวเดิม
    model_.setRow(static_cast<std::size_t>(currentRow_), r);

    table_->item(currentRow_, 2)->setText(QString::fromStdString(r.pcNo));
    table_->item(currentRow_, 3)->setText(QString::fromStdString(r.serialNo));
    table_->item(currentRow_, 6)->setText(r.verified ? "OK" : "");
    updateStatus();

    auto next = model_.nextUnverified(static_cast<std::size_t>(currentRow_ + 1));
    if (!next) next = model_.nextUnverified(0);
    if (next) selectRow(static_cast<int>(*next));
    // cursor กลับช่อง No. + select-all → พิมพ์ตัวถัดไปต่อได้เลย ไม่ต้องจับเมาส์
    pcEdit_->setFocus();
    pcEdit_->selectAll();
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
    currentRow_ = -1;  // ตั้งก่อนเคลียร์ฟอร์ม — กัน verifiedCheck toggled handler แตะ model ที่เพิ่ง clear
    csvPath_.clear();
    // imagesFolder_ ไม่ล้าง — ถือว่า user อาจ send ชุดใหม่ที่ folder เดิม
    pcEdit_->clear();
    serialEdit_->clear();
    batchEdit_->clear();
    dateEdit_->clear();
    notesEdit_->clear();
    verifiedCheck_->setChecked(false);
    originalLabel_->setText("Original: (none)");
    fullPixmap_ = {};
    imageZoom_ = 0.0;
    imageView_->setPixmap({});
    imageView_->setText("(no image selected)");
    imageView_->resize(imageScroll_->viewport()->size());
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

void ReviewTab::keyPressEvent(QKeyEvent* event) {
    // ↑/↓ เลื่อนแถวเมื่อ focus อยู่ในฟอร์ม (ตอน focus อยู่ในตาราง ตารางกิน arrow เอง
    // แล้ว currentCellChanged จัดการ — จึงไม่มา reach ที่นี่ → ไม่เลื่อนซ้อน)
    // ponytail: พึ่ง QLineEdit บรรทัดเดียวไม่ consume Up/Down → event bubble ขึ้นถึง parent นี้
    if (model_.size() > 0 &&
        (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        const int next = currentRow_ + (event->key() == Qt::Key_Down ? 1 : -1);
        if (next >= 0 && next < static_cast<int>(model_.size())) {
            selectRow(next);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

bool ReviewTab::eventFilter(QObject* obj, QEvent* event) {
    if (imageScroll_ && obj == imageScroll_->viewport()) {
        switch (event->type()) {
            case QEvent::Wheel: {
                auto* we = static_cast<QWheelEvent*>(event);
                if (!(we->modifiers() & Qt::ControlModifier)) return false;  // wheel เปล่า = scroll
                if (fullPixmap_.isNull()) return true;
                if (imageZoom_ <= 0.0) {
                    // ออกจาก fit → เริ่มที่ ratio ปัจจุบัน เพื่อ zoom ต่อเนื่องไม่กระโดด
                    const QSize vp = imageScroll_->viewport()->size();
                    imageZoom_ = std::min(
                        static_cast<double>(vp.width()) / fullPixmap_.width(),
                        static_cast<double>(vp.height()) / fullPixmap_.height());
                }
                const double step = (we->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
                imageZoom_ = std::clamp(imageZoom_ * step, 0.1, 8.0);
                renderImage();
                return true;
            }
            case QEvent::Resize:
                if (imageZoom_ <= 0.0) renderImage();  // re-fit เมื่อ splitter/หน้าต่างเปลี่ยน
                return false;
            case QEvent::MouseButtonDblClick:
                imageZoom_ = 0.0;  // reset เป็น fit
                renderImage();
                return true;
            default:
                break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

}  // namespace autopilot::gui
