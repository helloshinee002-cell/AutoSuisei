#include "ReviewTab.h"

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
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace autopilot::gui {

namespace {

constexpr int kCountColumns = 4;

QTableWidgetItem* makeItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

}  // namespace

ReviewTab::ReviewTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    // ---------- top button row ----------
    auto* topRow = new QHBoxLayout();
    loadCsvBtn_ = new QPushButton("Load CSV…");
    loadFolderBtn_ = new QPushButton("Images folder…");
    saveBtn_ = new QPushButton("Save ground truth…");
    saveBtn_->setEnabled(false);
    topRow->addWidget(loadCsvBtn_);
    topRow->addWidget(loadFolderBtn_);
    topRow->addStretch();
    topRow->addWidget(saveBtn_);
    root->addLayout(topRow);

    auto* infoRow = new QHBoxLayout();
    csvLabel_ = new QLabel("CSV: (none)");
    folderLabel_ = new QLabel("Folder: (none)");
    infoRow->addWidget(csvLabel_, 1);
    infoRow->addWidget(folderLabel_, 1);
    root->addLayout(infoRow);

    // ---------- main splitter ----------
    auto* split = new QSplitter(Qt::Horizontal, this);

    // Left: table
    table_ = new QTableWidget(0, kCountColumns);
    table_->setHorizontalHeaderLabels({"filename", "pc_no", "serial_no", "verified"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    split->addWidget(table_);

    // Right: image preview + edit form
    auto* right = new QWidget(this);
    auto* rlayout = new QVBoxLayout(right);

    imageView_ = new QLabel("(no image selected)");
    imageView_->setAlignment(Qt::AlignCenter);
    imageView_->setMinimumSize(400, 300);
    imageView_->setStyleSheet("background: #222; color: #aaa; border: 1px solid #444;");
    rlayout->addWidget(imageView_, 1);

    auto* form = new QFormLayout();
    pcEdit_ = new QLineEdit();
    serialEdit_ = new QLineEdit();
    notesEdit_ = new QLineEdit();
    verifiedCheck_ = new QCheckBox("Verified");
    originalLabel_ = new QLabel("Original: (none)");
    originalLabel_->setStyleSheet("color: #888; font-size: 11px;");
    form->addRow("PC No.:", pcEdit_);
    form->addRow("Serial:", serialEdit_);
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

    status_ = new QLabel("Ready");
    root->addWidget(status_);

    connect(loadCsvBtn_, &QPushButton::clicked, this, &ReviewTab::onLoadCsv);
    connect(loadFolderBtn_, &QPushButton::clicked, this, &ReviewTab::onLoadFolder);
    connect(saveBtn_, &QPushButton::clicked, this, &ReviewTab::onSave);
    connect(applyBtn_, &QPushButton::clicked, this, &ReviewTab::onApplyAndNext);
    connect(table_, &QTableWidget::cellClicked, this, &ReviewTab::onTableRowClicked);
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
    csvPath_ = path;
    csvLabel_->setText("CSV: " + QFileInfo(path).fileName());
    rebuildTable();
    saveBtn_->setEnabled(true);
    applyBtn_->setEnabled(true);
    auto next = model_.nextUnverified();
    selectRow(next ? static_cast<int>(*next) : 0);
}

void ReviewTab::onLoadFolder() {
    const auto path = QFileDialog::getExistingDirectory(
        this, "Choose images folder", QString());
    if (path.isEmpty()) return;
    imagesFolder_ = path;
    folderLabel_->setText("Folder: " + path);
    if (currentRow_ >= 0) loadImageForRow(currentRow_);
}

void ReviewTab::rebuildTable() {
    table_->setRowCount(static_cast<int>(model_.size()));
    for (std::size_t i = 0; i < model_.size(); ++i) {
        const auto row = *model_.at(i);
        const int r = static_cast<int>(i);
        table_->setItem(r, 0, makeItem(QString::fromStdString(row.filename)));
        table_->setItem(r, 1, makeItem(QString::fromStdString(row.pcNo)));
        table_->setItem(r, 2, makeItem(QString::fromStdString(row.serialNo)));
        table_->setItem(r, 3, makeItem(row.verified ? "✓" : ""));
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

    // refresh row in table
    table_->item(currentRow_, 1)->setText(QString::fromStdString(r.pcNo));
    table_->item(currentRow_, 2)->setText(QString::fromStdString(r.serialNo));
    table_->item(currentRow_, 3)->setText(r.verified ? "✓" : "");
    updateStatus();

    // advance to next unverified
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
    status_->setText("Saved → " + path);
}

void ReviewTab::updateStatus() {
    const auto total = model_.size();
    const auto done = model_.verifiedCount();
    status_->setText(QString("%1 / %2 verified").arg(done).arg(total));
}

}  // namespace autopilot::gui
