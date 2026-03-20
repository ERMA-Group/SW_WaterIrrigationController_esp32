#pragma once

#include <string>
#include <cstdint>

extern "C" {
#include "esp_http_client.h"
}

namespace app::cloud {

/**
 * @brief Base class for web service API calls
 * Provides common HTTP infrastructure for communicating with backend server
 */
class WebServiceApi {
public:
    WebServiceApi() = default;
    virtual ~WebServiceApi() = default;

    /**
     * @brief Result of an API call
     */
    enum class Result {
        kSuccess,           ///< Request succeeded with HTTP 200
        kNetworkError,      ///< Network/socket error
        kHttpError,         ///< Non-200 HTTP status code
        kParseError,        ///< Failed to parse JSON response
        kInvalidParam,      ///< Invalid parameter supplied
        kTimeout,           ///< Request timed out
    };

    /**
     * @brief Execute a POST request to the server
     * @param url Target URL
     * @param body JSON request body (null-terminated string)
     * @param response Output: server response body
     * @param timeout_ms Timeout in milliseconds
     * @return Result enum indicating success/failure
     */
    static Result PostJson(const std::string& url, const std::string& body, 
                          std::string& response, uint32_t timeout_ms = 15000);

    /**
     * @brief Execute a GET request to the server
     * @param url Target URL
     * @param response Output: server response body
     * @param timeout_ms Timeout in milliseconds
     * @return Result enum indicating success/failure
     */
    static Result GetJson(const std::string& url, std::string& response, 
                         uint32_t timeout_ms = 15000);

    /**
     * @brief Check if a result indicates success
     */
    static bool IsSuccess(Result result) { return result == Result::kSuccess; }

    /**
     * @brief Get human-readable error message
     */
    static const char* ResultToString(Result result);
};

} // namespace app::cloud
