#include "models.hpp"
#include "bars_client.hpp"
#include "database.hpp"
#include <crow.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace mpei {

static std::string generate_token() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream os;
    os << std::hex << dist(rng) << dist(rng);
    return os.str();
}

void register_auth_routes(crow::SimpleApp& app,
                          BarsClient& bars,
                          Database& db) {
    CROW_ROUTE(app, "/api/auth/login")
        .methods("POST"_method)
    ([&](const crow::request& req) {
        AuthRequest body;
        try {
            body = nlohmann::json::parse(req.body).get<AuthRequest>();
        } catch (...) {
            return crow::response(400, R"({"error":"invalid JSON body"})");
        }
        if (body.login.empty() || body.password.empty()) {
            return crow::response(400, R"({"error":"login and password required"})");
        }

        auto cookie = bars.login(body.login, body.password);
        AuthResponse resp;
        if (!cookie) {
            resp.success = false;
            resp.message = "Неверный логин или пароль (или сервер БАРС недоступен)";
            crow::response r(401);
            r.set_header("Content-Type", "application/json; charset=utf-8");
            r.body = nlohmann::json(resp).dump();
            return r;
        }

        std::string token = generate_token();
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // Session lives 12 hours
        db.session_put(token, *cookie, now + 12 * 3600);

        resp.success = true;
        resp.token = token;
        resp.message = "OK";
        resp.student_name = body.login; // real name would come from BARS profile

        crow::response r(200);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        r.body = nlohmann::json(resp).dump();
        return r;
    });

    CROW_ROUTE(app, "/api/auth/logout")
        .methods("POST"_method)
    ([&](const crow::request& req) {
        auto auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            std::string token = auth.substr(7);
            db.session_delete(token);
        }
        return crow::response(200, R"({"ok":true})");
    });
}

} // namespace mpei
