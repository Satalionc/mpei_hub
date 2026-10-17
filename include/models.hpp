#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace mpei {

struct Lesson {
    std::string date;          // YYYY-MM-DD
    std::string begin_time;    // HH:MM
    std::string end_time;      // HH:MM
    std::string discipline;
    std::string kind_of_work;  // Лекция / Практика / Лабораторная ...
    std::string auditorium;
    std::string building;
    std::string lecturer;
    std::string group;
    int stream_oid = 0;
    int lesson_oid = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Lesson,
        date, begin_time, end_time, discipline, kind_of_work,
        auditorium, building, lecturer, group, stream_oid, lesson_oid)
};

struct ScheduleDay {
    std::string date;
    std::vector<Lesson> lessons;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ScheduleDay, date, lessons)
};

struct ScheduleResponse {
    std::string group;
    std::string from_date;
    std::string to_date;
    std::vector<ScheduleDay> days;
    bool from_cache = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ScheduleResponse,
        group, from_date, to_date, days, from_cache)
};

struct BarsGradeItem {
    std::string discipline;
    std::string control_type;   // КМ, тест, лаб ...
    std::string date;
    double max_score = 0.0;
    double score = 0.0;
    std::string comment;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BarsGradeItem,
        discipline, control_type, date, max_score, score, comment)
};

struct BarsDiscipline {
    std::string name;
    double current_score = 0.0;
    double max_score = 0.0;
    std::string final_grade;    // зачёт / оценка
    std::vector<BarsGradeItem> items;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BarsDiscipline,
        name, current_score, max_score, final_grade, items)
};

struct BarsGradesResponse {
    std::string student_name;
    std::string group;
    std::string semester;
    std::vector<BarsDiscipline> disciplines;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BarsGradesResponse,
        student_name, group, semester, disciplines)
};

struct AuthRequest {
    std::string login;
    std::string password;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AuthRequest, login, password)
};

struct AuthResponse {
    bool success = false;
    std::string token;          // session token / cookie value we manage
    std::string message;
    std::string student_name;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AuthResponse,
        success, token, message, student_name)
};

} // namespace mpei
