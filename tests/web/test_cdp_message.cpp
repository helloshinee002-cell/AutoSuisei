#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "web/CdpMessage.h"

using autopilot::web::CdpEvent;
using autopilot::web::CdpMessage;
using autopilot::web::CdpRequest;
using autopilot::web::CdpResponse;

TEST(CdpMessage, SerializeRequestHasIdMethodParams) {
    CdpRequest req;
    req.id = 7;
    req.method = "Page.navigate";
    req.params = nlohmann::json{{"url", "https://example.com"}};

    const auto str = CdpMessage::serialize(req);
    const auto j = nlohmann::json::parse(str);

    EXPECT_EQ(j["id"].get<int>(), 7);
    EXPECT_EQ(j["method"].get<std::string>(), "Page.navigate");
    EXPECT_EQ(j["params"]["url"].get<std::string>(), "https://example.com");
}

TEST(CdpMessage, SerializeIncludesSessionIdWhenSet) {
    CdpRequest req;
    req.id = 1;
    req.method = "Runtime.evaluate";
    req.sessionId = "ABC123";

    const auto j = nlohmann::json::parse(CdpMessage::serialize(req));
    EXPECT_EQ(j["sessionId"].get<std::string>(), "ABC123");
}

TEST(CdpMessage, SerializeRejectsZeroId) {
    CdpRequest req;
    req.id = 0;
    req.method = "Page.navigate";
    EXPECT_THROW(CdpMessage::serialize(req), std::runtime_error);
}

TEST(CdpMessage, SerializeRejectsEmptyMethod) {
    CdpRequest req;
    req.id = 1;
    req.method = "";
    EXPECT_THROW(CdpMessage::serialize(req), std::runtime_error);
}

TEST(CdpMessage, ParseResponseExtractsResult) {
    const std::string frame = R"({"id": 42, "result": {"frameId": "F1"}})";
    const auto incoming = CdpMessage::parse(frame);

    ASSERT_TRUE(std::holds_alternative<CdpResponse>(incoming));
    const auto& resp = std::get<CdpResponse>(incoming);
    EXPECT_EQ(resp.id, 42);
    EXPECT_EQ(resp.result["frameId"].get<std::string>(), "F1");
    EXPECT_FALSE(resp.error.has_value());
}

TEST(CdpMessage, ParseResponseSurfacesError) {
    const std::string frame =
        R"({"id": 5, "error": {"code": -32601, "message": "Method not found"}})";
    const auto incoming = CdpMessage::parse(frame);

    ASSERT_TRUE(std::holds_alternative<CdpResponse>(incoming));
    const auto& resp = std::get<CdpResponse>(incoming);
    EXPECT_TRUE(resp.error.has_value());
    EXPECT_EQ((*resp.error)["code"].get<int>(), -32601);
}

TEST(CdpMessage, ParseEventHasNoId) {
    const std::string frame =
        R"({"method": "Page.loadEventFired", "params": {"timestamp": 1234.5}})";
    const auto incoming = CdpMessage::parse(frame);

    ASSERT_TRUE(std::holds_alternative<CdpEvent>(incoming));
    const auto& ev = std::get<CdpEvent>(incoming);
    EXPECT_EQ(ev.method, "Page.loadEventFired");
    EXPECT_DOUBLE_EQ(ev.params["timestamp"].get<double>(), 1234.5);
    EXPECT_FALSE(ev.sessionId.has_value());
}

TEST(CdpMessage, ParseEventPicksUpSessionId) {
    const std::string frame =
        R"({"method": "Target.attachedToTarget", "params": {}, "sessionId": "S1"})";
    const auto incoming = CdpMessage::parse(frame);

    ASSERT_TRUE(std::holds_alternative<CdpEvent>(incoming));
    EXPECT_EQ(std::get<CdpEvent>(incoming).sessionId.value(), "S1");
}

TEST(CdpMessage, ParseRejectsFrameMissingIdAndMethod) {
    EXPECT_THROW(CdpMessage::parse(R"({"foo": "bar"})"), std::runtime_error);
}

TEST(CdpMessage, ParseRejectsMalformedJson) {
    EXPECT_THROW(CdpMessage::parse("{ not json"), std::runtime_error);
}
