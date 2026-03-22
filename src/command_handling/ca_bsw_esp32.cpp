/**
 * @file ca_bsw_esp32.cpp
 * @brief C++ class implementation
 */

#include "ca_bsw_esp32.hpp"

#include <string>

namespace command_addapter {

namespace {

bool parse_bool_value(const std::string& value, bool default_value)
{
    if (value == "1" || value == "true" || value == "TRUE" || value == "yes")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE" || value == "no")
    {
        return false;
    }
    return default_value;
}

std::string extract_value(const std::string& payload, const std::string& key)
{
    const std::string token = key + "=";
    const size_t pos = payload.find(token);
    if (pos == std::string::npos)
    {
        return "";
    }

    const size_t start = pos + token.size();
    size_t end = payload.find(';', start);
    if (end == std::string::npos)
    {
        end = payload.find(',', start);
    }
    if (end == std::string::npos)
    {
        end = payload.size();
    }
    return payload.substr(start, end - start);
}

} // namespace

void CaBswEsp32::command_testData(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    printf("Test command received with %u bytes of data.\n", len);
    uart_.send(data, len); // Echo back received data
        
}

void CaBswEsp32::command_restart(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    printf("Restart command received.\n");
    esp_restart();
    response_code = 0x01; // Success
}

void CaBswEsp32::command_otaUpdate(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    printf("OTA is starting\n");
    //uart.send(data, len);
    if (len == 0)
    {
        ota_.start_update("http://www.erma.sk/fw/SW_WaterIrrigationController_esp32.bin");
    }
    else
    {
        ota_.start_update(reinterpret_cast<char*>(const_cast<uint8_t*>(data)));
    }
    response_code = 0x01; // Success
}

void CaBswEsp32::command_printApPassword(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    (void)data;
    (void)len;

    printf("Print credentials command received.\n");
    app::GlobalCredentials::Credentials global_credentials{};
    std::string ap_password;
    if (!global_credentials_.get(global_credentials) || !wifi_.get_ap_password(ap_password))
    {
        printf("Failed to read credentials from Wi-Fi config.\n");
        response_code = 0x02;
        return;
    }

    printf("Device ID: %s\n", global_credentials.device_id.c_str());
    printf("Device Password: %s\n", global_credentials.device_password.c_str());
    printf("AP Password: %s\n", ap_password.c_str());
    response_code = 0x01;
}

void CaBswEsp32::command_generateCredentials(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    printf("Generate Credentials command received.\n");

    std::string payload;
    if (data != nullptr && len > 0)
    {
        payload.assign(reinterpret_cast<const char*>(data), len);
    }

    bool update_device_id = false;
    bool reset_device_password = true;
    bool reset_ap_password = true;
    std::string new_device_id;

    if (!payload.empty())
    {
        if (payload.find('=') == std::string::npos)
        {
            new_device_id = payload;
            update_device_id = true;
        }
        else
        {
            const std::string id1 = extract_value(payload, "id");
            const std::string id2 = extract_value(payload, "device_id");
            if (!id1.empty())
            {
                new_device_id = id1;
                update_device_id = true;
            }
            else if (!id2.empty())
            {
                new_device_id = id2;
                update_device_id = true;
            }

            const std::string reset_dev = extract_value(payload, "reset_dev");
            const std::string reset_dev2 = extract_value(payload, "reset_device_password");
            if (!reset_dev.empty())
            {
                reset_device_password = parse_bool_value(reset_dev, reset_device_password);
            }
            else if (!reset_dev2.empty())
            {
                reset_device_password = parse_bool_value(reset_dev2, reset_device_password);
            }

            const std::string reset_ap = extract_value(payload, "reset_ap");
            const std::string reset_ap2 = extract_value(payload, "reset_ap_password");
            if (!reset_ap.empty())
            {
                reset_ap_password = parse_bool_value(reset_ap, reset_ap_password);
            }
            else if (!reset_ap2.empty())
            {
                reset_ap_password = parse_bool_value(reset_ap2, reset_ap_password);
            }
        }
    }

    app::GlobalCredentials::Credentials global_credentials{};
    if (!global_credentials_.update(new_device_id,
                                    update_device_id,
                                    "",
                                    false,
                                    reset_device_password,
                                    global_credentials))
    {
        printf("Failed to update global credentials.\n");
        response_code = 0x02;
        return;
    }

    std::string ap_password;
    if (reset_ap_password)
    {
        if (!wifi_.reset_ap_password(ap_password))
        {
            printf("Failed to reset AP password.\n");
            response_code = 0x02;
            return;
        }
    }
    else if (!wifi_.get_ap_password(ap_password))
    {
        printf("Failed to read AP password.\n");
        response_code = 0x02;
        return;
    }

    printf("Updated credentials:\n");
    printf("Device ID: %s\n", global_credentials.device_id.c_str());
    printf("Device Password: %s\n", global_credentials.device_password.c_str());
    printf("AP Password: %s\n", ap_password.c_str());
    response_code = 0x01;
}

void CaBswEsp32::command_readDeviceData(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    (void)data;
    (void)len;

    printf("Read Credentials command received.\n");
    app::GlobalCredentials::Credentials global_credentials{};
    std::string ap_password;
    if (!global_credentials_.get(global_credentials) || !wifi_.get_ap_password(ap_password))
    {
        printf("Failed to read credentials from Wi-Fi config.\n");
        response_code = 0x02;
        return;
    }

    printf("Device ID: %s\n", global_credentials.device_id.c_str());
    printf("Device Password: %s\n", global_credentials.device_password.c_str());
    printf("AP Password: %s\n", ap_password.c_str());
    printf("Device Type: %s\n", global_credentials.device_type == app::DeviceType::Wireless ? "radio" : "wired");
    printf("Valve Count: %u\n", global_credentials.valve_count);

    // print all global setting and wifi settings
    printf("Global Settings:\n");
    printf("  Device mode: %s\n", global_credentials.operating_mode == app::OperatingMode::Standard ? "Cloud+HA" : "Home Assistant");
    printf("  Device ID: %s\n", global_credentials.device_id.c_str());
    printf("  Device Password: %s\n", global_credentials.device_password.c_str());
    printf("  Device Type: %s\n", global_credentials.device_type == app::DeviceType::Wireless ? "radio" : "wired");
    printf("  Valve Count: %u\n", global_credentials.valve_count);

    printf("Wi-Fi Settings:\n");
    printf("  SSID: %s\n", wifi_.get_ssid().c_str());
    printf("  Wi-Fi Password: %s\n", wifi_.get_password().c_str());

    response_code = 0x01;
}

void CaBswEsp32::command_setDeviceType(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    if (data == nullptr || len < 1)
    {
        printf("Set Device Type: invalid payload.\n");
        response_code = 0x02;
        return;
    }

    const uint8_t device_type_value = data[0];
    if (device_type_value != 0x00 && device_type_value != 0x01)
    {
        printf("Set Device Type: unsupported value 0x%02X (expected 0x00=wired, 0x01=radio).\n", device_type_value);
        response_code = 0x02;
        return;
    }

    const app::DeviceType device_type = (device_type_value == 0x01) ? app::DeviceType::Wireless : app::DeviceType::Wired;
    if (!global_credentials_.set_device_type(device_type))
    {
        printf("Set Device Type: failed to store credentials.\n");
        response_code = 0x02;
        return;
    }

    printf("Device Type updated: %s\n", device_type_value == 0x01 ? "radio" : "wired");
    response_code = 0x01;

}

// valve count is stored as integer value, not char
void CaBswEsp32::command_setNumberOfValves(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    if (data == nullptr || len < 1)
    {
        printf("Set Number Of Valves: invalid payload.\n");
        response_code = 0x02;
        return;
    }

    uint32_t valve_count = 0;
    if (len >= 4)
    {
        valve_count = static_cast<uint32_t>(data[0]) |
                      (static_cast<uint32_t>(data[1]) << 8) |
                      (static_cast<uint32_t>(data[2]) << 16) |
                      (static_cast<uint32_t>(data[3]) << 24);
    }
    else
    {
        valve_count = static_cast<uint32_t>(data[0]);
    }

    if (valve_count == 0)
    {
        printf("Set Number Of Valves: value 0 is invalid.\n");
        response_code = 0x02;
        return;
    }

    if (!global_credentials_.set_valve_count(valve_count))
    {
        printf("Set Number Of Valves: failed to store credentials.\n");
        response_code = 0x02;
        return;
    }

    printf("Valve Count updated: %lu\n", static_cast<unsigned long>(valve_count));
    response_code = 0x01;
}

void CaBswEsp32::command_setPollingInterval(const uint8_t* data, const uint16_t len, uint8_t & response_code)
{
    if (data == nullptr || len < 1)
    {
        printf("Set Polling Interval: invalid payload.\n");
        response_code = 0x02;
        return;
    }

    uint32_t interval_ms = 0;
    if (len >= 4)
    {
        interval_ms = static_cast<uint32_t>(data[0]) |
                      (static_cast<uint32_t>(data[1]) << 8) |
                      (static_cast<uint32_t>(data[2]) << 16) |
                      (static_cast<uint32_t>(data[3]) << 24);
    }
    else
    {
        interval_ms = static_cast<uint32_t>(data[0]);
    }
    interval_ms *= 1000; // Convert seconds to milliseconds
    if (interval_ms < 1000)
    {
        printf("Set Polling Interval: value %lu ms is too low, minimum is 1000 ms.\n", static_cast<unsigned long>(interval_ms));
        response_code = 0x02;
        return;
    }

    if (!global_credentials_.set_sync_period_ms(interval_ms))
    {
        printf("Set Polling Interval: failed to store credentials.\n");
        response_code = 0x02;
        return;
    }

    printf("Polling Interval updated: %lu ms\n", static_cast<unsigned long>(interval_ms));
    response_code = 0x01;
}
    
   
} // namespace command_addapter