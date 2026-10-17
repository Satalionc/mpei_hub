#include "ruz_client.hpp"
#include "bars_client.hpp"
#include "schedule_cache.hpp"
#include "database.hpp"
#include <crow.h>
#include <iostream>
#include <filesystem>

// Forward declarations of route registrars
namespace mpei {
void register_schedule_routes(crow::SimpleApp& app, RuzClient& ruz, ScheduleCache& cache);
void register_auth_routes(crow::SimpleApp& app, BarsClient& bars, Database& db);
void register_bars_routes(crow::SimpleApp& app, BarsClient& bars, Database& db);
}

int main(int argc, char** argv) {
    using namespace mpei;

    int port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);

    Database db("mpei_app.db");
    if (!db.ok()) {
        std::cerr << "Failed to open SQLite database\n";
        return 1;
    }

    RuzClient ruz("https://ts.mpei.ru");
    BarsClient bars("https://bars.mpei.ru");
    ScheduleCache cache(db);

    crow::SimpleApp app;

    // CORS middleware for local frontend development
    CROW_ROUTE(app, "/api/<path>")
        .methods("OPTIONS"_method)
    ([](const crow::request&, std::string) {
        crow::response r(204);
        r.set_header("Access-Control-Allow-Origin", "*");
        r.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        r.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        return r;
    });

    register_schedule_routes(app, ruz, cache);
    register_auth_routes(app, bars, db);
    register_bars_routes(app, bars, db);

    // Serve frontend static files
    CROW_ROUTE(app, "/")
    ([]() {
        crow::response r;
        r.set_static_file_info("frontend/index.html");
        return r;
    });

    CROW_ROUTE(app, "/<path>")
    ([](const crow::request&, std::string path) {
        // Simple static file server under frontend/
        std::string file = "frontend/" + path;
        if (std::filesystem::exists(file) && std::filesystem::is_regular_file(file)) {
            crow::response r;
            r.set_static_file_info(file);
            return r;
        }
        return crow::response(404);
    });

    // Add CORS headers to all responses via a simple filter is hard in Crow;
    // for production put nginx in front. For demos the OPTIONS handler is enough.

    std::cout << "MPEI Student App listening on http://0.0.0.0:" << port << std::endl;
    std::cout << "  GET  /api/schedule?group=С-10-24\n";
    std::cout << "  POST /api/auth/login\n";
    std::cout << "  GET  /api/bars/grades  (Authorization: Bearer <token>)\n";
    app.port(port).multithreaded().run();
    return 0;
}
