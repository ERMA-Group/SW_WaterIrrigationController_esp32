#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include "mqtt_client.h"
}

namespace app::cloud {

class HomeAssistantMqttBridge {
public:
    using SetValveStateCb = void (*)(void* context, uint32_t valve_index, bool open);

    HomeAssistantMqttBridge() = default;
    ~HomeAssistantMqttBridge() = default;

    void configure(const std::string& hw_id, uint32_t valve_count);
    void setCommandCallback(void* context, SetValveStateCb callback);
    bool start(const char* broker_uri, const char* username, const char* password, const char* discovery_prefix);
    void publishValveState(uint32_t valve_index, bool is_open);

private:
    std::string hw_id_;
    uint32_t valve_count_ = 0;
    std::string discovery_prefix_;
    std::vector<std::string> broker_candidates_;
    size_t broker_candidate_index_ = 0;
    bool mqtt_connected_once_ = false;

    void* callback_context_ = nullptr;
    SetValveStateCb command_callback_ = nullptr;

    esp_mqtt_client_handle_t mqtt_client_ = nullptr;

    static void mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void onMqttConnected();
    void onMqttData(const char* topic, int topic_len, const char* data, int data_len);
    bool rotateBrokerAndReconnect();

    std::string getStateTopic(uint32_t valve_index) const;
    std::string getCommandTopic(uint32_t valve_index) const;
    std::string getDiscoveryTopic(uint32_t valve_index) const;
    std::string getBatteryStateTopic() const;
    std::string getBatteryDiscoveryTopic() const;
    std::string getGatewayIpBrokerUri() const;
    void discoverMqttServices();
};

} // namespace app::cloud
