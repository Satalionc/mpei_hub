#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sqlite3.h>

namespace mpei {

class Database {
public:
    explicit Database(const std::string& path = "mpei_app.db");
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool ok() const { return db_ != nullptr; }

    // Generic exec
    bool exec(const std::string& sql);
    std::optional<std::string> query_scalar(const std::string& sql);

    // Schedule cache helpers
    bool cache_put(const std::string& key, const std::string& json_payload, int64_t expires_at);
    std::optional<std::string> cache_get(const std::string& key, int64_t now);
    bool cache_delete(const std::string& key_prefix);

    // Session store (token → bars cookie)
    bool session_put(const std::string& token, const std::string& bars_cookie, int64_t expires_at);
    std::optional<std::string> session_get(const std::string& token, int64_t now);
    bool session_delete(const std::string& token);

private:
    sqlite3* db_ = nullptr;
    void init_schema();
};

} // namespace mpei
