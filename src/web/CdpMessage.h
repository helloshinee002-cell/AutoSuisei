#pragma once

#include <optional>
#include <string>
#include <variant>

#include <nlohmann/json.hpp>

namespace autopilot::web {

/** CDP request ที่ client ส่งไป (id auto-assigned ตอน serialize) */
struct CdpRequest {
    int id{0};
    std::string method;
    nlohmann::json params{nlohmann::json::object()};
    std::optional<std::string> sessionId;
};

/** Response ของ request ตาม id */
struct CdpResponse {
    int id{0};
    nlohmann::json result{nlohmann::json::object()};
    std::optional<nlohmann::json> error;
};

/** Notification จาก browser (ไม่มี id เช่น Page.loadEventFired) */
struct CdpEvent {
    std::string method;
    nlohmann::json params{nlohmann::json::object()};
    std::optional<std::string> sessionId;
};

using CdpIncoming = std::variant<CdpResponse, CdpEvent>;

/**
 * Pure serdes — ไม่มี IO/threading
 * แยกเป็น static เพื่อ unit-test ได้โดยไม่ต้อง spin up WebSocket
 */
class CdpMessage {
public:
    /** Serialize request เป็น JSON string สำหรับส่งทาง WebSocket */
    [[nodiscard]] static std::string serialize(const CdpRequest& req);

    /**
     * Parse incoming frame; แยก response (มี id) กับ event (ไม่มี id) ออก
     * @throws std::runtime_error ถ้า JSON malformed หรือ schema ไม่ตรง
     */
    [[nodiscard]] static CdpIncoming parse(const std::string& jsonStr);
};

}  // namespace autopilot::web
