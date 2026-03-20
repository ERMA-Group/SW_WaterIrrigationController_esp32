/**
 * @file global_credentials.hpp
 * @brief Application-layer global credentials storage.
 */

#pragma once

#include <string>

namespace app {

enum class DeviceType
{
    Wired,
    Wireless
};

class GlobalCredentials final {
public:
    struct Credentials {
        std::string device_id;
        std::string device_password;
        std::string pairing_pin;
        DeviceType device_type;
        uint32_t valve_count = 8;
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

    bool consume_pairing_pin(std::string& out_pin);
    bool get_pairing_pin(std::string& out_pin);
    bool set_pairing_pin(const std::string& pin);
    bool set_device_type(DeviceType device_type);
    DeviceType get_device_type() const;
    bool set_valve_count(uint32_t valve_count);
    uint32_t get_valve_count() const;


    static bool get_pairing_pin_cb(void* context, std::string& out_pin);
    static bool set_pairing_pin_cb(void* context, const std::string& pin);

private:
    static constexpr const char* kNs = "global_cfg";
    static constexpr const char* kKeyDeviceId = "device_id";
    static constexpr const char* kKeyDevicePassword = "device_password";
    static constexpr const char* kKeyPairingPin = "pairing_pin";
    static constexpr const char* kKeyDeviceType = "device_type";
    static constexpr const char* kKeyValveCount = "valve_count";

    Credentials cache_{};

    bool ensure_loaded();
    bool load_from_nvs();
    bool save_to_nvs();

    std::string generate_device_id() const;
    std::string generate_device_password() const;
};

} // namespace app
