#pragma once

#include <optional>
#include <vector>

#include "../core/Macro.h"

namespace autopilot::storage {

class IMacroRepository {
public:
    virtual ~IMacroRepository() = default;

    virtual std::int64_t save(const core::Macro& macro) = 0;
    [[nodiscard]] virtual std::optional<core::Macro> findById(std::int64_t id) const = 0;
    [[nodiscard]] virtual std::vector<core::Macro> findAll() const = 0;
    virtual void remove(std::int64_t id) = 0;
};

}  // namespace autopilot::storage
