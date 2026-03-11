/**
 * @file ca_bsw_esp32.cpp
 * @brief C++ class implementation
 */

#include "ca_bsw_esp32.hpp"
namespace command_addapter {

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
        ota_.start_update("http://www.erma.sk/fw/SW_LedControlBoard.bin");
    }
    else
    {
        ota_.start_update(reinterpret_cast<char*>(const_cast<uint8_t*>(data)));
    }
    response_code = 0x01; // Success
}

} // namespace command_addapter