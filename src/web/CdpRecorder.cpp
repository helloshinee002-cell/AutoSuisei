#include "CdpRecorder.h"

#include <chrono>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "BrowserEvent.h"
#include "BrowserRecorderScript.h"

namespace autopilot::web {

namespace {

nlohmann::json await(std::future<nlohmann::json>&& f, const std::string& label) {
    if (f.wait_for(std::chrono::seconds{5}) != std::future_status::ready) {
        throw std::runtime_error("CdpRecorder: timeout on " + label);
    }
    return f.get();
}

}  // namespace

CdpRecorder::CdpRecorder(ICdpClient& client, std::string bindingName)
    : client_(client), bindingName_(std::move(bindingName)) {}

void CdpRecorder::start(ActionCallback onAction) {
    if (recording_) {
        throw std::runtime_error("CdpRecorder: already recording");
    }
    if (!client_.isConnected()) {
        throw std::runtime_error("CdpRecorder: CDP client not connected");
    }
    callback_ = std::move(onAction);

    await(client_.send("Page.enable", {}), "Page.enable");
    await(client_.send("Runtime.enable", {}), "Runtime.enable");
    await(client_.send("Runtime.addBinding", {{"name", bindingName_}}),
          "Runtime.addBinding");

    const auto src = BrowserRecorderScript::source(bindingName_);
    auto resp = await(client_.send("Page.addScriptToEvaluateOnNewDocument",
                                    {{"source", src}}),
                      "Page.addScriptToEvaluateOnNewDocument");
    scriptIdentifier_ = resp.value("identifier", std::string{});

    client_.setEventCallback(
        [this](const std::string& method, const nlohmann::json& params) {
            onCdpEvent(method, params);
        });

    recording_ = true;
}

void CdpRecorder::stop() noexcept {
    if (!recording_) return;
    recording_ = false;
    callback_ = nullptr;
    client_.setEventCallback(nullptr);

    try {
        if (!scriptIdentifier_.empty()) {
            client_.send("Page.removeScriptToEvaluateOnNewDocument",
                         {{"identifier", scriptIdentifier_}});
        }
        client_.send("Runtime.removeBinding", {{"name", bindingName_}});
    } catch (const std::exception& e) {
        spdlog::warn("CdpRecorder::stop: cleanup CDP error: {}", e.what());
    }
    scriptIdentifier_.clear();
}

void CdpRecorder::onCdpEvent(const std::string& method, const nlohmann::json& params) {
    if (method != "Runtime.bindingCalled") return;
    if (params.value("name", std::string{}) != bindingName_) return;

    const auto payload = params.value("payload", std::string{});
    auto action = BrowserEvent::toAction(payload);
    if (!action.has_value()) return;

    if (callback_) {
        try {
            callback_(*action);
        } catch (const std::exception& e) {
            spdlog::error("CdpRecorder: action callback threw: {}", e.what());
        }
    }
}

}  // namespace autopilot::web
