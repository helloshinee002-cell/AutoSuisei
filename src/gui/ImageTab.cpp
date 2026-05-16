#include "ImageTab.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "vision/ImageMatcher.h"

namespace autopilot::gui {

ImageTab::ImageTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout();

    haystackPath_ = new QLineEdit();
    auto* hayBtn = new QPushButton("Browse…");
    auto* hayRow = new QHBoxLayout();
    hayRow->addWidget(haystackPath_, 1);
    hayRow->addWidget(hayBtn);
    form->addRow("Haystack (large image):", hayRow);

    needlePath_ = new QLineEdit();
    auto* needleBtn = new QPushButton("Browse…");
    auto* needleRow = new QHBoxLayout();
    needleRow->addWidget(needlePath_, 1);
    needleRow->addWidget(needleBtn);
    form->addRow("Needle (template):", needleRow);

    root->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    matchBtn_ = new QPushButton("Find image");
    copyBtn_ = new QPushButton("Copy click coords");
    copyBtn_->setEnabled(false);
    btnRow->addWidget(matchBtn_);
    btnRow->addWidget(copyBtn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    resultLabel_ = new QLabel("Ready");
    resultLabel_->setStyleSheet("padding: 10px; background: #f0f0f0;");
    resultLabel_->setWordWrap(true);
    root->addWidget(resultLabel_);
    root->addStretch();

    connect(hayBtn, &QPushButton::clicked, this, &ImageTab::onPickHaystack);
    connect(needleBtn, &QPushButton::clicked, this, &ImageTab::onPickNeedle);
    connect(matchBtn_, &QPushButton::clicked, this, &ImageTab::onMatch);
    connect(copyBtn_, &QPushButton::clicked, this, &ImageTab::onCopyCoords);
}

void ImageTab::onPickHaystack() {
    const auto p = QFileDialog::getOpenFileName(
        this, "Pick haystack image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!p.isEmpty()) haystackPath_->setText(p);
}

void ImageTab::onPickNeedle() {
    const auto p = QFileDialog::getOpenFileName(
        this, "Pick needle (template) image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!p.isEmpty()) needlePath_->setText(p);
}

void ImageTab::onMatch() {
    const auto hay = haystackPath_->text();
    const auto needle = needlePath_->text();
    if (hay.isEmpty() || needle.isEmpty()) {
        QMessageBox::information(this, "Match", "Pick both images first.");
        return;
    }
    try {
        const auto result = autopilot::vision::ImageMatcher::matchFiles(
            hay.toStdString(), needle.toStdString());
        if (!result.has_value()) {
            resultLabel_->setText("No match (confidence below threshold)");
            copyBtn_->setEnabled(false);
            return;
        }
        lastCenterX_ = result->centerX();
        lastCenterY_ = result->centerY();
        resultLabel_->setText(
            QString("✓ Match at (%1, %2)  •  size %3×%4  •  confidence %5\n"
                    "Click center: (%6, %7)")
                .arg(result->x).arg(result->y)
                .arg(result->width).arg(result->height)
                .arg(result->confidence, 0, 'f', 3)
                .arg(lastCenterX_).arg(lastCenterY_));
        copyBtn_->setEnabled(true);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Match error", e.what());
    }
}

void ImageTab::onCopyCoords() {
    if (lastCenterX_ < 0) return;
    const auto text = QString("%1,%2").arg(lastCenterX_).arg(lastCenterY_);
    QApplication::clipboard()->setText(text);
    resultLabel_->setText(resultLabel_->text() + QString("\n\nCopied: %1").arg(text));
}

}  // namespace autopilot::gui
