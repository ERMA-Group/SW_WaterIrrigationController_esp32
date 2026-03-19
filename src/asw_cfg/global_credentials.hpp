/**
 * @file global_credentials.hpp
 * @brief Application-layer global credentials storage.
 */

#pragma once

#include <string>

#include "wifi.hpp"

namespace app {

class GlobalCredentials final {
public:
    struct Credentials {
        std::string device_id;
        std::string device_password;
    };

    GlobalCredentials() = default;
    ~GlobalCredentials() = default;

    bool get(Credentials& out);
    bool update(const std::string& new_device_id,
                bool update_device_id,
                const std::string& new_device_password,
                bool update_device_password,
                bool reset_device_password,
                Credentials& out);

    bool get_admin_credentials(bsw::AdminCredentials& out);
    bool set_admin_credentials(const bsw::AdminCredentials& in);

    static bool get_admin_credentials_cb(void* context, bsw::AdminCredentials& out);
    static bool set_admin_credentials_cb(void* context, const bsw::AdminCredentials& in);

private:
    static constexpr const char* kNs = "global_cfg";
    static constexpr const char* kKeyDeviceId = "device_id";
    static constexpr const char* kKeyDevicePassword = "device_password";

    Credentials cache_{};

    bool ensure_loaded();
    bool load_from_nvs();
    bool save_to_nvs();

    std::string generate_device_id() const;
    std::string generate_device_password() const;
};

} // namespace app
