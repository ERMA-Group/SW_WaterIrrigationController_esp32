#pragma once

#include <string>

namespace app::mqtt_cfg {

struct BrokerConfig {
    std::string broker_uri;
    std::string username;
    std::string password;
};

bool load(BrokerConfig& out);
bool save(const BrokerConfig& in);

} // namespace app::mqtt_cfg
