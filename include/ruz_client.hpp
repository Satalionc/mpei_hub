#pragma once

#include "models.hpp"
#include <string>
#include <vector>
#include <optional>

namespace mpei {

/**
 * Client for MPEI RUZ (https://ts.mpei.ru).
 *
 * Working endpoints (discovered 2026-10):
 *   GET /api/search?term=С-10-24
 *     → [{"id":22114,"label":"С-10-24","type":"group",...}, ...]
 *   GET /api/schedule/group/{oid}?start=YYYY.MM.DD&finish=YYYY.MM.DD
 *     → [ { date, beginLesson, endLesson, discipline, kindOfWork,
 *           auditorium, building, lecturer, stream, ... }, ... ]
 */
class RuzClient {
public:
    explicit RuzClient(std::string base_url = "https://ts.mpei.ru");

    std::optional<int> find_group_oid(const std::string& group_name);

    std::vector<Lesson> fetch_schedule(int group_oid,
                                       const std::string& from_date,
                                       const std::string& to_date);

    std::vector<Lesson> fetch_schedule_by_group(const std::string& group_name,
                                               const std::string& from_date,
                                               const std::string& to_date);

private:
    std::string base_url_;
    std::string http_get(const std::string& url);
    std::vector<Lesson> parse_lessons_json(const std::string& json_body);
};

} // namespace mpei
