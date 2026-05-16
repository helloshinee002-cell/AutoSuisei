#pragma once

#include <functional>
#include <future>
#include <string>

#include <nlohmann/json.hpp>

namespace autopilot::web {

/** Callback สำหรับ event แบบ broadcast — ถูกเรียกจาก service thread ห้าม block */
using CdpEventCallback =
    std::function<void(const std::string& method, const nlohmann::json& params)>;

/** Wrapper ของ Chrome DevTools Protocol — ใช้สำหรับ web recorder/player */
class ICdpClient {
public:
    virtual ~ICdpClient() = default;

    /**
     * เชื่อมต่อกับ Chrome ที่ launch ด้วย --remote-debugging-port=PORT
     * @param wsEndpoint รูปแบบ "ws://host:port/devtools/browser/{guid}"
     * @throws std::runtime_error เมื่อ URL parse fail หรือ connect timeout
     */
    virtual void connect(const std::string& wsEndpoint) = 0;

    /** ส่ง CDP command (เช่น "Page.navigate") คืน future ที่ resolve เมื่อได้ response */
    virtual std::future<nlohmann::json> send(const std::string& method,
                                             const nlohmann::json& params) = 0;

    /** Subscribe event broadcast — set null เพื่อเลิก subscribe */
    virtual void setEventCallback(CdpEventCallback cb) = 0;

    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
};

}  // namespace autopilot::web
