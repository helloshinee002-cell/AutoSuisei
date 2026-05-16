#include "WebTab.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "storage/IMacroRepository.h"
#include "web/CdpClient.h"
#include "web/WebPlayer.h"

namespace autopilot::gui {

WebTab::WebTab(storage::IMacroRepository& repo, QWidget* parent)
    : QWidget(parent),
      repo_(repo),
      client_(std::make_unique<web::CdpClient>()),
      player_(std::make_unique<web::WebPlayer>(*client_)) {

    auto* root = new QVBoxLayout(this);

    auto* connectBox = new QGroupBox("Chrome connection");
    auto* connectLayout = new QFormLayout(connectBox);
    wsUrl_ = new QLineEdit("ws://localhost:9222/devtools/page/<id>");
    connectLayout->addRow("WS endpoint:", wsUrl_);

    auto* connBtnRow = new QHBoxLayout();
    connectBtn_ = new QPushButton("Connect");
    disconnectBtn_ = new QPushButton("Disconnect");
    disconnectBtn_->setEnabled(false);
    connBtnRow->addWidget(connectBtn_);
    connBtnRow->addWidget(disconnectBtn_);
    connBtnRow->addStretch();
    connectLayout->addRow(connBtnRow);

    auto* hint = new QLabel(
        "Launch Chrome with:\n"
        "  chrome.exe --remote-debugging-port=9222 --user-data-dir=C:\\tmp\\chrome-profile\n"
        "Then visit http://localhost:9222/json/list to get the page's webSocketDebuggerUrl.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #888; font-size: 11px;");
    connectLayout->addRow(hint);
    root->addWidget(connectBox);

    auto* playBox = new QGroupBox("Replay desktop macro on browser");
    auto* playLayout = new QVBoxLayout(playBox);
    auto* comboRow = new QHBoxLayout();
    macroCombo_ = new QComboBox();
    refreshBtn_ = new QPushButton("⟳");
    refreshBtn_->setMaximumWidth(40);
    comboRow->addWidget(macroCombo_, 1);
    comboRow->addWidget(refreshBtn_);
    playLayout->addLayout(comboRow);
    playBtn_ = new QPushButton("▶ Play on browser");
    playBtn_->setEnabled(false);
    playLayout->addWidget(playBtn_);
    root->addWidget(playBox);

    status_ = new QLabel("Disconnected");
    root->addWidget(status_);
    root->addStretch();

    connect(connectBtn_, &QPushButton::clicked, this, &WebTab::onConnect);
    connect(disconnectBtn_, &QPushButton::clicked, this, &WebTab::onDisconnect);
    connect(playBtn_, &QPushButton::clicked, this, &WebTab::onPlay);
    connect(refreshBtn_, &QPushButton::clicked, this, &WebTab::onRefreshMacros);

    onRefreshMacros();
}

WebTab::~WebTab() {
    if (client_ && client_->isConnected()) client_->disconnect();
}

void WebTab::onConnect() {
    try {
        client_->connect(wsUrl_->text().toStdString());
        connectBtn_->setEnabled(false);
        disconnectBtn_->setEnabled(true);
        playBtn_->setEnabled(true);
        setStatus("Connected (waiting for handshake)");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Connect failed", e.what());
    }
}

void WebTab::onDisconnect() {
    client_->disconnect();
    connectBtn_->setEnabled(true);
    disconnectBtn_->setEnabled(false);
    playBtn_->setEnabled(false);
    setStatus("Disconnected");
}

void WebTab::onPlay() {
    const int idx = macroCombo_->currentIndex();
    if (idx < 0) {
        QMessageBox::information(this, "Play", "Pick a macro first.");
        return;
    }
    const auto id = macroCombo_->currentData().toLongLong();
    const auto macro = repo_.findById(id);
    if (!macro.has_value()) {
        QMessageBox::critical(this, "Play", "Macro not found.");
        return;
    }
    setStatus(QString("Playing '%1' on browser…").arg(QString::fromStdString(macro->name)));
    try {
        player_->play(*macro);
        setStatus("Playback complete");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Playback error", e.what());
        setStatus("Playback error");
    }
}

void WebTab::onRefreshMacros() {
    macroCombo_->clear();
    for (const auto& m : repo_.findAll()) {
        macroCombo_->addItem(
            QString("%1  (%2 actions)")
                .arg(QString::fromStdString(m.name))
                .arg(m.actions.size()),
            QVariant::fromValue(static_cast<qint64>(m.id)));
    }
}

void WebTab::setStatus(const QString& s) { status_->setText(s); }

}  // namespace autopilot::gui
