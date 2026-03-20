#include "cloud/webservice_api.hpp"

#include "esp_crt_bundle.h"
#include "esp_log.h"

static const char* TAG = "WebServiceApi";

namespace app::cloud {

WebServiceApi::Result WebServiceApi::PostJson(const std::string& url, const std::string& body,
                                             std::string& response, uint32_t timeout_ms)
{
    if (url.empty() || body.empty())
    {
        ESP_LOGE(TAG, "PostJson: Invalid parameters");
        return Result::kInvalidParam;
    }

    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = timeout_ms;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr)
    {
        ESP_LOGE(TAG, "PostJson: Failed to init HTTP client");
        return Result::kNetworkError;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body.c_str(), body.length());

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "PostJson: HTTP error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return Result::kNetworkError;
    }

    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    if (content_length < 0)
    {
        content_length = 0;
    }

    response.clear();
    response.reserve(static_cast<size_t>(content_length + 256));

    char buffer[256];
    while (true)
    {
        int read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read <= 0)
        {
            break;
        }
        response.append(buffer, static_cast<size_t>(read));
    }

    esp_http_client_cleanup(client);

    if (status != 200)
    {
        ESP_LOGE(TAG, "PostJson: HTTP %d: %s", status, response.c_str());
        return Result::kHttpError;
    }

    return Result::kSuccess;
}

WebServiceApi::Result WebServiceApi::GetJson(const std::string& url, std::string& response,
                                            uint32_t timeout_ms)
{
    if (url.empty())
    {
        ESP_LOGE(TAG, "GetJson: Invalid URL");
        return Result::kInvalidParam;
    }

    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.method = HTTP_METHOD_GET;
    cfg.timeout_ms = timeout_ms;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr)
    {
        ESP_LOGE(TAG, "GetJson: Failed to init HTTP client");
        return Result::kNetworkError;
    }

    esp_http_client_set_header(client, "Accept", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GetJson: HTTP error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return Result::kNetworkError;
    }

    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    if (content_length < 0)
    {
        content_length = 0;
    }

    response.clear();
    response.reserve(static_cast<size_t>(content_length + 256));

    char buffer[256];
    while (true)
    {
        int read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read <= 0)
        {
            break;
        }
        response.append(buffer, static_cast<size_t>(read));
    }

    esp_http_client_cleanup(client);

    if (status != 200)
    {
        ESP_LOGE(TAG, "GetJson: HTTP %d: %s", status, response.c_str());
        return Result::kHttpError;
    }

    return Result::kSuccess;
}

const char* WebServiceApi::ResultToString(Result result)
{
    switch (result)
    {
        case Result::kSuccess:
            return "Success";
        case Result::kNetworkError:
            return "Network Error";
        case Result::kHttpError:
            return "HTTP Error";
        case Result::kParseError:
            return "Parse Error";
        case Result::kInvalidParam:
            return "Invalid Parameter";
        case Result::kTimeout:
            return "Timeout";
        default:
            return "Unknown Error";
    }
}

} // namespace app::cloud
