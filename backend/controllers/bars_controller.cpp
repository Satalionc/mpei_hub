#include "models.hpp"
#include "bars_client.hpp"
#include "database.hpp"
#include <crow.h>
#include <chrono>

namespace mpei {

void register_bars_routes(crow::SimpleApp& app,
                          BarsClient& bars,
                          Database& db) {
    CROW_ROUTE(app, "/api/bars/grades")
        .methods("GET"_method)
    ([&](const crow::request& req) {
        auto auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) != 0) {
            return crow::response(401, R"({"error":"Authorization: Bearer <token> required"})");
        }
        std::string token = auth.substr(7);
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto cookie = db.session_get(token, now);
        if (!cookie) {
            return crow::response(401, R"({"error":"session expired or invalid"})");
        }

        auto grades = bars.fetch_grades(*cookie);
        if (!grades) {
            return crow::response(502, R"({"error":"failed to fetch grades from BARS"})");
        }

        crow::response r(200);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        r.body = nlohmann::json(*grades).dump(2);
        return r;
    });
}

} // namespace mpei
