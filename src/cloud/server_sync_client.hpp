#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

struct cJSON;

namespace app::cloud {

struct CommandHandlers {
    void* context = nullptr;
    void (*manual_run)(void* context, uint32_t valve_index, uint32_t duration_sec) = nullptr;
    void (*stop_run)(void* context, uint32_t valve_index) = nullptr;
    void (*restart)(void* context) = nullptr;
    void (*factory_reset)(void* context) = nullptr;
    void (*update)(void* context, const char* firmware_url) = nullptr;
    void (*sync_ok)(void* context) = nullptr;
};

class ServerSyncClient {
public:
    ServerSyncClient();
    ~ServerSyncClient();

    void configure(const char* sync_url, const char* poll_url_base, uint32_t sync_period_ms);
    void setCommandHandlers(const CommandHandlers& handlers);
    void setDeviceHwId(const std::string& hw_id);
    void setFirmwareVersion(const std::string& fw_version);
    void setValveConnectionType(const std::string& connection_type);
    void setValveCount(uint32_t valve_count);
    void setValveState(uint32_t valve_index, bool is_open);
    void start();

private:
    static constexpr uint32_t kDefaultSyncPeriodMs {5000};
    static constexpr uint32_t kPollFailureBackoffMs {3000};
    static constexpr size_t kMaxLoggedPayloadBytes {256};
    static const char* kDefaultSyncUrl;
    static const char* kDefaultPollUrlBase;

    std::string sync_url_ {kDefaultSyncUrl};
    std::string poll_url_base_ {kDefaultPollUrlBase};
    uint32_t sync_period_ms_ {kDefaultSyncPeriodMs};

    std::string device_hw_id_;
    std::string firmware_version_ {"0.0.0"};
    std::string valve_connection_type_ {"Wired"};
    std::vector<bool> valve_states_ {false};
    CommandHandlers command_handlers_;
    SemaphoreHandle_t state_mutex_ = nullptr;

    TaskHandle_t sync_task_handle_ = nullptr;
    TaskHandle_t poll_task_handle_ = nullptr;

    static void syncTaskEntry(void* arg);
    static void pollTaskEntry(void* arg);
    void syncTaskLoop();
    void pollTaskLoop();

    bool postDeviceSync();
    bool pollCommandsOnce();
    bool handlePollResponse(const std::string& json_text);
    void handleCommandJson(cJSON* json);
};

} // namespace app::cloud
