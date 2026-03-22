#include "asw_cfg/mqtt_runtime_config.hpp"

#include "nvram.hpp"

namespace app::mqtt_cfg {

namespace {

constexpr const char* kNs = "mqtt_cfg";
constexpr const char* kKeyUri = "broker_uri";
constexpr const char* kKeyUser = "username";
constexpr const char* kKeyPass = "password";

} // namespace

bool load(BrokerConfig& out)
{
    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    out.broker_uri = nvs.get_string(kKeyUri);
    out.username = nvs.get_string(kKeyUser);
    out.password = nvs.get_string(kKeyPass);
    nvs.close();
    return true;
}

bool save(const BrokerConfig& in)
{
    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    bool ok = nvs.set_string(kKeyUri, in.broker_uri) == ESP_OK;
    ok = ok && (nvs.set_string(kKeyUser, in.username) == ESP_OK);
    ok = ok && (nvs.set_string(kKeyPass, in.password) == ESP_OK);
    nvs.close();
    return ok;
}

} // namespace app::mqtt_cfg
