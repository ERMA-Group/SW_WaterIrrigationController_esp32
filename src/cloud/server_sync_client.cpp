#include "cloud/server_sync_client.hpp"

#include <cstdio>
#include <cstring>

#include "freertos/idf_additions.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "cloud/webservice_api.hpp"

namespace app::cloud {

namespace {

std::string get_reset_reason_text()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON: return "Power On";
        case ESP_RST_EXT: return "External";
        case ESP_RST_SW: return "Software";
        case ESP_RST_PANIC: return "Panic";
        case ESP_RST_INT_WDT: return "Interrupt WDT";
        case ESP_RST_TASK_WDT: return "Task WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO";
        default: return "Unknown";
    }
}

std::string get_wifi_ssid()
{
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        return "";
    }

    const char* ssid = reinterpret_cast<const char*>(ap_info.ssid);
    const size_t ssid_len = strnlen(ssid, sizeof(ap_info.ssid));
    return std::string(ssid, ssid_len);
}

int get_wifi_rssi()
{
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        return 0;
    }
    return static_cast<int>(ap_info.rssi);
}

std::string get_ip_address()
{
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr)
    {
        return "";
    }

    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK)
    {
        return "";
    }

    char ip_buf[16] = {0};
    std::snprintf(ip_buf,
                  sizeof(ip_buf),
                  "%u.%u.%u.%u",
                  static_cast<unsigned int>(ip4_addr1(&ip_info.ip)),
                  static_cast<unsigned int>(ip4_addr2(&ip_info.ip)),
                  static_cast<unsigned int>(ip4_addr3(&ip_info.ip)),
                  static_cast<unsigned int>(ip4_addr4(&ip_info.ip)));
    return std::string(ip_buf);
}

std::string get_mac_address()
{
    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK)
    {
        return "";
    }

    char mac_buf[18] = {0};
    std::snprintf(mac_buf,
                  sizeof(mac_buf),
                  "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0],
                  mac[1],
                  mac[2],
                  mac[3],
                  mac[4],
                  mac[5]);
    return std::string(mac_buf);
}

} // namespace

const char* ServerSyncClient::kDefaultSyncUrl = "https://aqua.erma.sk/api/device_sync.php";
const char* ServerSyncClient::kDefaultPollUrlBase = "https://aqua.erma.sk/api/commands.php?hw_id=";

ServerSyncClient::ServerSyncClient()
{
    state_mutex_ = xSemaphoreCreateMutex();
}

ServerSyncClient::~ServerSyncClient()
{
    if (state_mutex_ != nullptr)
    {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = nullptr;
    }
}

