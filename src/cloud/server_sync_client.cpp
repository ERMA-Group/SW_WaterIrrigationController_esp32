#include "cloud/server_sync_client.hpp"

#include <cstring>

#include "freertos/idf_additions.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_system.h"

namespace app::cloud {

const char* ServerSyncClient::kDefaultSyncUrl = "https://aqua.erma.sk/api/device_sync.php";
const char* ServerSyncClient::kDefaultSseUrlBase = "https://aqua.erma.sk/api/commands.php?hw_id=";

void ServerSyncClient::configure(const char* sync_url, const char* sse_url_base, uint32_t sync_period_ms)
{
    if (sync_url != nullptr && sync_url[0] != '\0')
    {
        sync_url_ = sync_url;
    }
    if (sse_url_base != nullptr && sse_url_base[0] != '\0')
    {
        sse_url_base_ = sse_url_base;
    }
    if (sync_period_ms != 0U)
    {
        sync_period_ms_ = sync_period_ms;
    }
}

void ServerSyncClient::setCommandHandlers(const CommandHandlers& handlers)
{
    command_handlers_ = handlers;
}

void ServerSyncClient::setDeviceHwId(const std::string& hw_id)
{
    device_hw_id_ = hw_id;
}

void ServerSyncClient::setFirmwareVersion(const std::string& fw_version)
{
    firmware_version_ = fw_version.empty() ? "0.0.0" : fw_version;
}

void ServerSyncClient::setValveCount(uint32_t valve_count)
{
    if (valve_count == 0U)
    {
        valve_count = 1U;
    }
    valve_states_.assign(static_cast<size_t>(valve_count), false);
}

void ServerSyncClient::setValveState(uint32_t valve_index, bool is_open)
{
    const size_t index = static_cast<size_t>(valve_index);
    if (index >= valve_states_.size())
    {
        return;
    }
    valve_states_[index] = is_open;
}

void ServerSyncClient::start()
{
    if (sync_task_handle_ == nullptr)
    {
        xTaskCreatePinnedToCore(
            &ServerSyncClient::syncTaskEntry,
            "device_sync",
            8192,
            this,
            4,
            &sync_task_handle_,
            1);
    }

    if (sse_task_handle_ == nullptr)
    {
        xTaskCreatePinnedToCore(
            &ServerSyncClient::sseTaskEntry,
            "device_sse",
            10240,
            this,
            4,
            &sse_task_handle_,
            0);
    }
}

void ServerSyncClient::syncTaskEntry(void* arg)
{
    auto* self = static_cast<ServerSyncClient*>(arg);
    if (self != nullptr)
    {
        self->syncTaskLoop();
    }
    vTaskDelete(nullptr);
}

void ServerSyncClient::sseTaskEntry(void* arg)
{
    auto* self = static_cast<ServerSyncClient*>(arg);
    if (self != nullptr)
    {
        self->sseTaskLoop();
    }
    vTaskDelete(nullptr);
}

void ServerSyncClient::syncTaskLoop()
{
    while (true)
    {
        if (!postDeviceSync())
        {
            printf("device_sync failed (will retry)\n");
        }
        vTaskDelay(pdMS_TO_TICKS(sync_period_ms_));
    }
}

bool ServerSyncClient::postDeviceSync()
{
    if (device_hw_id_.empty())
    {
        return false;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hw_id", device_hw_id_.c_str());
    cJSON_AddStringToObject(root, "status", "Online");
    cJSON_AddStringToObject(root, "system_health", "OK");
    cJSON_AddStringToObject(root, "fw_version", firmware_version_.c_str());

    cJSON* valves = cJSON_AddArrayToObject(root, "valves_telemetry");
    for (size_t i = 0; i < valve_states_.size(); ++i)
    {
        cJSON* valve = cJSON_CreateObject();
        cJSON_AddNumberToObject(valve, "id", static_cast<double>(i));
        cJSON_AddStringToObject(valve, "status", valve_states_[i] ? "open" : "closed");
        cJSON_AddNumberToObject(valve, "battery", 100);
        cJSON_AddNumberToObject(valve, "signal", 5);
        cJSON_AddItemToArray(valves, valve);
    }

    printf("device_sync telemetry valves=%lu\n", static_cast<unsigned long>(valve_states_.size()));

    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr)
    {
        return false;
    }

    esp_http_client_config_t cfg = {};
    cfg.url = sync_url_.c_str();
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr)
    {
        cJSON_free(body);
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, std::strlen(body));

    const esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        printf("device_sync http error: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        cJSON_free(body);
        return false;
    }

    const int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    if (content_length < 0)
    {
        content_length = 0;
    }

