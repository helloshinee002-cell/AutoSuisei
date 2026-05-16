#include "CdpClient.h"

#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <libwebsockets.h>
#include <spdlog/spdlog.h>

#include "CdpMessage.h"

namespace autopilot::web {

namespace {

struct ParsedUrl {
    bool secure{false};
    std::string host;
    int port{0};
    std::string path{"/"};
};

ParsedUrl parseWsUrl(const std::string& url) {
    ParsedUrl out;
    constexpr std::string_view kWs = "ws://";
    constexpr std::string_view kWss = "wss://";

    std::string_view rest;
    if (url.rfind(kWss, 0) == 0) {
        out.secure = true;
        rest = std::string_view{url}.substr(kWss.size());
    } else if (url.rfind(kWs, 0) == 0) {
        rest = std::string_view{url}.substr(kWs.size());
    } else {
        throw std::runtime_error("parseWsUrl: must start with ws:// or wss://");
    }

    const auto slash = rest.find('/');
    const auto hostPort = (slash == std::string_view::npos) ? rest : rest.substr(0, slash);
    out.path = (slash == std::string_view::npos) ? "/" : std::string{rest.substr(slash)};

    const auto colon = hostPort.find(':');
    if (colon == std::string_view::npos) {
        out.host = std::string{hostPort};
        out.port = out.secure ? 443 : 80;
    } else {
        out.host = std::string{hostPort.substr(0, colon)};
        const auto portStr = std::string{hostPort.substr(colon + 1)};
        try {
            out.port = std::stoi(portStr);
        } catch (const std::exception&) {
            throw std::runtime_error("parseWsUrl: invalid port '" + portStr + "'");
        }
    }
    return out;
}

}  // namespace

struct CdpClient::Impl {
    struct lws_context* context{nullptr};
    struct lws* wsi{nullptr};
    struct lws_protocols protocols[2]{};

    std::thread serviceThread;
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> connected{false};

    std::mutex pendingMutex;
    std::unordered_map<int, std::promise<nlohmann::json>> pending;
    std::atomic<int> nextId{1};

    std::mutex outMutex;
    std::deque<std::string> outQueue;

    std::mutex callbackMutex;
    CdpEventCallback eventCb;

    std::string recvAccum;  ///< อ่านระหว่าง fragmented frames

    static inline thread_local Impl* s_dispatch{nullptr};

