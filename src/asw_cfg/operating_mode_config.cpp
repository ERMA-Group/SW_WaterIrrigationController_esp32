#include "asw_cfg/operating_mode_config.hpp"

#include "nvram.hpp"

namespace app::mode_cfg {

namespace {

constexpr const char* kNs = "app_mode_cfg";
constexpr const char* kKeyMode = "mode";

} // namespace

bool load(OperatingMode& out)
{
    out = OperatingMode::Standard;

    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    const uint8_t raw = nvs.get_value<uint8_t>(kKeyMode, static_cast<uint8_t>(OperatingMode::Standard));
    nvs.close();

    out = (raw == static_cast<uint8_t>(OperatingMode::PureMqtt))
              ? OperatingMode::PureMqtt
              : OperatingMode::Standard;
    return true;
}

bool save(OperatingMode mode)
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

    const bool ok = nvs.set_value<uint8_t>(kKeyMode, static_cast<uint8_t>(mode)) == ESP_OK;
    nvs.close();
    return ok;
}

} // namespace app::mode_cfg
