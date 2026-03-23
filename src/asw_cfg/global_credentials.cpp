/**
 * @file global_credentials.cpp
 * @brief Application-layer global credentials implementation.
 */

#include "global_credentials.hpp"

#include <cstdio>

#include "esp_mac.h"
#include "esp_random.h"
#include "nvram.hpp"
#include "bsw_cfg.hpp"

namespace app {

namespace {

constexpr uint32_t kDefaultSyncPeriodMs = 60000U;
constexpr uint32_t kMinSyncPeriodMs = 30000U;
constexpr uint32_t kMaxSyncPeriodMs = 60000U;

uint32_t clamp_sync_period_ms(uint32_t value)
{
    if (value < kMinSyncPeriodMs)
    {
        return kMinSyncPeriodMs;
    }
    if (value > kMaxSyncPeriodMs)
    {
        return kMaxSyncPeriodMs;
    }
    return value;
}

DeviceType parse_device_type_u8(uint8_t value)
{
    return (value == 1U) ? DeviceType::Wireless : DeviceType::Wired;
}

uint8_t device_type_to_u8(DeviceType value)
{
    return (value == DeviceType::Wireless) ? 1U : 0U;
}

OperatingMode parse_operating_mode_u8(uint8_t value)
{
    return (value == static_cast<uint8_t>(OperatingMode::PureMqtt)) ? OperatingMode::PureMqtt : OperatingMode::Standard;
}

uint8_t operating_mode_to_u8(OperatingMode value)
{
    return static_cast<uint8_t>(value);
}

} // namespace

bool GlobalCredentials::load_from_nvs()
{
    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    cache_.device_id = nvs.get_string(kKeyDeviceId);
    cache_.device_password = nvs.get_string(kKeyDevicePassword);
    cache_.pairing_pin = nvs.get_string(kKeyPairingPin);
    cache_.device_type = parse_device_type_u8(nvs.get_value<uint8_t>(kKeyDeviceType, 0U));
    cache_.valve_count = nvs.get_value<uint32_t>(kKeyValveCount, app::sld_cfg::kNumberOfWiredOutputs);
    cache_.operating_mode = parse_operating_mode_u8(nvs.get_value<uint8_t>(kKeyOperatingMode, static_cast<uint8_t>(OperatingMode::Standard)));
    cache_.local_mode_boot = nvs.get_value<uint8_t>(kKeyLocalModeBoot, 0U) != 0U;
    cache_.sync_period_ms = nvs.get_value<uint32_t>(kKeySyncPeriodMs, kDefaultSyncPeriodMs);
    nvs.close();
    return true;
}

bool GlobalCredentials::save_to_nvs()
{
    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    bool ok = nvs.set_string(kKeyDeviceId, cache_.device_id) == ESP_OK;
    ok = ok && (nvs.set_string(kKeyDevicePassword, cache_.device_password) == ESP_OK);
    ok = ok && (nvs.set_string(kKeyPairingPin, cache_.pairing_pin) == ESP_OK);
    ok = ok && (nvs.set_value<uint8_t>(kKeyDeviceType, device_type_to_u8(cache_.device_type)) == ESP_OK);
    ok = ok && (nvs.set_value<uint32_t>(kKeyValveCount, cache_.valve_count) == ESP_OK);
    ok = ok && (nvs.set_value<uint8_t>(kKeyOperatingMode, operating_mode_to_u8(cache_.operating_mode)) == ESP_OK);
    ok = ok && (nvs.set_value<uint8_t>(kKeyLocalModeBoot, cache_.local_mode_boot ? 1U : 0U) == ESP_OK);
    ok = ok && (nvs.set_value<uint32_t>(kKeySyncPeriodMs, cache_.sync_period_ms) == ESP_OK);
    nvs.close();
    return ok;
}

std::string GlobalCredentials::generate_device_id() const
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char id_buf[20] = {0};
    std::snprintf(id_buf,
                  sizeof(id_buf),
                  "WIC-%02X%02X%02X",
                  mac[3],
                  mac[4],
                  mac[5]);
    return std::string(id_buf);
}

std::string GlobalCredentials::generate_device_password() const
{
    static constexpr char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    constexpr size_t kCharsetLen = sizeof(charset) - 1;

    char pw[13] = {0};
    for (size_t i = 0; i < 12; ++i)
    {
        pw[i] = charset[esp_random() % kCharsetLen];
    }
    return std::string(pw);
}