void ServerSyncClient::configure(const char* sync_url, const char* poll_url_base, uint32_t sync_period_ms)
{
    if (sync_url != nullptr && sync_url[0] != '\0')
    {
        sync_url_ = sync_url;
    }
    if (poll_url_base != nullptr && poll_url_base[0] != '\0')
    {
        poll_url_base_ = poll_url_base;
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

void ServerSyncClient::setValveConnectionType(const std::string& connection_type)
{
    if (connection_type.empty())
    {
        return;
    }
    valve_connection_type_ = connection_type;
}

void ServerSyncClient::setValveCount(uint32_t valve_count)
{
    if (valve_count == 0U)
    {
        valve_count = 1U;
    }
    if (state_mutex_ != nullptr)
    {
        xSemaphoreTake(state_mutex_, portMAX_DELAY);
    }
    valve_states_.assign(static_cast<size_t>(valve_count), false);
    if (state_mutex_ != nullptr)
    {
        xSemaphoreGive(state_mutex_);
    }
}

void ServerSyncClient::setValveState(uint32_t valve_index, bool is_open)
{
    if (state_mutex_ != nullptr)
    {
        xSemaphoreTake(state_mutex_, portMAX_DELAY);
    }
    const size_t index = static_cast<size_t>(valve_index);
    if (index >= valve_states_.size())
    {
        if (state_mutex_ != nullptr)
        {
            xSemaphoreGive(state_mutex_);
        }
        return;
    }
    valve_states_[index] = is_open;
    if (state_mutex_ != nullptr)
    {
        xSemaphoreGive(state_mutex_);
    }
}

void ServerSyncClient::requestImmediateSync()
{
    if (sync_task_handle_ != nullptr)
    {
        xTaskNotifyGive(sync_task_handle_);
    }
}

void ServerSyncClient::start()
{
    printf("server_sync start: hw_id='%s' sync_period_ms=%lu\n",
           device_hw_id_.c_str(),
           static_cast<unsigned long>(sync_period_ms_));

    if (sync_task_handle_ == nullptr)
    {
        const BaseType_t rc = xTaskCreatePinnedToCore(
            &ServerSyncClient::syncTaskEntry,
            "device_sync",
            8192,
            this,
            4,
            &sync_task_handle_,
            1);
        if (rc != pdPASS)
        {
            sync_task_handle_ = nullptr;
            printf("server_sync: failed to create sync task\n");
        }
        else
        {
            printf("server_sync: sync task created handle=%p\n", static_cast<void*>(sync_task_handle_));
        }
    }
    else
    {
        printf("server_sync: sync task already running handle=%p\n", static_cast<void*>(sync_task_handle_));
    }

    if (poll_task_handle_ == nullptr)
    {
        const BaseType_t rc = xTaskCreatePinnedToCore(
            &ServerSyncClient::pollTaskEntry,
            "device_poll",
            10240,
            this,
            4,
            &poll_task_handle_,
            0);
        if (rc != pdPASS)
        {
            poll_task_handle_ = nullptr;
            printf("server_sync: failed to create poll task\n");
        }
        else
        {
            printf("server_sync: poll task created handle=%p\n", static_cast<void*>(poll_task_handle_));
        }
    }
    else
    {
        printf("server_sync: poll task already running handle=%p\n", static_cast<void*>(poll_task_handle_));
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

void ServerSyncClient::pollTaskEntry(void* arg)
{
    printf("commands_poll entry arg=%p\n", arg);
    auto* self = static_cast<ServerSyncClient*>(arg);
    if (self != nullptr)
    {
        self->pollTaskLoop();
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

        // Wake early when requestImmediateSync() notifies this task.
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sync_period_ms_));
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
    cJSON_AddStringToObject(root, "wifi_ssid", get_wifi_ssid().c_str());
    cJSON_AddNumberToObject(root, "wifi_signal", get_wifi_rssi());
    cJSON_AddStringToObject(root, "ip_address", get_ip_address().c_str());
    cJSON_AddStringToObject(root, "mac_address", get_mac_address().c_str());
    cJSON_AddStringToObject(root, "reset_reason", get_reset_reason_text().c_str());

    std::vector<bool> valve_states_snapshot;
    if (state_mutex_ != nullptr)
    {
        xSemaphoreTake(state_mutex_, portMAX_DELAY);
    }
    valve_states_snapshot = valve_states_;
    if (state_mutex_ != nullptr)
    {
        xSemaphoreGive(state_mutex_);
    }

    cJSON* valves = cJSON_AddArrayToObject(root, "valves_telemetry");
    for (size_t i = 0; i < valve_states_snapshot.size(); ++i)
    {
        cJSON* valve = cJSON_CreateObject();
        cJSON_AddNumberToObject(valve, "id", static_cast<double>(i));
        cJSON_AddStringToObject(valve, "status", valve_states_snapshot[i] ? "open" : "closed");
        cJSON_AddNumberToObject(valve, "battery", 100);
        cJSON_AddNumberToObject(valve, "signal", 5);
        cJSON_AddStringToObject(valve, "type", valve_connection_type_.c_str());
        if (command_handlers_.augment_sync_valve_payload != nullptr)
        {
            command_handlers_.augment_sync_valve_payload(command_handlers_.context,
                                                         static_cast<uint32_t>(i),
                                                         valve);
        }
        cJSON_AddItemToArray(valves, valve);
    }

    printf("device_sync telemetry valves=%lu\n", static_cast<unsigned long>(valve_states_snapshot.size()));

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

    uint32_t server_time_unix = 0U;
    cJSON* server_time = cJSON_GetObjectItem(resp_json, "server_time");
    if (cJSON_IsNumber(server_time) && server_time->valuedouble > 0)
    {
        server_time_unix = static_cast<uint32_t>(server_time->valuedouble);
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

            applyServerValveState(command, server_time_unix);

            if (cJSON_IsNumber(valve_id) && command_handlers_.sync_valve_command != nullptr)
            {
                const uint32_t raw_id = static_cast<uint32_t>(valve_id->valuedouble);
                uint32_t mapped_index = 0U;
                if (mapValveIdToIndex(raw_id, mapped_index))
                {
                    command_handlers_.sync_valve_command(command_handlers_.context,
                                                         mapped_index,
                                                         command);
                }
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

void ServerSyncClient::pollTaskLoop()
{
    printf("commands_poll task started\n");
    printf("commands_poll interval ms=%lu\n", static_cast<unsigned long>(sync_period_ms_));
    uint32_t poll_tick = 0U;

    while (true)
    {
        if (device_hw_id_.empty())
        {
            printf("commands_poll waiting: device_hw_id is empty\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ++poll_tick;
        const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
        printf("commands_poll tick=%lu uptime_ms=%llu\n",
               static_cast<unsigned long>(poll_tick),
               static_cast<unsigned long long>(now_ms));

        if (!pollCommandsOnce())
        {
            printf("commands_poll retry in %lu ms\n", static_cast<unsigned long>(kPollFailureBackoffMs));
            vTaskDelay(pdMS_TO_TICKS(kPollFailureBackoffMs));
        }
        else
        {
            // Long-poll protocol: immediately issue next request after server closes connection.
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

bool ServerSyncClient::pollCommandsOnce()
{
    if (device_hw_id_.empty())
    {
        return false;
    }

    std::string response;
    const std::string poll_url = poll_url_base_ + device_hw_id_;
    printf("commands_poll request: %s\n", poll_url.c_str());
    const auto result = WebServiceApi::GetJson(poll_url, response, 30000);
    if (!WebServiceApi::IsSuccess(result))
    {
        printf("commands_poll failed: %s\n", WebServiceApi::ResultToString(result));
        return false;
    }

        const size_t preview_len = (response.size() > kMaxLoggedPayloadBytes) ? kMaxLoggedPayloadBytes : response.size();
        printf("commands_poll response bytes=%lu preview='%.*s'%s\n",
            static_cast<unsigned long>(response.size()),
            static_cast<int>(preview_len),
            response.c_str(),
            response.size() > preview_len ? "..." : "");

    if (response.empty())
    {
        printf("commands_poll: empty response (no commands)\n");
        return true;
    }

    return handlePollResponse(response);
}

bool ServerSyncClient::handlePollResponse(const std::string& json_text)
{
    const size_t preview_len = (json_text.size() > kMaxLoggedPayloadBytes) ? kMaxLoggedPayloadBytes : json_text.size();
    printf("commands_poll parsing bytes=%lu preview='%.*s'%s\n",
           static_cast<unsigned long>(json_text.size()),
           static_cast<int>(preview_len),
           json_text.c_str(),
           json_text.size() > preview_len ? "..." : "");
    cJSON* json = cJSON_Parse(json_text.c_str());
    if (json == nullptr)
    {
        printf("commands_poll parse failed: %s\n", json_text.c_str());
        return false;
    }

    if (cJSON_IsObject(json))
    {
        cJSON* status = cJSON_GetObjectItem(json, "status");
        if (cJSON_IsString(status) && status->valuestring != nullptr)
        {
            if (std::strcmp(status->valuestring, "no_commands") == 0)
            {
                cJSON_Delete(json);
                return true;
            }

            if (std::strcmp(status->valuestring, "success") == 0)
            {
                cJSON* commands = cJSON_GetObjectItem(json, "commands");
                if (cJSON_IsArray(commands))
                {
                    const int command_count = cJSON_GetArraySize(commands);
                    for (int i = 0; i < command_count; ++i)
                    {
                        cJSON* command = cJSON_GetArrayItem(commands, i);
                        if (!cJSON_IsObject(command))
                        {
                            continue;
                        }

                        handleCommandJson(command);
                    }
                }

                cJSON_Delete(json);
                return true;
            }
        }

        if (cJSON_GetObjectItem(json, "action") != nullptr)
        {
            handleCommandJson(json);
            cJSON_Delete(json);
            return true;
        }
    }

    cJSON_Delete(json);
    return false;
}

bool ServerSyncClient::mapValveIdToIndex(uint32_t raw_valve_id, uint32_t& out_index) const
{
    uint32_t valve_count = 0U;
    if (state_mutex_ != nullptr)
    {
        xSemaphoreTake(state_mutex_, portMAX_DELAY);
    }
    valve_count = static_cast<uint32_t>(valve_states_.size());
    if (state_mutex_ != nullptr)
    {
        xSemaphoreGive(state_mutex_);
    }

    if (valve_count == 0U)
    {
        return false;
    }

    if (raw_valve_id < valve_count)
    {
        out_index = raw_valve_id;
        return true;
    }

    return false;
}

void ServerSyncClient::applyServerValveState(cJSON* command, uint32_t server_time_unix)
{
    if (!cJSON_IsObject(command))
    {
        return;
    }

    cJSON* valve_id = cJSON_GetObjectItem(command, "id");
    cJSON* status = cJSON_GetObjectItem(command, "status");
    cJSON* run_until_unix = cJSON_GetObjectItem(command, "run_until_unix");
    if (!cJSON_IsNumber(valve_id) || !cJSON_IsString(status) || status->valuestring == nullptr)
    {
        return;
    }

    const uint32_t raw_id = static_cast<uint32_t>(valve_id->valuedouble);
    uint32_t mapped_index = 0U;
    if (!mapValveIdToIndex(raw_id, mapped_index))
    {
        return;
    }

    const uint32_t run_until = (cJSON_IsNumber(run_until_unix) && run_until_unix->valuedouble > 0)
                                 ? static_cast<uint32_t>(run_until_unix->valuedouble)
                                 : 0U;

    if (std::strcmp(status->valuestring, "Running") == 0)
    {
        if (server_time_unix == 0U)
        {
            printf("device_sync warning: server_time missing, skipping restore for valve %lu\n",
                   static_cast<unsigned long>(mapped_index));
            return;
        }

        if (run_until > server_time_unix)
        {
            const uint32_t duration_sec = run_until - server_time_unix;
            if (command_handlers_.manual_run != nullptr)
            {
                command_handlers_.manual_run(command_handlers_.context, mapped_index, duration_sec);
            }
        }
        else if (command_handlers_.stop_run != nullptr)
        {
            command_handlers_.stop_run(command_handlers_.context, mapped_index);
        }
        return;
    }

    if (std::strcmp(status->valuestring, "Idle") == 0)
    {
        if (command_handlers_.stop_run != nullptr)
        {
            command_handlers_.stop_run(command_handlers_.context, mapped_index);
        }
    }
}

void ServerSyncClient::handleCommandJson(cJSON* json)
{
    if (json == nullptr)
    {
        return;
    }

    cJSON* action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == nullptr)
    {
        return;
    }

    printf("command action received: %s\n", action->valuestring);

    if (std::strcmp(action->valuestring, "manual_run") == 0)
    {
        cJSON* valve_index = cJSON_GetObjectItem(json, "valve_id");
        if (!cJSON_IsNumber(valve_index))
        {
            valve_index = cJSON_GetObjectItem(json, "valve_index");
        }
        cJSON* duration = cJSON_GetObjectItem(json, "duration_sec");
        if (cJSON_IsNumber(valve_index) && cJSON_IsNumber(duration) && command_handlers_.manual_run != nullptr)
        {
            const uint32_t raw_id = static_cast<uint32_t>(valve_index->valuedouble);
            uint32_t mapped_index = 0U;
            if (!mapValveIdToIndex(raw_id, mapped_index))
            {
                printf("manual_run rejected: valve id/index %lu out of range (valves=%lu)\n",
                       static_cast<unsigned long>(raw_id),
                       static_cast<unsigned long>(valve_states_.size()));
                return;
            }

            printf("manual_run mapped valve id/index %lu -> index %lu\n",
                   static_cast<unsigned long>(raw_id),
                   static_cast<unsigned long>(mapped_index));
            command_handlers_.manual_run(
                command_handlers_.context,
                mapped_index,
                static_cast<uint32_t>(duration->valuedouble));
        }
    }
    else if (std::strcmp(action->valuestring, "stop_run") == 0)
    {
        cJSON* valve_index = cJSON_GetObjectItem(json, "valve_id");
        if (!cJSON_IsNumber(valve_index))
        {
            valve_index = cJSON_GetObjectItem(json, "valve_index");
        }
        if (cJSON_IsNumber(valve_index) && command_handlers_.stop_run != nullptr)
        {
            const uint32_t raw_id = static_cast<uint32_t>(valve_index->valuedouble);
            uint32_t mapped_index = 0U;
            if (!mapValveIdToIndex(raw_id, mapped_index))
            {
                printf("stop_run rejected: valve id/index %lu out of range (valves=%lu)\n",
                       static_cast<unsigned long>(raw_id),
                       static_cast<unsigned long>(valve_states_.size()));
                return;
            }

            printf("stop_run mapped valve id/index %lu -> index %lu\n",
                   static_cast<unsigned long>(raw_id),
                   static_cast<unsigned long>(mapped_index));
            command_handlers_.stop_run(
                command_handlers_.context,
                mapped_index);
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
        printf("factory_reset command accepted.\n");
        if (command_handlers_.factory_reset != nullptr)
        {
            command_handlers_.factory_reset(command_handlers_.context);
        }
    }
    else if (std::strcmp(action->valuestring, "update") == 0)
    {
        if (command_handlers_.update != nullptr)
        {
            cJSON* fw = cJSON_GetObjectItem(json, "firmware_url");
            const char* url_from_server = nullptr;
            if (cJSON_IsString(fw) && fw->valuestring != nullptr)
            {
                url_from_server = fw->valuestring;
            }
            command_handlers_.update(command_handlers_.context, url_from_server);
        }
    }
}

} // namespace app::cloud
