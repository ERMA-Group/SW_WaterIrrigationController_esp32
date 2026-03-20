#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

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
    ServerSyncClient() = default;
    ~ServerSyncClient() = default;

    void configure(const char* sync_url, const char* sse_url_base, uint32_t sync_period_ms);
    void setCommandHandlers(const CommandHandlers& handlers);
    void setDeviceHwId(const std::string& hw_id);
    void setFirmwareVersion(const std::string& fw_version);
    void setValveCount(uint32_t valve_count);
    void setValveState(uint32_t valve_index, bool is_open);
    void start();

private:
    static constexpr uint32_t kDefaultSyncPeriodMs {60000};
    static const char* kDefaultSyncUrl;
    static const char* kDefaultSseUrlBase;

    std::string sync_url_ {kDefaultSyncUrl};
    std::string sse_url_base_ {kDefaultSseUrlBase};
    uint32_t sync_period_ms_ {kDefaultSyncPeriodMs};

    std::string device_hw_id_;
    std::string firmware_version_ {"0.0.0"};
    std::vector<bool> valve_states_ {false};
    CommandHandlers command_handlers_;

    TaskHandle_t sync_task_handle_ = nullptr;
    TaskHandle_t sse_task_handle_ = nullptr;

    static void syncTaskEntry(void* arg);
    static void sseTaskEntry(void* arg);
    void syncTaskLoop();
    void sseTaskLoop();

    bool postDeviceSync();
    void processSseLine(const std::string& line, bool& is_command_event);
    void handleSseCommandJson(const std::string& json_text);
};

} // namespace app::cloud
