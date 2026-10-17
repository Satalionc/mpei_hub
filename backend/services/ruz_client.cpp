#include "ruz_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace mpei {

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string url_encode(CURL* curl, const std::string& s) {
    char* enc = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.size()));
    std::string r = enc ? enc : s;
    if (enc) curl_free(enc);
    return r;
}

std::string to_ruz_date(std::string d) {
    std::replace(d.begin(), d.end(), '-', '.');
    return d;
}

std::string from_ruz_date(std::string d) {
    std::replace(d.begin(), d.end(), '.', '-');
    return d;
}

std::string norm(std::string s) {
    while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
    while (!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
    return s;
}

// nlohmann::value() throws type_error.302 when key exists but is null
// and default is a string. Always use this helper.
std::string jstr(const nlohmann::json& j, const char* key, const std::string& def = "") {
    if (!j.contains(key) || j[key].is_null()) return def;
    if (j[key].is_string()) return j[key].get<std::string>();
    if (j[key].is_number_integer()) return std::to_string(j[key].get<long long>());
    if (j[key].is_number_float()) return std::to_string(j[key].get<double>());
    if (j[key].is_boolean()) return j[key].get<bool>() ? "true" : "false";
    return def;
}

int jint(const nlohmann::json& j, const char* key, int def = 0) {
    if (!j.contains(key) || j[key].is_null()) return def;
    if (j[key].is_number_integer()) return j[key].get<int>();
    if (j[key].is_number_float()) return static_cast<int>(j[key].get<double>());
    if (j[key].is_string()) {
        try { return std::stoi(j[key].get<std::string>()); } catch (...) {}
    }
    return def;
}
} // namespace

RuzClient::RuzClient(std::string base_url) : base_url_(std::move(base_url)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::string RuzClient::http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MPEI-Student-App/0.3");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[RuzClient] curl error on " << url << ": "
                  << curl_easy_strerror(res) << std::endl;
        return {};
    }
    if (http_code >= 400) {
        std::cerr << "[RuzClient] HTTP " << http_code << " on " << url << std::endl;
        return {};
    }
    return response;
}

std::optional<int> RuzClient::find_group_oid(const std::string& group_name) {
    const std::string wanted = norm(group_name);
    if (wanted.empty()) return std::nullopt;

    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    const std::string url = base_url_ + "/api/search?term=" + url_encode(curl, wanted);
    curl_easy_cleanup(curl);

    std::cerr << "[RuzClient] search URL: " << url << std::endl;
    auto body = http_get(url);
    if (body.empty()) {
        std::cerr << "[RuzClient] empty search body for \"" << wanted << "\"\n";
        return std::nullopt;
    }
    std::cerr << "[RuzClient] search body size=" << body.size()
              << " head=" << body.substr(0, std::min<size_t>(120, body.size())) << "\n";

    try {
        auto j = nlohmann::json::parse(body);
        if (!j.is_array()) return std::nullopt;

        for (const auto& item : j) {
            if (jstr(item, "type") != "group") continue;
            if (norm(jstr(item, "label")) == wanted) {
                int id = jint(item, "id");
                std::cerr << "[RuzClient] exact match id=" << id << "\n";
                return id;
            }
        }
        for (const auto& item : j) {
            if (jstr(item, "type") != "group") continue;
            std::string label = norm(jstr(item, "label"));
            if (label.find(wanted) != std::string::npos || wanted.find(label) != std::string::npos) {
                int id = jint(item, "id");
                std::cerr << "[RuzClient] partial match id=" << id << " label=" << label << "\n";
                return id;
            }
        }
        std::vector<int> group_ids;
        for (const auto& item : j) {
            if (jstr(item, "type") == "group") {
                int id = jint(item, "id", -1);
                if (id >= 0) group_ids.push_back(id);
            }
        }
        if (group_ids.size() == 1) {
            std::cerr << "[RuzClient] single group result id=" << group_ids[0] << "\n";
            return group_ids[0];
        }
    } catch (const std::exception& e) {
        std::cerr << "[RuzClient] search parse: " << e.what() << "\n";
    }
    std::cerr << "[RuzClient] group not found: \"" << wanted << "\"\n";
    return std::nullopt;
}

std::vector<Lesson> RuzClient::parse_lessons_json(const std::string& json_body) {
    std::vector<Lesson> result;
    if (json_body.empty() || json_body == "[]") return result;

    try {
        auto j = nlohmann::json::parse(json_body);
        if (!j.is_array()) {
            std::cerr << "[RuzClient] schedule not array, head="
                      << json_body.substr(0, 100) << "\n";
            return result;
        }
        result.reserve(j.size());
        int skipped = 0;
        for (const auto& item : j) {
            try {
                Lesson L;
                L.date         = from_ruz_date(jstr(item, "date"));
                L.begin_time   = jstr(item, "beginLesson");
                L.end_time     = jstr(item, "endLesson");
                L.discipline   = jstr(item, "discipline");
                if (L.discipline.empty()) L.discipline = jstr(item, "subject");
                L.kind_of_work = jstr(item, "kindOfWork");
                L.auditorium   = jstr(item, "auditorium");
                L.building     = jstr(item, "building");
                L.lecturer     = jstr(item, "lecturer");
                if (L.lecturer.empty()) L.lecturer = jstr(item, "lecturer_title");
                L.group        = jstr(item, "stream");
                if (L.group.empty()) L.group = jstr(item, "group");
                L.stream_oid   = jint(item, "streamOid");
                L.lesson_oid   = jint(item, "lessonOid");

                if (!L.date.empty() && !L.begin_time.empty())
                    result.push_back(std::move(L));
                else
                    ++skipped;
            } catch (const std::exception& e) {
                ++skipped;
                std::cerr << "[RuzClient] skip lesson: " << e.what() << "\n";
            }
        }
        if (skipped)
            std::cerr << "[RuzClient] skipped " << skipped << " malformed lessons\n";
    } catch (const std::exception& e) {
        std::cerr << "[RuzClient] schedule parse: " << e.what() << "\n";
    }
    return result;
}

std::vector<Lesson> RuzClient::fetch_schedule(int group_oid,
                                              const std::string& from_date,
                                              const std::string& to_date) {
    if (group_oid <= 0) return {};

    std::ostringstream url;
    url << base_url_ << "/api/schedule/group/" << group_oid
        << "?start=" << to_ruz_date(from_date)
        << "&finish=" << to_ruz_date(to_date);

    std::cerr << "[RuzClient] schedule URL: " << url.str() << "\n";
    auto body = http_get(url.str());
    std::cerr << "[RuzClient] schedule body size=" << body.size() << "\n";
    auto lessons = parse_lessons_json(body);
    std::cerr << "[RuzClient] parsed lessons=" << lessons.size() << "\n";
    return lessons;
}

std::vector<Lesson> RuzClient::fetch_schedule_by_group(const std::string& group_name,
                                                      const std::string& from_date,
                                                      const std::string& to_date) {
    auto oid = find_group_oid(group_name);
    if (!oid) {
        std::cerr << "[RuzClient] resolve failed for \"" << group_name << "\"\n";
        return {};
    }
    auto lessons = fetch_schedule(*oid, from_date, to_date);
    for (auto& L : lessons) {
        if (L.group.empty()) L.group = group_name;
    }
    return lessons;
}

} // namespace mpei
