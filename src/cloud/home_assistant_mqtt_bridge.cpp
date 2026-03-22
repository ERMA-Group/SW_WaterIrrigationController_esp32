#include "cloud/home_assistant_mqtt_bridge.hpp"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <vector>

extern "C" {
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
}

namespace app::cloud {

namespace {

bool isTcpPortOpen(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port, int timeout_ms)
{
    int sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        return false;
    }

    struct timeval tv = {};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    lwip_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    const uint32_t ip = (static_cast<uint32_t>(a) << 24) |
                        (static_cast<uint32_t>(b) << 16) |
                        (static_cast<uint32_t>(c) << 8) |
                        static_cast<uint32_t>(d);
    dest.sin_addr.s_addr = htonl(ip);

    const int ret = lwip_connect(sock, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
    lwip_close(sock);
    return ret == 0;
}

} // namespace

void HomeAssistantMqttBridge::configure(const std::string& hw_id, uint32_t valve_count)
{
    hw_id_ = hw_id;
    valve_count_ = valve_count;
}

void HomeAssistantMqttBridge::setCommandCallback(void* context, SetValveStateCb callback)
{
    callback_context_ = context;
    command_callback_ = callback;
}

bool HomeAssistantMqttBridge::start(const char* broker_uri,
                                    const char* username,
                                    const char* password,
                                    const char* discovery_prefix)
{
    if (hw_id_.empty() || valve_count_ == 0)
    {
        printf("HA MQTT not started: hw_id or valve_count not configured.\n");
        return false;
    }

    broker_candidates_.clear();
    broker_candidate_index_ = 0;
    mqtt_connected_once_ = false;

    printf("HA MQTT: Starting MQTT broker discovery...\n");

    auto add_candidate_unique = [this](const std::string& uri) {
        if (uri.empty())
        {
            return;
        }
        const auto it = std::find(broker_candidates_.begin(), broker_candidates_.end(), uri);
        if (it == broker_candidates_.end())
        {
            broker_candidates_.push_back(uri);
        }
    };

    // Use configured broker URI first (can be hostname or IP).
    if (broker_uri != nullptr && broker_uri[0] != '\0')
    {
        std::string explicit_uri = broker_uri;
        explicit_uri.erase(std::remove_if(explicit_uri.begin(), explicit_uri.end(), [](unsigned char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }), explicit_uri.end());
        add_candidate_unique(explicit_uri);
    }

    // Build IP-based candidates from active network configuration.
    discoverMqttServices();

    // If subnet discovery didn't find anything, try direct gateway getter.
    if (broker_candidates_.empty())
    {
        printf("HA MQTT: No subnet candidates found, trying direct gateway lookup...\n");
        std::string gateway_broker = getGatewayIpBrokerUri();
        if (!gateway_broker.empty())
        {
            add_candidate_unique(gateway_broker);
            printf("HA MQTT: Added gateway IP: %s\n", gateway_broker.c_str());
        }
    }

    // Last-resort defaults when DHCP data is incomplete.
    if (broker_candidates_.empty())
    {
        printf("HA MQTT: Using generic gateway fallbacks\n");
        add_candidate_unique("mqtt://192.168.1.1:1883");
        add_candidate_unique("mqtt://192.168.0.1:1883");
        add_candidate_unique("mqtt://10.0.0.1:1883");
    }

    if (broker_candidates_.empty())
    {
        printf("HA MQTT: No broker candidates available!\n");
        return false;
    }

    discovery_prefix_ = (discovery_prefix != nullptr && discovery_prefix[0] != '\0')
                            ? discovery_prefix
                            : "homeassistant";

    // Log all broker candidates
    printf("HA MQTT broker candidates (total: %zu):\n", broker_candidates_.size());
    for (size_t i = 0; i < broker_candidates_.size(); ++i)
    {
        printf("  [%zu] %s\n", i, broker_candidates_[i].c_str());
    }

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = broker_candidates_[broker_candidate_index_].c_str();
    cfg.credentials.client_id = hw_id_.c_str();
    cfg.credentials.username = (username != nullptr && username[0] != '\0') ? username : nullptr;
    cfg.credentials.authentication.password = (password != nullptr && password[0] != '\0') ? password : nullptr;

    if (cfg.credentials.username == nullptr)
    {
        printf("HA MQTT: Anonymous mode (no credentials)\n");
    }

    mqtt_client_ = esp_mqtt_client_init(&cfg);
    if (mqtt_client_ == nullptr)
    {
        printf("HA MQTT init failed.\n");
        return false;
    }

    esp_mqtt_client_register_event(mqtt_client_, MQTT_EVENT_ANY, &HomeAssistantMqttBridge::mqttEventHandler, this);
    esp_err_t start_err = esp_mqtt_client_start(mqtt_client_);
    if (start_err != ESP_OK)
    {
        printf("HA MQTT start failed: %s\n", esp_err_to_name(start_err));
        return false;
    }

    printf("HA MQTT starting with: %s\n", broker_candidates_[broker_candidate_index_].c_str());
    return true;
}

void HomeAssistantMqttBridge::publishValveState(uint32_t valve_index, bool is_open)
{
    if (mqtt_client_ == nullptr || valve_index >= valve_count_)
    {
        return;
    }

    const std::string topic = getStateTopic(valve_index);
    const char* payload = is_open ? "ON" : "OFF";
    esp_mqtt_client_publish(mqtt_client_, topic.c_str(), payload, 0, 1, 1);
}

void HomeAssistantMqttBridge::mqttEventHandler(void* handler_args,
                                               esp_event_base_t base,
                                               int32_t event_id,
                                               void* event_data)
{
    (void)base;
    auto* self = static_cast<HomeAssistantMqttBridge*>(handler_args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);
    if (self == nullptr || event == nullptr)
    {
        return;
    }

    switch (event_id)
    {
        case MQTT_EVENT_CONNECTED:
            self->mqtt_connected_once_ = true;
            self->onMqttConnected();
            break;
        case MQTT_EVENT_DISCONNECTED:
            printf("HA MQTT disconnected.\n");
            break;
        case MQTT_EVENT_ERROR:
            if (event->error_handle != nullptr)
            {
                const int connect_ret = static_cast<int>(event->error_handle->connect_return_code);
                printf("HA MQTT error: type=%d, connect_return_code=%d.\n",
                       static_cast<int>(event->error_handle->error_type),
                       connect_ret);

                if (connect_ret == 5)
                {
                    printf("HA MQTT auth rejected (code 5). Configure kMqttUsername/kMqttPassword in src/asw_cfg/home_assistant_cfg.hpp.\n");
                }

                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
                {
                    self->rotateBrokerAndReconnect();
                }
            }
            break;
        case MQTT_EVENT_DATA:
            self->onMqttData(event->topic, event->topic_len, event->data, event->data_len);
            break;
        default:
            break;
    }
}

bool HomeAssistantMqttBridge::rotateBrokerAndReconnect()
{
    if (mqtt_client_ == nullptr || broker_candidates_.size() <= 1)
    {
        return false;
    }

    const size_t next_index = (broker_candidate_index_ + 1) % broker_candidates_.size();
    if (next_index == broker_candidate_index_)
    {
        return false;
    }

    broker_candidate_index_ = next_index;
    const std::string& next_uri = broker_candidates_[broker_candidate_index_];

    printf("HA MQTT retry with broker: %s\n", next_uri.c_str());
    esp_mqtt_client_set_uri(mqtt_client_, next_uri.c_str());
    const esp_err_t reconnect_err = esp_mqtt_client_reconnect(mqtt_client_);
    if (reconnect_err != ESP_OK)
    {
        printf("HA MQTT reconnect failed: %s\n", esp_err_to_name(reconnect_err));
        return false;
    }

    return true;
}

void HomeAssistantMqttBridge::onMqttConnected()
{
    printf("HA MQTT connected to %s, publishing discovery for %lu valves.\n",
           broker_candidates_[broker_candidate_index_].c_str(),
           static_cast<unsigned long>(valve_count_));

    const std::string battery_discovery_topic = getBatteryDiscoveryTopic();
    const std::string battery_state_topic = getBatteryStateTopic();

    char battery_payload[768] = {0};
    std::snprintf(battery_payload,
                  sizeof(battery_payload),
                  "{\"name\":\"Battery Level\"," 
                  "\"unique_id\":\"%s_battery\"," 
                  "\"state_topic\":\"%s\"," 
                  "\"device_class\":\"battery\"," 
                  "\"unit_of_measurement\":\"%%\"," 
                  "\"device\":{" 
                      "\"identifiers\":[\"%s\"]," 
                      "\"name\":\"%s\"," 
                      "\"manufacturer\":\"DIY\"," 
                      "\"model\":\"Battery ESP32\""
                  "}}",
                  hw_id_.c_str(),
                  battery_state_topic.c_str(),
                  hw_id_.c_str(),
                  hw_id_.c_str());

    const int batt_disc_msg_id = esp_mqtt_client_publish(mqtt_client_, battery_discovery_topic.c_str(), battery_payload, 0, 1, 1);
    printf("HA MQTT battery discovery msg_id=%d topic=%s\n", batt_disc_msg_id, battery_discovery_topic.c_str());

    for (uint32_t i = 0; i < valve_count_; ++i)
    {
        const std::string discovery_topic = getDiscoveryTopic(i);
        const std::string command_topic = getCommandTopic(i);
        const std::string state_topic = getStateTopic(i);

        char payload[1024] = {0};
        std::snprintf(payload,
                      sizeof(payload),
                      "{\"name\":\"%s Valve %lu\","
                      "\"object_id\":\"%s_valve_%lu\","
                      "\"unique_id\":\"%s_valve_%lu\","
                      "\"command_topic\":\"%s\","
                      "\"state_topic\":\"%s\","
                      "\"retain\":true,"
                      "\"icon\":\"mdi:water-pump\","
                      "\"payload_on\":\"ON\","
                      "\"payload_off\":\"OFF\","
                      "\"state_on\":\"ON\","
                      "\"state_off\":\"OFF\","
                      "\"device\":{"
                          "\"identifiers\":[\"%s\"],"
                          "\"name\":\"%s\","
                          "\"manufacturer\":\"DIY\"," 
                          "\"model\":\"Battery ESP32\""
                      "}}",
                      hw_id_.c_str(),
                      static_cast<unsigned long>(i),
                      hw_id_.c_str(),
                      static_cast<unsigned long>(i),
                      hw_id_.c_str(),
                      static_cast<unsigned long>(i),
                      command_topic.c_str(),
                      state_topic.c_str(),
                      hw_id_.c_str(),
                      hw_id_.c_str());

        const int disc_msg_id = esp_mqtt_client_publish(mqtt_client_, discovery_topic.c_str(), payload, 0, 1, 1);
        const int sub_msg_id = esp_mqtt_client_subscribe(mqtt_client_, command_topic.c_str(), 1);
        printf("HA MQTT discovery msg_id=%d topic=%s\n", disc_msg_id, discovery_topic.c_str());
        printf("HA MQTT subscribe msg_id=%d topic=%s\n", sub_msg_id, command_topic.c_str());
    }
}

void HomeAssistantMqttBridge::onMqttData(const char* topic, int topic_len, const char* data, int data_len)
{
    if (topic == nullptr || data == nullptr || topic_len <= 0 || data_len <= 0)
    {
        return;
    }

    const std::string topic_str(topic, static_cast<size_t>(topic_len));
    const std::string data_str(data, static_cast<size_t>(data_len));

    const std::string topic_prefix = "erma/" + hw_id_ + "/valve/";
    const std::string topic_suffix = "/set";

    if (topic_str.rfind(topic_prefix, 0) != 0 ||
        topic_str.size() <= topic_prefix.size() + topic_suffix.size() ||
        topic_str.substr(topic_str.size() - topic_suffix.size()) != topic_suffix)
    {
        return;
    }

    const std::string index_text = topic_str.substr(topic_prefix.size(),
                                                    topic_str.size() - topic_prefix.size() - topic_suffix.size());
    uint32_t valve_index = static_cast<uint32_t>(std::strtoul(index_text.c_str(), nullptr, 10));
    if (valve_index >= valve_count_)
    {
        return;
    }

    bool open = false;
    if (data_str == "ON" || data_str == "on" || data_str == "1")
    {
        open = true;
    }
    else if (data_str == "OFF" || data_str == "off" || data_str == "0")
    {
        open = false;
    }
    else
    {
        return;
    }

    if (command_callback_ != nullptr)
    {
        command_callback_(callback_context_, valve_index, open);
    }
}

std::string HomeAssistantMqttBridge::getStateTopic(uint32_t valve_index) const
{
    return "erma/" + hw_id_ + "/valve/" + std::to_string(valve_index) + "/state";
}

std::string HomeAssistantMqttBridge::getCommandTopic(uint32_t valve_index) const
{
    return "erma/" + hw_id_ + "/valve/" + std::to_string(valve_index) + "/set";
}

std::string HomeAssistantMqttBridge::getDiscoveryTopic(uint32_t valve_index) const
{
    return discovery_prefix_ + "/switch/" + hw_id_ + "/valve_" + std::to_string(valve_index) + "/config";
}

std::string HomeAssistantMqttBridge::getBatteryStateTopic() const
{
    return "erma/" + hw_id_ + "/battery/state";
}

std::string HomeAssistantMqttBridge::getBatteryDiscoveryTopic() const
{
    return discovery_prefix_ + "/sensor/" + hw_id_ + "/battery/config";
}

std::string HomeAssistantMqttBridge::getGatewayIpBrokerUri() const
{
    // Get gateway IP from WiFi STA interface (most common broker location in home setups).
    esp_netif_t* wifi_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_sta == nullptr)
    {
        printf("HA MQTT: WiFi STA interface not found\n");
        return "";
    }

