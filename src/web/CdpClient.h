#pragma once

#include <memory>

#include "ICdpClient.h"

namespace autopilot::web {

/**
 * libwebsockets-backed CDP client
 *
 * Threading model:
 *   - service thread รัน lws_service() loop
 *   - send() เพิ่ม payload ลง outgoing queue + register promise → service thread flush
 *   - incoming response/event ถูก parse ใน service thread → fulfill promise หรือ call event cb
 *
 * Memory: ใช้ pimpl เพื่อไม่ leak libwebsockets header ออกไป
 */
class CdpClient final : public ICdpClient {
public:
    CdpClient();
    ~CdpClient() override;

    CdpClient(const CdpClient&) = delete;
    CdpClient& operator=(const CdpClient&) = delete;

    void connect(const std::string& wsEndpoint) override;
    std::future<nlohmann::json> send(const std::string& method,
                                     const nlohmann::json& params) override;
    void setEventCallback(CdpEventCallback cb) override;
    void disconnect() noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;

    struct Impl;  ///< public so libwebsockets C trampoline can dispatch into it

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace autopilot::web
