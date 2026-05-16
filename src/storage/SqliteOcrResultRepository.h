#pragma once

#include <memory>
#include <string>

#include "IOcrResultRepository.h"

namespace autopilot::storage {

class SqliteOcrResultRepository final : public IOcrResultRepository {
public:
    explicit SqliteOcrResultRepository(const std::string& dbPath);
    ~SqliteOcrResultRepository() override;

    SqliteOcrResultRepository(const SqliteOcrResultRepository&) = delete;
    SqliteOcrResultRepository& operator=(const SqliteOcrResultRepository&) = delete;

    std::int64_t insert(const StoredOcrResult& result) override;
    [[nodiscard]] std::vector<StoredOcrResult> findByFilename(
        const std::string& filename) const override;
    [[nodiscard]] std::vector<StoredOcrResult> findAll() const override;

private:
    struct Db;
    std::unique_ptr<Db> db_;
};

}  // namespace autopilot::storage