    std::string response;
    response.reserve(static_cast<size_t>(content_length));
    char buffer[256];
    while (true)
    {
        const int read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read <= 0)
        {
            break;
        }
        response.append(buffer, static_cast<size_t>(read));
    }

    esp_http_client_cleanup(client);
    cJSON_free(body);

    if (status != 200)
    {
        printf("device_sync status code: %d, body='%s'\n", status, response.c_str());
        return false;
    }

    if (response.empty())
    {
        if (command_handlers_.sync_ok != nullptr)
        {
            command_handlers_.sync_ok(command_handlers_.context);
        }
        return true;
    }

    cJSON* resp_json = cJSON_Parse(response.c_str());
    if (resp_json == nullptr)
    {
        printf("device_sync warning: response parse failed, body='%s'\n", response.c_str());
        if (command_handlers_.sync_ok != nullptr)
        {
            command_handlers_.sync_ok(command_handlers_.context);
        }
        return true;
    }

    cJSON* valve_commands = cJSON_GetObjectItem(resp_json, "valve_commands");
    if (valve_commands != nullptr && cJSON_IsArray(valve_commands))
    {
        const int command_count = cJSON_GetArraySize(valve_commands);
        for (int i = 0; i < command_count; ++i)
        {
            cJSON* command = cJSON_GetArrayItem(valve_commands, i);
            if (!cJSON_IsObject(command))
            {
                continue;
            }

            cJSON* valve_id = cJSON_GetObjectItem(command, "id");
            cJSON* operating_mode = cJSON_GetObjectItem(command, "operating_mode");
            if (cJSON_IsNumber(valve_id) && cJSON_IsString(operating_mode) && operating_mode->valuestring != nullptr)
            {
                printf("valve %lu mode from cloud: %s\n",
                       static_cast<unsigned long>(valve_id->valuedouble),
                       operating_mode->valuestring);
            }
        }
    }

    cJSON_Delete(resp_json);

    if (command_handlers_.sync_ok != nullptr)
    {
        command_handlers_.sync_ok(command_handlers_.context);
    }

    return true;
}

void ServerSyncClient::sseTaskLoop()
{
    while (true)
    {
        if (device_hw_id_.empty())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        std::string url = sse_url_base_ + device_hw_id_;

        esp_http_client_config_t cfg = {};
        cfg.url = url.c_str();
        cfg.method = HTTP_METHOD_GET;
        cfg.timeout_ms = 45000;
        cfg.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client == nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        esp_http_client_set_header(client, "Accept", "text/event-stream");
        esp_http_client_set_header(client, "Cache-Control", "no-cache");

        if (esp_http_client_open(client, 0) != ESP_OK)
        {
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        if (esp_http_client_fetch_headers(client) < 0)
        {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        std::string line_buffer;
        bool is_command_event = false;
        char chunk[256];

        while (true)
        {
            int r = esp_http_client_read(client, chunk, sizeof(chunk));
            if (r <= 0)
            {
                break;
            }

            for (int i = 0; i < r; ++i)
            {
                char c = chunk[i];
                if (c == '\r')
                {
                    continue;
                }
                if (c == '\n')
                {
                    processSseLine(line_buffer, is_command_event);
                    line_buffer.clear();
                }
                else
                {
                    line_buffer.push_back(c);
                }
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ServerSyncClient::processSseLine(const std::string& line, bool& is_command_event)
{
    if (line.empty())
    {
        is_command_event = false;
        return;
    }

    if (line.rfind("event:", 0) == 0)
    {
        const std::string event_name = line.substr(6);
        is_command_event = (event_name.find("command") != std::string::npos);
        return;
    }

    if (line.rfind("data:", 0) == 0 && is_command_event)
    {
        std::string payload = line.substr(5);
        while (!payload.empty() && payload.front() == ' ')
        {
            payload.erase(payload.begin());
        }
        handleSseCommandJson(payload);
    }
}

void ServerSyncClient::handleSseCommandJson(const std::string& json_text)
{
    cJSON* json = cJSON_Parse(json_text.c_str());
    if (json == nullptr)
    {
        printf("SSE command parse failed: %s\n", json_text.c_str());
        return;
    }

    cJSON* action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == nullptr)
    {
        cJSON_Delete(json);
        return;
    }

    printf("SSE action received: %s\n", action->valuestring);

    if (std::strcmp(action->valuestring, "manual_run") == 0)
    {
        cJSON* valve_index = cJSON_GetObjectItem(json, "valve_id");
        cJSON* duration = cJSON_GetObjectItem(json, "duration_sec");
        if (cJSON_IsNumber(valve_index) && cJSON_IsNumber(duration) && command_handlers_.manual_run != nullptr)
        {
            command_handlers_.manual_run(
                command_handlers_.context,
                static_cast<uint32_t>(valve_index->valuedouble),
                static_cast<uint32_t>(duration->valuedouble));
        }
    }
    else if (std::strcmp(action->valuestring, "stop_run") == 0)
    {
        cJSON* valve_index = cJSON_GetObjectItem(json, "valve_id");
        if (cJSON_IsNumber(valve_index) && command_handlers_.stop_run != nullptr)
        {
            command_handlers_.stop_run(
                command_handlers_.context,
                static_cast<uint32_t>(valve_index->valuedouble));
        }
    }
    else if (std::strcmp(action->valuestring, "restart") == 0)
    {
        if (command_handlers_.restart != nullptr)
        {
            command_handlers_.restart(command_handlers_.context);
        }
    }
    else if (std::strcmp(action->valuestring, "factory_reset") == 0)
    {
        printf("SSE factory_reset command accepted.\n");
        if (command_handlers_.factory_reset != nullptr)
        {
            command_handlers_.factory_reset(command_handlers_.context);
        }
    }
    else if (std::strcmp(action->valuestring, "update") == 0)
    {
        cJSON* fw = cJSON_GetObjectItem(json, "firmware_url");
        if (cJSON_IsString(fw) && fw->valuestring != nullptr && command_handlers_.update != nullptr)
        {
            command_handlers_.update(command_handlers_.context, fw->valuestring);
        }
    }

    cJSON_Delete(json);
}

} // namespace app::cloud
