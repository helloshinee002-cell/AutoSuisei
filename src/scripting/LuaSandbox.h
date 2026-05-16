#pragma once

#include <memory>
#include <string>
#include <vector>

namespace autopilot::scripting {

struct SandboxConfig {
    std::vector<std::string> allowedReadPaths;
    std::vector<std::string> allowedWritePaths;
    bool allowNetwork{false};
};

/**
 * Lua sandbox สำหรับรัน user script
 *
 * Removes ทันทีหลัง state init:
 *   - os.execute, os.remove, os.rename, os.exit, os.tmpname
 *   - io.popen, io.open (จะมี wrapper จำกัด path ภายหลัง)
 *   - require, loadfile, dofile, load, loadstring, package, debug
 *
 * Provides global `ap`:
 *   - ap.sleep(ms)   — sleep ปัจจุบัน thread
 *   - ap.log(msg)    — log ผ่าน spdlog::info
 *   - ap.version()   — คืน "AutoPilot Lua 1"
 */
class LuaSandbox {
public:
    explicit LuaSandbox(SandboxConfig config = {});
    ~LuaSandbox();

    LuaSandbox(const LuaSandbox&) = delete;
    LuaSandbox& operator=(const LuaSandbox&) = delete;

    /**
     * รัน script source คืนค่า return value แปลงเป็น string
     * @throws std::runtime_error เมื่อ syntax error หรือ runtime error ใน Lua
     */
    std::string run(const std::string& source);

    /** ตรวจว่า global ชื่อนี้ยังเข้าถึงได้ใน sandbox — ใช้ใน test */
    [[nodiscard]] bool hasGlobal(const std::string& name) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace autopilot::scripting
