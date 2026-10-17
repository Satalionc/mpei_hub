#include "schedule_cache.hpp"
#include "database.hpp"
#include <nlohmann/json.hpp>
#include <chrono>

namespace mpei {

ScheduleCache::ScheduleCache(Database& db, std::chrono::seconds ttl)
    : db_(db), ttl_(ttl) {}

std::optional<std::vector<Lesson>> ScheduleCache::get(const std::string& group,
                                                      const std::string& from_date,
                                                      const std::string& to_date) {
    std::string key = "sched:" + group + ":" + from_date + ":" + to_date;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto payload = db_.cache_get(key, now);
    if (!payload) return std::nullopt;
    try {
        auto j = nlohmann::json::parse(*payload);
        return j.get<std::vector<Lesson>>();
    } catch (...) {
        return std::nullopt;
    }
}

void ScheduleCache::put(const std::string& group,
                        const std::string& from_date,
                        const std::string& to_date,
                        const std::vector<Lesson>& lessons) {
    std::string key = "sched:" + group + ":" + from_date + ":" + to_date;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t expires = now + ttl_.count();
    nlohmann::json j = lessons;
    db_.cache_put(key, j.dump(), expires);
}

void ScheduleCache::invalidate(const std::string& group) {
    db_.cache_delete("sched:" + group + ":");
}

} // namespace mpei
