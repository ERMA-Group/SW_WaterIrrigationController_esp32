/**
 * @file global_credentials.cpp
 * @brief Application-layer global credentials implementation.
 */

#include "global_credentials.hpp"

#include <cstdio>

#include "esp_mac.h"
#include "esp_random.h"
#include "nvram.hpp"

namespace app {

bool GlobalCredentials::load_from_nvs()
{
    bsw::Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    cache_.device_id = nvs.get_string(kKeyDeviceId);
    cache_.device_password = nvs.get_string(kKeyDevicePassword);
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

bool GlobalCredentials::get_admin_credentials(bsw::AdminCredentials& out)
{
    Credentials credentials{};
    if (!get(credentials))
    {
        return false;
    }
    out.device_id = credentials.device_id;
    out.device_password = credentials.device_password;
    return true;
}

bool GlobalCredentials::set_admin_credentials(const bsw::AdminCredentials& in)
{
    Credentials out{};
    return update(in.device_id, true, in.device_password, true, false, out);
}

bool GlobalCredentials::get_admin_credentials_cb(void* context, bsw::AdminCredentials& out)
{
    if (context == nullptr)
    {
        return false;
    }

    auto* self = static_cast<GlobalCredentials*>(context);
    return self->get_admin_credentials(out);
}

bool GlobalCredentials::set_admin_credentials_cb(void* context, const bsw::AdminCredentials& in)
{
    if (context == nullptr)
    {
        return false;
    }

    auto* self = static_cast<GlobalCredentials*>(context);
    return self->set_admin_credentials(in);
}

} // namespace app
