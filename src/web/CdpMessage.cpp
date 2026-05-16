#include "CdpMessage.h"

#include <stdexcept>

namespace autopilot::web {

std::string CdpMessage::serialize(const CdpRequest& req) {
    if (req.id <= 0) {
        throw std::runtime_error("CdpMessage::serialize: request id must be > 0");
    }
    if (req.method.empty()) {
        throw std::runtime_error("CdpMessage::serialize: method is required");
    }

    nlohmann::json j;
    j["id"] = req.id;
    j["method"] = req.method;
    j["params"] = req.params;
    if (req.sessionId.has_value()) {
        j["sessionId"] = *req.sessionId;
    }
    return j.dump();
}

CdpIncoming CdpMessage::parse(const std::string& jsonStr) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(jsonStr);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string{"CdpMessage::parse JSON error: "} + e.what());
    }

    if (j.contains("id") && j["id"].is_number_integer()) {
        CdpResponse resp;
        resp.id = j["id"].get<int>();
        if (j.contains("result")) {
            resp.result = j["result"];
        }
        if (j.contains("error")) {
            resp.error = j["error"];
        }
        return resp;
    }

    if (j.contains("method") && j["method"].is_string()) {
        CdpEvent ev;
        ev.method = j["method"].get<std::string>();
        if (j.contains("params")) {
            ev.params = j["params"];
        }
        if (j.contains("sessionId") && j["sessionId"].is_string()) {
            ev.sessionId = j["sessionId"].get<std::string>();
        }
        return ev;
    }

    throw std::runtime_error("CdpMessage::parse: frame has neither id nor method");
}

}  // namespace autopilot::web
