#include "cloud/device_claim_client.hpp"

#include <algorithm>
#include <cctype>

#include "cJSON.h"
#include "esp_log.h"

static const char* TAG = "DeviceClaimClient";

namespace app::cloud {

namespace {

std::string trim_copy(const std::string& input)
{
    const auto first = std::find_if_not(input.begin(), input.end(),
                                        [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(input.rbegin(), input.rend(),
                                       [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (first >= last)
    {
        return "";
    }
    return std::string(first, last);
}

bool is_valid_pairing_pin(const std::string& pin)
{
    if (pin.size() != 6)
    {
        return false;
    }

    for (char ch : pin)
    {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (std::isalnum(u) == 0)
        {
            return false;
        }
    }

    return true;
}

} // namespace

DeviceClaimClient::ClaimResult DeviceClaimClient::Claim(
    const std::string& hw_id, const std::string& pin, const std::string& server_url)
{
    if (hw_id.empty() || pin.empty() || !is_valid_pairing_pin(pin))
    {
        ESP_LOGE(TAG, "Claim: Invalid hw_id or pin format");
        return ClaimResult::kInvalidParam;
    }

    // Build request payload: {"hw_id": "...", "pin": "..."}
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hw_id", hw_id.c_str());
    cJSON_AddStringToObject(root, "pin", pin.c_str());

    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr)
    {
        ESP_LOGE(TAG, "Claim: Failed to serialize JSON");
        return ClaimResult::kParseError;
    }

    // Determine server URL
    std::string url = server_url.empty() ? kDefaultClaimUrl : server_url;

    // Make POST request
    std::string response;
    WebServiceApi::Result api_result = WebServiceApi::PostJson(url, std::string(body), response, 15000);
    cJSON_free(body);

    if (!WebServiceApi::IsSuccess(api_result))
    {
        ESP_LOGE(TAG, "Claim: API call failed: %s", WebServiceApi::ResultToString(api_result));
        return ClaimResult::kApiCallFailed;
    }

    const std::string trimmed_response = trim_copy(response);

    if (trimmed_response.empty())
    {
        ESP_LOGW(TAG, "Claim: Empty response body on HTTP 200, assuming success");
        return ClaimResult::kSuccess;
    }

    if (trimmed_response.find("success") != std::string::npos)
    {
        ESP_LOGI(TAG, "Claim: Plain-text success response detected");
        return ClaimResult::kSuccess;
    }

    // Parse response: {"status": "success"} or error message
    cJSON* resp_json = cJSON_Parse(trimmed_response.c_str());
    if (resp_json == nullptr)
    {
        ESP_LOGE(TAG, "Claim: Failed to parse response: %s", trimmed_response.c_str());
        return ClaimResult::kParseError;
    }

    ClaimResult result = ClaimResult::kParseError;
    cJSON* status = cJSON_GetObjectItem(resp_json, "status");
    if (cJSON_IsString(status) && status->valuestring != nullptr)
    {
        if (strcmp(status->valuestring, "success") == 0)
        {
            result = ClaimResult::kSuccess;
            ESP_LOGI(TAG, "Claim: Device successfully claimed!");
        }
        else if (strcmp(status->valuestring, "invalid_pin") == 0)
        {
            result = ClaimResult::kInvalidPin;
            ESP_LOGW(TAG, "Claim: Invalid or expired PIN");
        }
        else if (strcmp(status->valuestring, "already_claimed") == 0)
        {
            result = ClaimResult::kAlreadyClaimed;
            ESP_LOGW(TAG, "Claim: Device already claimed");
        }
        else
        {
            result = ClaimResult::kParseError;
            ESP_LOGW(TAG, "Claim: Unknown status: %s", status->valuestring);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Claim: Missing or invalid 'status' field in response");
    }

    cJSON_Delete(resp_json);
    return result;
}

const char* DeviceClaimClient::ResultToString(ClaimResult result)
{
    switch (result)
    {
        case ClaimResult::kSuccess:
            return "Success";
        case ClaimResult::kApiCallFailed:
            return "API Call Failed";
        case ClaimResult::kInvalidPin:
            return "Invalid or Expired PIN";
        case ClaimResult::kAlreadyClaimed:
            return "Device Already Claimed";
        case ClaimResult::kParseError:
            return "Parse Error";
        case ClaimResult::kInvalidParam:
            return "Invalid Parameter";
        default:
            return "Unknown Error";
    }
}

} // namespace app::cloud
