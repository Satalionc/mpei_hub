#include "bars_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace mpei {

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

struct HeaderData {
    std::string cookies;
};

size_t header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    std::string line(buffer, total);
    auto* hd = static_cast<HeaderData*>(userdata);
    if (line.find("Set-Cookie:") == 0 || line.find("set-cookie:") == 0) {
        size_t start = line.find(':') + 1;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
        size_t end = line.find(';', start);
        if (end == std::string::npos) end = line.find('\r', start);
        if (end == std::string::npos) end = line.size();
        std::string cookie = line.substr(start, end - start);
        if (!hd->cookies.empty()) hd->cookies += "; ";
        hd->cookies += cookie;
    }
    return total;
}
} // namespace

BarsClient::BarsClient(std::string base_url) : base_url_(std::move(base_url)) {}

std::string BarsClient::http_request(const std::string& method,
                                     const std::string& url,
                                     const std::string& body,
                                     const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string response;
    HeaderData hd;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hd);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MPEI-Student-App/0.1");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    struct curl_slist* hdrs = nullptr;
    for (const auto& [k, v] : headers) {
        std::string h = k + ": " + v;
        hdrs = curl_slist_append(hdrs, h.c_str());
    }
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        if (!body.empty()) {
            hdrs = curl_slist_append(hdrs, "Content-Type: application/x-www-form-urlencoded");
        }
    }
    if (hdrs) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    if (hdrs) curl_slist_free_all(hdrs);

    if (res != CURLE_OK) {
        std::cerr << "[BarsClient] " << method << " " << url << " failed: "
                  << curl_easy_strerror(res) << std::endl;
        return {};
    }
    // Store cookies in response via a side channel? For login we need them.
    // We return body; cookies are collected in hd — caller of login uses a special path.
    (void)http_code;
    return response;
}

std::optional<std::string> BarsClient::login(const std::string& login,
                                             const std::string& password) {
    // Typical ASP.NET / form login flow for BARS.
    // Real form fields must be confirmed with browser DevTools on bars.mpei.ru.
    // Common pattern: POST /bars_web/ or /Account/Login with UserName / Password.
    std::ostringstream body;
    // URL-encode simple values
    auto encode = [](const std::string& s) {
        CURL* c = curl_easy_init();
        char* enc = curl_easy_escape(c, s.c_str(), 0);
        std::string r = enc ? enc : s;
        if (enc) curl_free(enc);
        curl_easy_cleanup(c);
        return r;
    };
    body << "UserName=" << encode(login)
         << "&Password=" << encode(password)
         << "&RememberMe=false";

    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    std::string response;
    HeaderData hd;
    std::string url = base_url_ + "/bars_web/";  // adjust after inspecting live site
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    std::string post = body.str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hd);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L); // capture 302 + Set-Cookie
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MPEI-Student-App/0.1");

    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    if (hdrs) curl_slist_free_all(hdrs);

    if (res != CURLE_OK) {
        std::cerr << "[BarsClient] login request failed" << std::endl;
        return std::nullopt;
    }

    // Success indicators: 302 redirect or presence of session cookie
    if (!hd.cookies.empty() && (http_code == 302 || http_code == 200)) {
        return hd.cookies;
    }

    // Development fallback: accept any non-empty credentials and return a mock cookie
    if (!login.empty() && !password.empty()) {
        std::cerr << "[BarsClient] Live login did not return cookies — using mock session."
                  << std::endl;
        return std::string("MOCK_SESSION=" + login);
    }
    return std::nullopt;
}

std::optional<BarsGradesResponse> BarsClient::fetch_grades(const std::string& session_cookie) {
    if (session_cookie.find("MOCK_SESSION=") == 0) {
        // Mock response for skeleton
        BarsGradesResponse resp;
        resp.student_name = "Студент Тестовый";
        resp.group = "С-10-24";
        resp.semester = "Осенний 2025/2026";
        BarsDiscipline d;
        d.name = "Теоретические основы электротехники";
        d.current_score = 42.5;
        d.max_score = 100.0;
        d.final_grade = "";
        BarsGradeItem item;
        item.discipline = d.name;
        item.control_type = "КМ-1";
        item.date = "2025-10-01";
        item.max_score = 20.0;
        item.score = 18.0;
        d.items.push_back(item);
        resp.disciplines.push_back(d);
        return resp;
    }

    std::map<std::string, std::string> headers;
    headers["Cookie"] = session_cookie;
    // Real endpoint must be discovered from the student cabinet network tab
    std::string url = base_url_ + "/bars_web/Student/Grades"; // placeholder
    auto body = http_request("GET", url, "", headers);
    if (body.empty()) return std::nullopt;

    // TODO: parse real HTML/JSON response once structure is known
    try {
        auto j = nlohmann::json::parse(body);
        return j.get<BarsGradesResponse>();
    } catch (...) {
        std::cerr << "[BarsClient] Could not parse grades response as JSON" << std::endl;
        return std::nullopt;
    }
}

bool BarsClient::ping(const std::string& session_cookie) {
    if (session_cookie.find("MOCK_SESSION=") == 0) return true;
    std::map<std::string, std::string> headers;
    headers["Cookie"] = session_cookie;
    auto body = http_request("GET", base_url_ + "/bars_web/", "", headers);
    return !body.empty();
}

} // namespace mpei