bool GlobalCredentials::ensure_loaded()
{
    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    if (!load_from_nvs())
    {
        return false;
    }

    bool changed = false;
    if (cache_.device_id.empty())
    {
        cache_.device_id = generate_device_id();
        changed = true;
    }
    if (cache_.device_password.size() < 8)
    {
        cache_.device_password = generate_device_password();
        changed = true;
    }
    if (cache_.valve_count == 0U)
    {
        cache_.valve_count = app::sld_cfg::kNumberOfWiredOutputs;
        changed = true;
    }
    const uint32_t clamped_sync_period_ms = clamp_sync_period_ms(cache_.sync_period_ms);
    if (clamped_sync_period_ms != cache_.sync_period_ms)
    {
        cache_.sync_period_ms = clamped_sync_period_ms;
        changed = true;
    }

    if (changed)
    {
        return save_to_nvs();
    }
    return true;
}

bool GlobalCredentials::get(Credentials& out)
{
    if (!ensure_loaded())
    {
        return false;
    }
    out = cache_;
    return true;
}

bool GlobalCredentials::update(const std::string& new_device_id,
                               bool update_device_id,
                               const std::string& new_device_password,
                               bool update_device_password,
                               bool reset_device_password,
                               Credentials& out)
{
    if (!ensure_loaded())
    {
        return false;
    }

    if (update_device_id)
    {
        cache_.device_id = new_device_id;
    }
    if (update_device_password)
    {
        cache_.device_password = new_device_password;
    }
    if (reset_device_password)
    {
        cache_.device_password = generate_device_password();
    }

    if (!save_to_nvs())
    {
        return false;
    }

    out = cache_;
    return true;
}

bool GlobalCredentials::get_pairing_pin(std::string& out_pin)
{
    out_pin.clear();
    if (!ensure_loaded())
    {
        return false;
    }

    out_pin = cache_.pairing_pin;
    return true;
}

bool GlobalCredentials::set_pairing_pin(const std::string& pin)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.pairing_pin = pin;
    return save_to_nvs();
}

bool GlobalCredentials::set_device_type(DeviceType device_type)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.device_type = device_type;
    return save_to_nvs();
}

bool GlobalCredentials::set_valve_count(uint32_t valve_count)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.valve_count = (valve_count == 0U) ? 8U : valve_count;
    return save_to_nvs();
}

bool GlobalCredentials::consume_pairing_pin(std::string& out_pin)
{
    out_pin.clear();
    if (!ensure_loaded())
    {
        return false;
    }

    if (cache_.pairing_pin.empty())
    {
        return false;
    }

    out_pin = cache_.pairing_pin;
    cache_.pairing_pin.clear();
    return save_to_nvs();
}

bool GlobalCredentials::get_pairing_pin_cb(void* context, std::string& out_pin)
{
    if (context == nullptr)
    {
        return false;
    }

    auto* self = static_cast<GlobalCredentials*>(context);
    return self->get_pairing_pin(out_pin);
}

bool GlobalCredentials::set_pairing_pin_cb(void* context, const std::string& pin)
{
    if (context == nullptr)
    {
        return false;
    }

    auto* self = static_cast<GlobalCredentials*>(context);
    return self->set_pairing_pin(pin);
}

DeviceType GlobalCredentials::get_device_type() const
{
    return cache_.device_type;
}

uint32_t GlobalCredentials::get_valve_count() const
{
    return cache_.valve_count;
}

bool GlobalCredentials::set_operating_mode(OperatingMode mode)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.operating_mode = mode;
    return save_to_nvs();
}

OperatingMode GlobalCredentials::get_operating_mode() const
{
    return cache_.operating_mode;
}

bool GlobalCredentials::set_local_mode_boot(bool enabled)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.local_mode_boot = enabled;
    return save_to_nvs();
}

bool GlobalCredentials::get_local_mode_boot() const
{
    return cache_.local_mode_boot;
}

bool GlobalCredentials::set_sync_period_ms(uint32_t sync_period_ms)
{
    if (!ensure_loaded())
    {
        return false;
    }

    cache_.sync_period_ms = clamp_sync_period_ms(sync_period_ms);
    return save_to_nvs();
}

uint32_t GlobalCredentials::get_sync_period_ms() const
{
    return cache_.sync_period_ms;
}


} // namespace app