    esp_netif_ip_info_t ip_info = {};
    esp_err_t ret = esp_netif_get_ip_info(wifi_sta, &ip_info);
    if (ret != ESP_OK)
    {
        printf("HA MQTT: Failed to get IP info (ret=%d)\n", ret);
        return "";
    }

    printf("HA MQTT: WiFi IP info - IP: %08lx, Netmask: %08lx, Gateway: %08lx\n", 
           ip_info.ip.addr, ip_info.netmask.addr, ip_info.gw.addr);

    if (ip_info.gw.addr == 0)
    {
        printf("HA MQTT: Gateway IP is 0 (WiFi not connected or no gateway info)\n");
        return "";
    }

    char gw_str[24] = {0};
    const ip4_addr_t* gw = reinterpret_cast<const ip4_addr_t*>(&ip_info.gw);
    std::snprintf(gw_str,
                  sizeof(gw_str),
                  "mqtt://%u.%u.%u.%u:1883",
                  static_cast<unsigned int>(ip4_addr1(gw)),
                  static_cast<unsigned int>(ip4_addr2(gw)),
                  static_cast<unsigned int>(ip4_addr3(gw)),
                  static_cast<unsigned int>(ip4_addr4(gw)));
    
    printf("HA MQTT: Discovered gateway IP broker: %s\n", gw_str);
    return std::string(gw_str);
}

