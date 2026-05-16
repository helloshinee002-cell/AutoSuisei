#pragma once

#include <memory>

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace autopilot::web {
class CdpClient;
class WebPlayer;
}
namespace autopilot::storage { class IMacroRepository; }

namespace autopilot::gui {

/**
 * Web tab: เชื่อมกับ Chrome ที่ launch ด้วย --remote-debugging-port=9222 → load macro → play
 */
class WebTab : public QWidget {
    Q_OBJECT

public:
    explicit WebTab(storage::IMacroRepository& repo, QWidget* parent = nullptr);
    ~WebTab() override;

private slots:
    void onConnect();
    void onDisconnect();
    void onPlay();
    void onRefreshMacros();

private:
    void setStatus(const QString& s);

    storage::IMacroRepository& repo_;
    std::unique_ptr<web::CdpClient> client_;
    std::unique_ptr<web::WebPlayer> player_;

    QLineEdit* wsUrl_{};
    QPushButton* connectBtn_{};
    QPushButton* disconnectBtn_{};
    QPushButton* playBtn_{};
    QPushButton* refreshBtn_{};
    QComboBox* macroCombo_{};
    QLabel* status_{};
};

}  // namespace autopilot::gui
