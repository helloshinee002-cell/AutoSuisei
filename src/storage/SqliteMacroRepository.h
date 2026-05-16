#pragma once

#include <memory>
#include <string>

#include "IMacroRepository.h"

namespace autopilot::storage {

/**
 * SQLite-backed repository
 *
 * เปิด DB ด้วย `journal_mode=WAL` + `foreign_keys=ON` (ตาม .clauderules)
 * Schema สร้างอัตโนมัติถ้ายังไม่มี (CREATE TABLE IF NOT EXISTS)
 *
 * รับ ":memory:" สำหรับ unit test
 */
class SqliteMacroRepository final : public IMacroRepository {
public:
    explicit SqliteMacroRepository(const std::string& dbPath);
    ~SqliteMacroRepository() override;

    SqliteMacroRepository(const SqliteMacroRepository&) = delete;
    SqliteMacroRepository& operator=(const SqliteMacroRepository&) = delete;

    std::int64_t save(const core::Macro& macro) override;
    [[nodiscard]] std::optional<core::Macro> findById(std::int64_t id) const override;
    [[nodiscard]] std::vector<core::Macro> findAll() const override;
    void remove(std::int64_t id) override;

private:
    struct Db;
    std::unique_ptr<Db> db_;
};

}  // namespace autopilot::storage
