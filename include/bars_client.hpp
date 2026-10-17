#pragma once

#include "models.hpp"
#include <string>
#include <optional>
#include <map>

namespace mpei {

/**
 * Client for BARS MPEI (https://bars.mpei.ru).
 *
 * Authentication is based on the corporate MPEI account (ОСЭП).
 * The real login flow uses form POST + cookies / 2FA in some cases.
 * This skeleton implements a session-cookie proxy pattern:
 *   1. POST credentials → receive Set-Cookie
 *   2. Subsequent requests carry the cookie
 *   3. We expose a short-lived internal token to the frontend
 *
 * NOTE: Real endpoints and form field names may change; adjust after
 * inspecting the live login page with browser DevTools.
 */
class BarsClient {
public:
    explicit BarsClient(std::string base_url = "https://bars.mpei.ru");

    // Perform login. On success returns session cookie string.
    std::optional<std::string> login(const std::string& login,
                                     const std::string& password);

    // Fetch grades using a previously obtained session cookie.
    std::optional<BarsGradesResponse> fetch_grades(const std::string& session_cookie);

    // Validate that session is still alive
    bool ping(const std::string& session_cookie);

private:
    std::string base_url_;
    std::string http_request(const std::string& method,
                             const std::string& url,
                             const std::string& body = "",
                             const std::map<std::string, std::string>& headers = {});
};

} // namespace mpei
