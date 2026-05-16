#include "LuaSandbox.h"

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace autopilot::scripting {

namespace {

constexpr const char* kDangerousGlobals[] = {
    "loadfile", "dofile", "load", "loadstring", "require",
    "package", "debug", "io", "os"
};

constexpr const char* kDangerousOsFns[] = {
    "execute", "remove", "rename", "exit", "tmpname", "getenv", "setlocale"
};

}  // namespace

struct LuaSandbox::Impl {
    sol::state state;
    SandboxConfig config;

    void setupBase() {
        state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                              sol::lib::table, sol::lib::utf8);
        // os และ io ไม่ได้ open แต่ default state มี global อยู่บ้าง — clear
        for (const char* g : kDangerousGlobals) {
            state[g] = sol::lua_nil;
        }
    }

    void exposeAp() {
        sol::table ap = state.create_named_table("ap");
        ap.set_function("sleep", [](int ms) {
            if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds{ms});
        });
        ap.set_function("log", [](const std::string& msg) { spdlog::info("[lua] {}", msg); });
        ap.set_function("version", [] { return std::string{"AutoPilot Lua 1"}; });
    }
};

LuaSandbox::LuaSandbox(SandboxConfig config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
    impl_->setupBase();
    impl_->exposeAp();
}

LuaSandbox::~LuaSandbox() = default;

std::string LuaSandbox::run(const std::string& source) {
    sol::protected_function_result result = impl_->state.safe_script(
        source, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string{"Lua error: "} + err.what());
    }

    if (result.return_count() == 0) return "";

    sol::object obj = result;
    switch (obj.get_type()) {
        case sol::type::nil: return "nil";
        case sol::type::boolean: return obj.as<bool>() ? "true" : "false";
        case sol::type::number: {
            std::ostringstream oss;
            oss << obj.as<double>();
            return oss.str();
        }
        case sol::type::string: return obj.as<std::string>();
        default: return "<unprintable>";
    }
}

bool LuaSandbox::hasGlobal(const std::string& name) const {
    return impl_->state[name].valid() &&
           impl_->state[name].get_type() != sol::type::nil;
}

}  // namespace autopilot::scripting