void HomeAssistantMqttBridge::discoverMqttServices()
{
    // Discover MQTT broker candidates from DHCP data and local subnet heuristics.
    printf("HA MQTT: Discovering brokers on local subnet...\n");

    esp_netif_t* wifi_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_sta == nullptr)
    {
        printf("HA MQTT: WiFi interface not available\n");
        return;
    }

    esp_netif_ip_info_t ip_info = {};
    esp_err_t ret = ESP_FAIL;

    // Wait for DHCP/IP info so discovery can use the real subnet instead of generic fallbacks.
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        ret = esp_netif_get_ip_info(wifi_sta, &ip_info);
        if (ret == ESP_OK && ip_info.ip.addr != 0)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (ret != ESP_OK || ip_info.ip.addr == 0)
    {
        printf("HA MQTT: WiFi not connected or no IP after wait\n");
        return;
    }

    auto add_candidate_unique = [this](const std::string& uri) {
        if (uri.empty())
        {
            return;
        }
        const auto it = std::find(broker_candidates_.begin(), broker_candidates_.end(), uri);
        if (it == broker_candidates_.end())
        {
            broker_candidates_.push_back(uri);
        }
    };

    const ip4_addr_t* ip = reinterpret_cast<const ip4_addr_t*>(&ip_info.ip);
    const ip4_addr_t* gw = reinterpret_cast<const ip4_addr_t*>(&ip_info.gw);

    const uint8_t b0 = static_cast<uint8_t>(ip4_addr1(ip));
    const uint8_t b1 = static_cast<uint8_t>(ip4_addr2(ip));
    const uint8_t b2 = static_cast<uint8_t>(ip4_addr3(ip));

    printf("HA MQTT: Device on subnet %u.%u.%u.0/24\n", b0, b1, b2);

    // Try DHCP gateway first.
    if (ip_info.gw.addr != 0)
    {
        char gw_str[24] = {0};
        std::snprintf(gw_str,
                      sizeof(gw_str),
                      "mqtt://%u.%u.%u.%u:1883",
                      static_cast<unsigned int>(ip4_addr1(gw)),
                      static_cast<unsigned int>(ip4_addr2(gw)),
                      static_cast<unsigned int>(ip4_addr3(gw)),
                      static_cast<unsigned int>(ip4_addr4(gw)));
        add_candidate_unique(std::string(gw_str));
        printf("HA MQTT: Added gateway: %s\n", gw_str);
    }

    // Probe full /24 host range for a live MQTT listener on port 1883.
    std::vector<uint8_t> host_candidates;
    host_candidates.reserve(254);

    if (ip_info.gw.addr != 0)
    {
        host_candidates.push_back(static_cast<uint8_t>(ip4_addr4(gw)));
    }

    for (uint16_t h = 1; h <= 254; ++h)
    {
        const uint8_t host = static_cast<uint8_t>(h);
        if (std::find(host_candidates.begin(), host_candidates.end(), h) == host_candidates.end())
        {
            host_candidates.push_back(host);
        }
    }

    char candidate[24] = {0};
    uint32_t probed_hosts = 0;
    uint32_t open_hosts = 0;
    for (uint8_t host : host_candidates)
    {
        if (host == static_cast<uint8_t>(ip4_addr4(ip)))
        {
            continue;
        }

        ++probed_hosts;

        if (!isTcpPortOpen(b0, b1, b2, host, 1883, 120))
        {
            continue;
        }

        ++open_hosts;

        std::snprintf(candidate,
                      sizeof(candidate),
                      "mqtt://%u.%u.%u.%u:1883",
                      static_cast<unsigned int>(b0),
                      static_cast<unsigned int>(b1),
                      static_cast<unsigned int>(b2),
                      static_cast<unsigned int>(host));
        add_candidate_unique(std::string(candidate));
        printf("HA MQTT: Found MQTT listener: %s\n", candidate);
    }

    printf("HA MQTT: Probed %lu hosts, found %lu open MQTT listeners\n",
           static_cast<unsigned long>(probed_hosts),
           static_cast<unsigned long>(open_hosts));

    // Keep at least gateway and .1 candidates if active probing found none.
    if (broker_candidates_.empty())
    {
        std::snprintf(candidate,
                      sizeof(candidate),
                      "mqtt://%u.%u.%u.%u:1883",
                      static_cast<unsigned int>(b0),
                      static_cast<unsigned int>(b1),
                      static_cast<unsigned int>(b2),
                      1U);
        add_candidate_unique(std::string(candidate));

        if (ip_info.gw.addr != 0)
        {
            char gw_str[24] = {0};
            std::snprintf(gw_str,
                          sizeof(gw_str),
                          "mqtt://%u.%u.%u.%u:1883",
                          static_cast<unsigned int>(ip4_addr1(gw)),
                          static_cast<unsigned int>(ip4_addr2(gw)),
                          static_cast<unsigned int>(ip4_addr3(gw)),
                          static_cast<unsigned int>(ip4_addr4(gw)));
            add_candidate_unique(std::string(gw_str));
        }
    }

    printf("HA MQTT: Discovery added %zu candidates\n", broker_candidates_.size());
}

} // namespace app::cloud
