#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Action.h"

namespace autopilot::core {

/** Macro = ลำดับ Action + metadata */
struct Macro {
    std::int64_t id{0};
    std::string name;
    std::string description;
    std::vector<Action> actions;
    std::int64_t createdAtUnixMs{0};
    std::int64_t updatedAtUnixMs{0};
};

}  // namespace autopilot::core