    int onLws(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in,
              size_t len);
    void runService();
    void handleIncoming(std::string msg);
};

namespace {

int trampoline(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in,
               size_t len) {
    auto* impl = CdpClient::Impl::s_dispatch;
    if (!impl) return 0;
    return impl->onLws(wsi, reason, user, in, len);
}

}  // namespace

int CdpClient::Impl::onLws(struct lws* wsi_in, enum lws_callback_reasons reason,
                           void* /*user*/, void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            connected.store(true, std::memory_order_release);
            spdlog::info("CdpClient: WebSocket connected");
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            spdlog::error("CdpClient: connection error: {}",
                          in ? std::string{static_cast<char*>(in), len} : std::string{"?"});
            connected.store(false, std::memory_order_release);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            recvAccum.append(static_cast<const char*>(in), len);
            if (lws_is_final_fragment(wsi_in)) {
                handleIncoming(std::move(recvAccum));
                recvAccum.clear();
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            std::string payload;
            {
                std::lock_guard<std::mutex> lock(outMutex);
                if (!outQueue.empty()) {
                    payload = std::move(outQueue.front());
                    outQueue.pop_front();
                }
            }
            if (!payload.empty()) {
                std::vector<unsigned char> buf(LWS_PRE + payload.size());
                std::memcpy(buf.data() + LWS_PRE, payload.data(), payload.size());
                const int wrote = lws_write(wsi_in, buf.data() + LWS_PRE, payload.size(),
                                            LWS_WRITE_TEXT);
                if (wrote < static_cast<int>(payload.size())) {
                    spdlog::warn("CdpClient: lws_write truncated ({}/{})", wrote, payload.size());
                }
                std::lock_guard<std::mutex> lock(outMutex);
                if (!outQueue.empty()) {
                    lws_callback_on_writable(wsi_in);
                }
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_CLOSED:
            connected.store(false, std::memory_order_release);
            spdlog::info("CdpClient: WebSocket closed");
            break;

        default:
            break;
    }
    return 0;
}

void CdpClient::Impl::handleIncoming(std::string msg) {
    try {
        auto parsed = CdpMessage::parse(msg);
        std::visit(
            [this](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, CdpResponse>) {
                    std::lock_guard<std::mutex> lock(pendingMutex);
                    auto it = pending.find(v.id);
                    if (it != pending.end()) {
                        if (v.error.has_value()) {
                            it->second.set_exception(std::make_exception_ptr(
                                std::runtime_error("CDP error: " + v.error->dump())));
                        } else {
                            it->second.set_value(std::move(v.result));
                        }
                        pending.erase(it);
                    } else {
                        spdlog::warn("CdpClient: response for unknown id {}", v.id);
                    }
                } else {
                    CdpEventCallback cb;
                    {
                        std::lock_guard<std::mutex> lock(callbackMutex);
                        cb = eventCb;
                    }
                    if (cb) {
                        try {
                            cb(v.method, v.params);
                        } catch (const std::exception& e) {
                            spdlog::error("CdpClient: event callback threw: {}", e.what());
                        }
                    }
                }
            },
            parsed);
    } catch (const std::exception& e) {
        spdlog::error("CdpClient: parse failed: {} (frame={})", e.what(), msg);
    }
}

void CdpClient::Impl::runService() {
    s_dispatch = this;
    while (!shouldStop.load(std::memory_order_acquire)) {
        lws_service(context, 50);
    }
    s_dispatch = nullptr;
}

CdpClient::CdpClient() : impl_(std::make_unique<Impl>()) {}

CdpClient::~CdpClient() {
    disconnect();
}

void CdpClient::connect(const std::string& wsEndpoint) {
    if (impl_->context != nullptr) {
        throw std::runtime_error("CdpClient: already connected");
    }
    const auto parsed = parseWsUrl(wsEndpoint);

    impl_->protocols[0] = lws_protocols{"cdp", &trampoline, 0, 65536, 0, nullptr, 0};
    impl_->protocols[1] = lws_protocols{nullptr, nullptr, 0, 0, 0, nullptr, 0};

    lws_context_creation_info info{};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = impl_->protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.gid = static_cast<gid_t>(-1);
    info.uid = static_cast<uid_t>(-1);

    impl_->context = lws_create_context(&info);
    if (impl_->context == nullptr) {
        throw std::runtime_error("CdpClient: lws_create_context failed");
    }

    lws_client_connect_info ccinfo{};
    ccinfo.context = impl_->context;
    ccinfo.address = parsed.host.c_str();
    ccinfo.port = parsed.port;
    ccinfo.path = parsed.path.c_str();
    ccinfo.host = parsed.host.c_str();
    ccinfo.origin = parsed.host.c_str();
    ccinfo.protocol = impl_->protocols[0].name;
    ccinfo.ssl_connection = parsed.secure ? LCCSCF_USE_SSL : 0;

    impl_->shouldStop.store(false, std::memory_order_release);
    impl_->serviceThread = std::thread([this] { impl_->runService(); });

    impl_->wsi = lws_client_connect_via_info(&ccinfo);
    if (impl_->wsi == nullptr) {
        impl_->shouldStop.store(true, std::memory_order_release);
        if (impl_->serviceThread.joinable()) impl_->serviceThread.join();
        lws_context_destroy(impl_->context);
        impl_->context = nullptr;
        throw std::runtime_error("CdpClient: lws_client_connect_via_info failed");
    }
}

std::future<nlohmann::json> CdpClient::send(const std::string& method,
                                            const nlohmann::json& params) {
    CdpRequest req;
    req.id = impl_->nextId.fetch_add(1, std::memory_order_relaxed);
    req.method = method;
    req.params = params;

    std::promise<nlohmann::json> promise;
    auto future = promise.get_future();
    {
        std::lock_guard<std::mutex> lock(impl_->pendingMutex);
        impl_->pending.emplace(req.id, std::move(promise));
    }

    const auto payload = CdpMessage::serialize(req);
    {
        std::lock_guard<std::mutex> lock(impl_->outMutex);
        impl_->outQueue.push_back(payload);
    }
    if (impl_->wsi) {
        lws_callback_on_writable(impl_->wsi);
    }
    return future;
}

void CdpClient::setEventCallback(CdpEventCallback cb) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->eventCb = std::move(cb);
}

void CdpClient::disconnect() noexcept {
    if (impl_->context == nullptr) return;

    impl_->shouldStop.store(true, std::memory_order_release);
    if (impl_->serviceThread.joinable()) impl_->serviceThread.join();

    lws_context_destroy(impl_->context);
    impl_->context = nullptr;
    impl_->wsi = nullptr;
    impl_->connected.store(false, std::memory_order_release);

    std::lock_guard<std::mutex> lock(impl_->pendingMutex);
    for (auto& [id, promise] : impl_->pending) {
        try {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("CdpClient disconnected before response")));
        } catch (...) {
            // promise might already be satisfied
        }
    }
    impl_->pending.clear();
}

bool CdpClient::isConnected() const noexcept {
    return impl_->connected.load(std::memory_order_acquire);
}

}  // namespace autopilot::web
