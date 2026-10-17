#include "models.hpp"
#include "ruz_client.hpp"
#include "schedule_cache.hpp"
#include "database.hpp"
#include <crow.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>
#include <ctime>
#include <iostream>

namespace mpei {

static std::pair<std::string, std::string> current_week_range() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    int wday = tm.tm_wday;
    int days_from_monday = (wday == 0) ? 6 : wday - 1;
    auto monday = now - hours(24 * days_from_monday);
    auto sunday = monday + hours(24 * 6);

    auto fmt = [](system_clock::time_point tp) {
        std::time_t tt = system_clock::to_time_t(tp);
        std::tm ttm = *std::localtime(&tt);
        std::ostringstream os;
        os << std::put_time(&ttm, "%Y-%m-%d");
        return os.str();
    };
    return {fmt(monday), fmt(sunday)};
}

static ScheduleResponse build_response(const std::string& group,
                                       const std::string& from,
                                       const std::string& to,
                                       const std::vector<Lesson>& lessons,
                                       bool from_cache) {
    ScheduleResponse resp;
    resp.group = group;
    resp.from_date = from;
    resp.to_date = to;
    resp.from_cache = from_cache;

    std::map<std::string, std::vector<Lesson>> by_date;
    for (const auto& L : lessons) {
        by_date[L.date].push_back(L);
    }
    for (auto& [date, ls] : by_date) {
        std::sort(ls.begin(), ls.end(), [](const Lesson& a, const Lesson& b) {
            return a.begin_time < b.begin_time;
        });
        ScheduleDay day;
        day.date = date;
        day.lessons = std::move(ls);
        resp.days.push_back(std::move(day));
    }
    std::sort(resp.days.begin(), resp.days.end(),
              [](const ScheduleDay& a, const ScheduleDay& b) { return a.date < b.date; });
    return resp;
}

void register_schedule_routes(crow::SimpleApp& app,
                              RuzClient& ruz,
                              ScheduleCache& cache) {
    CROW_ROUTE(app, "/api/schedule")
        .methods("GET"_method)
    ([&](const crow::request& req) {
        std::string group_str;
        if (auto group = req.url_params.get("group")) group_str = group;
        while (!group_str.empty() && (group_str.front()==' '||group_str.front()=='\t'))
            group_str.erase(group_str.begin());
        while (!group_str.empty() && (group_str.back()==' '||group_str.back()=='\t'))
            group_str.pop_back();

        if (group_str.empty()) {
            crow::response r(400);
            r.set_header("Content-Type", "application/json; charset=utf-8");
            r.set_header("Access-Control-Allow-Origin", "*");
            r.body = R"({"error":"query parameter 'group' is required"})";
            return r;
        }

        auto [from, to] = current_week_range();
        if (auto f = req.url_params.get("from")) from = f;
        if (auto t = req.url_params.get("to")) to = t;

        // Step-by-step diagnostics
        nlohmann::json debug;
        debug["group_received"] = group_str;
        debug["from"] = from;
        debug["to"] = to;

        auto oid = ruz.find_group_oid(group_str);
        if (!oid) {
            debug["step"] = "find_group_oid FAILED";
            debug["oid"] = nullptr;
            nlohmann::json j;
            j["group"] = group_str;
            j["from_date"] = from;
            j["to_date"] = to;
            j["from_cache"] = false;
            j["days"] = nlohmann::json::array();
            j["lesson_count"] = 0;
            j["day_count"] = 0;
            j["debug"] = debug;
            crow::response r(200);
            r.set_header("Content-Type", "application/json; charset=utf-8");
            r.set_header("Access-Control-Allow-Origin", "*");
            r.body = j.dump(2);
            return r;
        }
        debug["oid"] = *oid;
        debug["step_find"] = "ok";

        auto lessons = ruz.fetch_schedule(*oid, from, to);
        debug["step_fetch"] = "ok";
        debug["raw_lesson_count"] = lessons.size();

        if (!lessons.empty()) {
            cache.put(group_str, from, to, lessons);
            // sample first lesson for debug
            debug["sample"] = {
                {"date", lessons[0].date},
                {"begin", lessons[0].begin_time},
                {"discipline", lessons[0].discipline}
            };
        } else {
            debug["step_fetch"] = "EMPTY — curl failed or parse failed, see server stderr [RuzClient]";
        }

        auto resp = build_response(group_str, from, to, lessons, false);
        nlohmann::json j = resp;
        j["lesson_count"] = lessons.size();
        j["day_count"] = resp.days.size();
        j["debug"] = debug;

        crow::response r(200);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        r.set_header("Access-Control-Allow-Origin", "*");
        r.body = j.dump(2);
        return r;
    });

    // Full pipeline test
    CROW_ROUTE(app, "/api/debug/full")
        .methods("GET"_method)
    ([&](const crow::request& req) {
        std::string term = "ЭРэ-41-26";
        if (auto t = req.url_params.get("term")) term = t;
        std::string from = "2026-10-12";
        std::string to = "2026-10-18";
        if (auto f = req.url_params.get("from")) from = f;
        if (auto t = req.url_params.get("to")) to = t;

        nlohmann::json j;
        j["term"] = term;
        j["from"] = from;
        j["to"] = to;

        auto oid = ruz.find_group_oid(term);
        if (!oid) {
            j["ok"] = false;
            j["error"] = "find_group_oid failed";
            crow::response r(200);
            r.set_header("Content-Type", "application/json; charset=utf-8");
            r.set_header("Access-Control-Allow-Origin", "*");
            r.body = j.dump(2);
            return r;
        }
        j["oid"] = *oid;

        auto lessons = ruz.fetch_schedule(*oid, from, to);
        j["lesson_count"] = lessons.size();
        j["ok"] = !lessons.empty();
        if (!lessons.empty()) {
            j["first"] = {
                {"date", lessons[0].date},
                {"begin", lessons[0].begin_time},
                {"end", lessons[0].end_time},
                {"discipline", lessons[0].discipline},
                {"kind", lessons[0].kind_of_work},
                {"auditorium", lessons[0].auditorium}
            };
        }
        crow::response r(200);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        r.set_header("Access-Control-Allow-Origin", "*");
        r.body = j.dump(2);
        return r;
    });

    CROW_ROUTE(app, "/api/debug/search")
        .methods("GET"_method)
    ([&](const crow::request& req) {
        std::string term = "ЭРэ-41-26";
        if (auto t = req.url_params.get("term")) term = t;
        auto oid = ruz.find_group_oid(term);
        nlohmann::json j;
        j["term"] = term;
        j["ok"] = oid.has_value();
        if (oid) j["oid"] = *oid;
        else j["oid"] = nullptr;
        crow::response r(200);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        r.set_header("Access-Control-Allow-Origin", "*");
        r.body = j.dump(2);
        return r;
    });
}

} // namespace mpei
