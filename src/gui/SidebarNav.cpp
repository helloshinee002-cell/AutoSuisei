#include "SidebarNav.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace autopilot::gui {

namespace {

/** A single nav item widget — left badge box, label+subtitle stack, right count badge. */
class NavRow : public QWidget {
public:
    NavRow(const QString& code, const QString& label, const QString& subtitle,
           QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(8, 6, 10, 6);
        root->setSpacing(8);

        // Left: code badge (e.g. "OCR")
        auto* code_ = new QLabel(code);
        code_->setObjectName("navCodeBadge");
        code_->setAlignment(Qt::AlignCenter);
        code_->setFixedSize(36, 36);
        code_->setStyleSheet(
            "QLabel#navCodeBadge {"
            "  background-color: #0F1416;"
            "  color: #6B7B78;"
            "  border-radius: 6px;"
            "  font-size: 10px;"
            "  font-weight: 700;"
            "  letter-spacing: 1px;"
            "}");
        root->addWidget(code_);

        // Middle: label + subtitle
        auto* mid = new QVBoxLayout();
        mid->setContentsMargins(0, 0, 0, 0);
        mid->setSpacing(2);
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #E5F4F0; background: transparent;");
        auto* sub = new QLabel(subtitle);
        sub->setStyleSheet("font-size: 11px; color: #6B7B78; background: transparent;");
        mid->addWidget(lbl);
        mid->addWidget(sub);
        root->addLayout(mid, 1);

        // Right: count badge
        badge_ = new QLabel();
        badge_->setObjectName("navCountBadge");
        badge_->setAlignment(Qt::AlignCenter);
        badge_->setMinimumWidth(28);
        badge_->setStyleSheet(
            "QLabel#navCountBadge {"
            "  color: #6B7B78;"
            "  font-size: 12px;"
            "  background: transparent;"
            "}");
        root->addWidget(badge_);
    }

    void setBadge(const QString& text) { badge_->setText(text); }

private:
    QLabel* badge_{};
};

}  // namespace

SidebarNav::SidebarNav(QWidget* parent) : QWidget(parent) {
    setObjectName("sidebar");
    setFixedWidth(240);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* brand = new QLabel("AUTOSUISEI");
    brand->setObjectName("brandTitle");
    auto* tagline = new QLabel("OCR Workspace");
    tagline->setObjectName("brandSubtitle");
    root->addWidget(brand);
    root->addWidget(tagline);

    list_ = new QListWidget();
    list_->setObjectName("sidebarNav");
    list_->setFrameShape(QFrame::NoFrame);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setUniformItemSizes(false);
    list_->setSpacing(0);
    root->addWidget(list_, 1);

    connect(list_, &QListWidget::currentRowChanged,
            this, &SidebarNav::currentChanged);
}

int SidebarNav::addItem(const QString& code, const QString& label,
                         const QString& subtitle) {
    auto* row = new NavRow(code, label, subtitle);
    auto* item = new QListWidgetItem(list_);
    item->setSizeHint(QSize(220, 58));  // explicit width prevents label truncation
    list_->setItemWidget(item, row);
    items_.append(item);
    if (items_.size() == 1) list_->setCurrentRow(0);
    return items_.size() - 1;
}

void SidebarNav::setBadge(int idx, const QString& text) {
    if (idx < 0 || idx >= items_.size()) return;
    // We constructed every itemWidget as NavRow in addItem(), so static_cast
    // is safe. qobject_cast would require Q_OBJECT which NavRow can't have
    // while in an anonymous namespace.
    auto* row = static_cast<NavRow*>(list_->itemWidget(items_[idx]));
    if (row) row->setBadge(text);
}

int SidebarNav::currentIndex() const {
    return list_->currentRow();
}

void SidebarNav::setCurrentIndex(int idx) {
    list_->setCurrentRow(idx);
}

}  // namespace autopilot::gui
