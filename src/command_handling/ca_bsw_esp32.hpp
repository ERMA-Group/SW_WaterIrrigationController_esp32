/**
 * @file ca_bsw_esp32.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "dispatcher.hpp"

#include "ota.hpp"
#include "uart.hpp"

namespace command_addapter {

class CaBswEsp32 {
public:
    static constexpr uint16_t kStartId = 0x0000;
    static constexpr uint16_t kEndId = 0x00FF;

    CaBswEsp32(bsw::Uart& uart, bsw::Ota& ota)
        : uart_{uart}, ota_{ota} {}
    ~CaBswEsp32() = default;

    void init(DispatcherBase& dispatcher)
    {
        dispatcher.subscribe<CaBswEsp32, &CaBswEsp32::command_testData>(kStartId + 0x0000, this);
        dispatcher.subscribe<CaBswEsp32, &CaBswEsp32::command_restart>(kStartId + 0x0001, this);
        dispatcher.subscribe<CaBswEsp32, &CaBswEsp32::command_otaUpdate>(kStartId + 0x00EF, this);
    }

    void command_testData(const uint8_t* data, const uint16_t len, uint8_t & response_code);
    void command_restart(const uint8_t* data, const uint16_t len, uint8_t & response_code);
    void command_otaUpdate(const uint8_t* data, const uint16_t len, uint8_t & response_code);

private:
    bsw::Uart& uart_;
    bsw::Ota& ota_;
};

} // namespace command_addapter