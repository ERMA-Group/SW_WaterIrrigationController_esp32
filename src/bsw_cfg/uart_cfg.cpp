/**
 * @file uart_cfg.cpp
 * @brief UART configuration implementation
 */

#include "bsw_cfg.hpp"

/* ---------------- UART Configuration ---------------- */
namespace app::uart_cfg
{
    bsw::Uart::Config getUartConfig()
    {
        return bsw::Uart::Config{
            .module = kUartModule,
            .data_bits = kUartDataBits,
            .stop_bits = kUartStopBits,
            .parity = kUartParity,
            .tx_pin = kUartTxPin,
            .rx_pin = kUartRxPin,
            .baud_rate = kUartBaudRate,
            .rx_buf_size = kUartRxBufferSize
        };
    }
} // namespace uart_cfg