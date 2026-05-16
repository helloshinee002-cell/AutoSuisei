#include "SqliteOcrResultRepository.h"

#include <stdexcept>

#include <sqlite3.h>

namespace autopilot::storage {

namespace {

constexpr const char* kSchema = R"(
CREATE TABLE IF NOT EXISTS ocr_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    extracted_text TEXT NOT NULL DEFAULT '',
    confidence REAL NOT NULL DEFAULT 0,
    timestamp_ms INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_ocr_filename ON ocr_results(filename);
)";

struct StmtGuard {
    sqlite3_stmt* stmt{nullptr};
    ~StmtGuard() {
        if (stmt) sqlite3_finalize(stmt);
    }
};

[[noreturn]] void throwSqliteError(sqlite3* db, const std::string& op) {
    throw std::runtime_error(std::string{"sqlite3 "} + op + ": " + sqlite3_errmsg(db));
}

void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string what = err ? err : "(unknown)";
        sqlite3_free(err);
        throw std::runtime_error("sqlite3_exec: " + what);
    }
}

StoredOcrResult rowToResult(sqlite3_stmt* s) {
    StoredOcrResult r;
    r.id = sqlite3_column_int64(s, 0);
    r.filename = reinterpret_cast<const char*>(sqlite3_column_text(s, 1));
    r.extractedText = reinterpret_cast<const char*>(sqlite3_column_text(s, 2));
    r.confidence = static_cast<float>(sqlite3_column_double(s, 3));
    r.timestampMs = sqlite3_column_int64(s, 4);
    return r;
}

}  // namespace

struct SqliteOcrResultRepository::Db {
    sqlite3* handle{nullptr};

    explicit Db(const std::string& path) {
        if (sqlite3_open(path.c_str(), &handle) != SQLITE_OK) {
            const std::string msg = sqlite3_errmsg(handle);
            sqlite3_close(handle);
            throw std::runtime_error("sqlite3_open: " + msg);
        }
        exec(handle, "PRAGMA journal_mode=WAL;");
        exec(handle, "PRAGMA foreign_keys=ON;");
        exec(handle, kSchema);
    }

    ~Db() {
        if (handle) sqlite3_close(handle);
    }
};

SqliteOcrResultRepository::SqliteOcrResultRepository(const std::string& dbPath)
    : db_(std::make_unique<Db>(dbPath)) {}

SqliteOcrResultRepository::~SqliteOcrResultRepository() = default;

std::int64_t SqliteOcrResultRepository::insert(const StoredOcrResult& r) {
    const char* sql =
        "INSERT INTO ocr_results(filename, extracted_text, confidence, timestamp_ms) "
        "VALUES(?, ?, ?, ?)";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare insert");
    }
    sqlite3_bind_text(g.stmt, 1, r.filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.stmt, 2, r.extractedText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(g.stmt, 3, r.confidence);
    sqlite3_bind_int64(g.stmt, 4, r.timestampMs);
    if (sqlite3_step(g.stmt) != SQLITE_DONE) {
        throwSqliteError(db_->handle, "step insert");
    }
    return sqlite3_last_insert_rowid(db_->handle);
}

std::vector<StoredOcrResult> SqliteOcrResultRepository::findByFilename(
    const std::string& filename) const {
    const char* sql =
        "SELECT id, filename, extracted_text, confidence, timestamp_ms "
        "FROM ocr_results WHERE filename=? ORDER BY timestamp_ms DESC";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare findByFilename");
    }
    sqlite3_bind_text(g.stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    std::vector<StoredOcrResult> out;
    int rc;
    while ((rc = sqlite3_step(g.stmt)) == SQLITE_ROW) {
        out.push_back(rowToResult(g.stmt));
    }
    if (rc != SQLITE_DONE) throwSqliteError(db_->handle, "step findByFilename");
    return out;
}

std::vector<StoredOcrResult> SqliteOcrResultRepository::findAll() const {
    const char* sql =
        "SELECT id, filename, extracted_text, confidence, timestamp_ms "
        "FROM ocr_results ORDER BY timestamp_ms DESC LIMIT 5000";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_->handle, sql, -1, &g.stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_->handle, "prepare findAll");
    }
    std::vector<StoredOcrResult> out;
    int rc;
    while ((rc = sqlite3_step(g.stmt)) == SQLITE_ROW) {
        out.push_back(rowToResult(g.stmt));
    }
    if (rc != SQLITE_DONE) throwSqliteError(db_->handle, "step findAll");
    return out;
}

}  // namespace autopilot::storage
