#pragma once

#include <string>

#include "core/Macro.h"

namespace autopilot::storage {

/**
 * Pure (no I/O) conversion ระหว่าง `core::Macro` ↔ JSON
 *
 * รูปแบบ JSON (เวอร์ชัน 1):
 * {
 *   "schemaVersion": 1,
 *   "id": 123,
 *   "name": "...",
 *   "description": "...",
 *   "createdAtUnixMs": 1700000000000,
 *   "updatedAtUnixMs": 1700000000000,
 *   "actions": [
 *     { "kind": "KeyDown", "timestampNs": 12345, "key": {"vk": 65, "ext": false} },
 *     { "kind": "MouseClick", "timestampNs": 12350, "mouse": {"x":100,"y":200,"button":0,"scroll":0} },
 *     { "kind": "Delay", "timestampNs": 12400, "delay": {"ms": 500} },
 *     ...
 *   ]
 * }
 */
class MacroSerializer {
public:
    /** @throws std::runtime_error เมื่อ macro อยู่ในสถานะ invalid (เช่น variant ไม่ตรงกับ kind) */
    [[nodiscard]] static std::string toJson(const core::Macro& macro);

    /** @throws std::runtime_error เมื่อ JSON malformed หรือ schemaVersion ไม่รองรับ */
    [[nodiscard]] static core::Macro fromJson(const std::string& json);
};

}  // namespace autopilot::storage
