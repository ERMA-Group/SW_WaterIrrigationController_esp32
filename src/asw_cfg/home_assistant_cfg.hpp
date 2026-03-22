#pragma once

namespace app::ha_cfg {

// Default broker URI for zero-config MQTT mode in local HA networks.
// This expects the broker to be reachable as homeassistant.local and allow current credentials policy.
constexpr const char* kMqttBrokerUri = "mqtt://homeassistant.local:1883";
constexpr const char* kMqttUsername = "";
constexpr const char* kMqttPassword = "";
constexpr const char* kDiscoveryPrefix = "homeassistant";

} // namespace app::ha_cfg
