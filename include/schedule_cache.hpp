#pragma once

#include "models.hpp"
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace mpei {

class Database; // forward

/**
 * Simple cache layer over SQLite for RUZ schedules.
 * TTL default = 6 hours.
 */
class ScheduleCache {
public:
    explicit ScheduleCache(Database& db, std::chrono::seconds ttl = std::chrono::hours(6));

    std::optional<std::vector<Lesson>> get(const std::string& group,
                                           const std::string& from_date,
                                           const std::string& to_date);

    void put(const std::string& group,
             const std::string& from_date,
             const std::string& to_date,
             const std::vector<Lesson>& lessons);

    void invalidate(const std::string& group);

private:
    Database& db_;
    std::chrono::seconds ttl_;
};

} // namespace mpei
