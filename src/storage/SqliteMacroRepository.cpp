#include "SqliteMacroRepository.h"

#include <chrono>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "MacroSerializer.h"

namespace autopilot::storage {

namespace {

constexpr const char* kSchemaSql = R"(
CREATE TABLE IF NOT EXISTS macros (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    actions_json TEXT NOT NULL,
    created_at_ms INTEGER NOT NULL,
    updated_at_ms INTEGER NOT NULL
);
)";

std::int64_t nowUnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct StmtGuard {
    sqlite3_stmt* stmt{nullptr};
    ~StmtGuard() {
        if (stmt) sqlite3_finalize(stmt);
    }
};

[[noreturn]] void throwSqliteError(sqlite3* db, const std::string& op) {
    const std::string msg = std::string{"sqlite3 "} + op + ": " + sqlite3_errmsg(db);
    throw std::runtime_error(msg);
}

void exec(sqlite3* db, const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        const std::string what = errMsg ? errMsg : "(unknown)";
        sqlite3_free(errMsg);
        throw std::runtime_error("sqlite3_exec: " + what);
    }
}

core::Macro rowToMacro(sqlite3_stmt* s) {
    auto m = MacroSerializer::fromJson(
        reinterpret_cast<const char*>(sqlite3_column_text(s, 3)));
    m.id = sqlite3_column_int64(s, 0);
    m.name = reinterpret_cast<const char*>(sqlite3_column_text(s, 1));
    m.description = reinterpret_cast<const char*>(sqlite3_column_text(s, 2));
    m.createdAtUnixMs = sqlite3_column_int64(s, 4);
    m.updatedAtUnixMs = sqlite3_column_int64(s, 5);
    return m;
}

}  // namespace

struct SqliteMacroRepository::Db {
    sqlite3* handle{nullptr};

    explicit Db(const std::string& path) {
        if (sqlite3_open(path.c_str(), &handle) != SQLITE_OK) {
            const std::string msg = sqlite3_errmsg(handle);
            sqlite3_close(handle);
            handle = nullptr;
            throw std::runtime_error("sqlite3_open failed: " + msg);
        }
        // ".clauderules" บังคับ
        exec(handle, "PRAGMA journal_mode=WAL;");
        exec(handle, "PRAGMA foreign_keys=ON;");
        exec(handle, kSchemaSql);
    }

    ~Db() {
        if (handle) sqlite3_close(handle);
    }
};

SqliteMacroRepository::SqliteMacroRepository(const std::string& dbPath)
    : db_(std::make_unique<Db>(dbPath)) {}

SqliteMacroRepository::~SqliteMacroRepository() = default;

std::int64_t SqliteMacroRepository::save(const core::Macro& macro) {
    const auto json = MacroSerializer::toJson(macro);
    const auto now = nowUnixMs();
    const auto createdAt = (macro.createdAtUnixMs > 0) ? macro.createdAtUnixMs : now;

    if (macro.id == 0) {
        const char* sql =
            "INSERT INTO macros(name, description, actions_json, created_at_ms, updated_at_ms) "
            "VALUES(?, ?, ?, ?, ?)";
        StmtGuard g;
        if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
            throwSqliteError(db_->handle, "prepare INSERT");
        }
        sqlite3_bind_text(g.stmt, 1, macro.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(g.stmt, 2, macro.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(g.stmt, 3, json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(g.stmt, 4, createdAt);
        sqlite3_bind_int64(g.stmt, 5, now);

        if (sqlite3_step(g.stmt) != SQLITE_DONE) {
            throwSqliteError(db_->handle, "step INSERT");
        }
        return sqlite3_last_insert_rowid(db_->handle);
    }

    const char* sql =
        "UPDATE macros SET name=?, description=?, actions_json=?, updated_at_ms=? WHERE id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare UPDATE");
    }
    sqlite3_bind_text(g.stmt, 1, macro.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.stmt, 2, macro.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.stmt, 3, json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(g.stmt, 4, now);
    sqlite3_bind_int64(g.stmt, 5, macro.id);

    if (sqlite3_step(g.stmt) != SQLITE_DONE) {
        throwSqliteError(db_->handle, "step UPDATE");
    }
    return macro.id;
}

std::optional<core::Macro> SqliteMacroRepository::findById(std::int64_t id) const {
    const char* sql =
        "SELECT id, name, description, actions_json, created_at_ms, updated_at_ms "
        "FROM macros WHERE id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare findById");
    }
    sqlite3_bind_int64(g.stmt, 1, id);

    const int rc = sqlite3_step(g.stmt);
    if (rc == SQLITE_ROW) {
        return rowToMacro(g.stmt);
    }
    if (rc == SQLITE_DONE) return std::nullopt;
    throwSqliteError(db_->handle, "step findById");
}

std::vector<core::Macro> SqliteMacroRepository::findAll() const {
    const char* sql =
        "SELECT id, name, description, actions_json, created_at_ms, updated_at_ms "
        "FROM macros ORDER BY updated_at_ms DESC";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare findAll");
    }
    std::vector<core::Macro> result;
    while (true) {
        const int rc = sqlite3_step(g.stmt);
        if (rc == SQLITE_ROW) {
            result.push_back(rowToMacro(g.stmt));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throwSqliteError(db_->handle, "step findAll");
        }
    }
    return result;
}

void SqliteMacroRepository::remove(std::int64_t id) {
    const char* sql = "DELETE FROM macros WHERE id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare DELETE");
    }
    sqlite3_bind_int64(g.stmt, 1, id);
    if (sqlite3_step(g.stmt) != SQLITE_DONE) {
        throwSqliteError(db_->handle, "step DELETE");
    }
}

}  // namespace autopilot::storage
