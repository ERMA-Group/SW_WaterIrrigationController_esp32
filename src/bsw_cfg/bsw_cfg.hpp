#ifndef __GPIO_CFG_H
#define __GPIO_CFG_H

#pragma once

#include <cstdint>
#include <vector>
#include "gpio.hpp"
#include "uart.hpp"
#include "spi.hpp"

namespace app {

namespace sld_cfg {
    constexpr uint16_t kSldNumberOfOutputs {1};
} // namespace sld_cfg

namespace gpio_cfg
{
    /* GPIO LED Status pin settings */
    constexpr uint8_t kGpioLedStatusId = 27;
    constexpr bsw::GpioDirection kGpioLedStatusDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioLedStatusPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioLedStatusInitialState = bsw::GpioState::kHigh;

    /* GPIO Serial Clock pin settings */
    constexpr uint8_t kGpioSerialClockId = 25;
    constexpr bsw::GpioDirection kGpioSerialClockDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSerialClockPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSerialClockInitialState = bsw::GpioState::kLow;

    /* GPIO Serial Data pin settings */
    constexpr uint8_t kGpioSerialDataId = 33;
    constexpr bsw::GpioDirection kGpioSerialDataDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSerialDataPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSerialDataInitialState = bsw::GpioState::kLow;

    /* GPIO Serial Latch pin settings */
    constexpr uint8_t kGpioSerialLatchId = 32;
    constexpr bsw::GpioDirection kGpioSerialLatchDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSerialLatchPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSerialLatchInitialState = bsw::GpioState::kLow;

    /* GPIO Serial Enable pin settings */
    constexpr uint8_t kGpioSerialEnableId = 26;
    constexpr bsw::GpioDirection kGpioSerialEnableDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSerialEnablePullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSerialEnableInitialState = bsw::GpioState::kLow;

    /* GPIO Reset Settings pin settings */
    constexpr uint8_t kGpioResetSettingsId = 34;
    constexpr bsw::GpioDirection kGpioResetSettingsDirection = bsw::GpioDirection::kInput;
    constexpr bsw::GpioPullMode kGpioResetSettingsPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioResetSettingsInitialState = bsw::GpioState::kLow;

    /* GPIO Radio Input / nRESET pin settings */
    constexpr uint8_t kGpioRadioInputId = 35;
    constexpr bsw::GpioDirection kGpioRadioInputDirection = bsw::GpioDirection::kInput;
    constexpr bsw::GpioPullMode kGpioRadioInputPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioRadioInputInitialState = bsw::GpioState::kLow;

    /* GPIO Radio Output / DIO0 pin settings */
    constexpr uint8_t kGpioRadioOutputId = 4;
    constexpr bsw::GpioDirection kGpioRadioOutputDirection = bsw::GpioDirection::kInput;
    constexpr bsw::GpioPullMode kGpioRadioOutputPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioRadioOutputInitialState = bsw::GpioState::kLow;

    /* GPIO UART TX pin settings */
    constexpr uint8_t kGpioUartTxId = 17;
    constexpr bsw::GpioDirection kGpioUartTxDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioUartTxPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioUartTxInitialState = bsw::GpioState::kLow;

    /* GPIO UART RX pin settings */
    constexpr uint8_t kGpioUartRxId = 16;
    constexpr bsw::GpioDirection kGpioUartRxDirection = bsw::GpioDirection::kInput;
    constexpr bsw::GpioPullMode kGpioUartRxPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioUartRxInitialState = bsw::GpioState::kLow;

    /* GPIO SPI MOSI pin settings */
    constexpr uint8_t kGpioSpiMosiId = 23;
    constexpr bsw::GpioDirection kGpioSpiMosiDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSpiMosiPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSpiMosiInitialState = bsw::GpioState::kLow;

    /* GPIO SPI MISO pin settings */
    constexpr uint8_t kGpioSpiMisoId = 19;
    constexpr bsw::GpioDirection kGpioSpiMisoDirection = bsw::GpioDirection::kInput;
    constexpr bsw::GpioPullMode kGpioSpiMisoPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSpiMisoInitialState = bsw::GpioState::kLow;

    /* GPIO SPI SCK pin settings */
    constexpr uint8_t kGpioSpiSckId = 18;
    constexpr bsw::GpioDirection kGpioSpiSckDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSpiSckPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSpiSckInitialState = bsw::GpioState::kLow;

    /* GPIO SPI CS pin settings */
    constexpr uint8_t kGpioSpiCsId = 5;
    constexpr bsw::GpioDirection kGpioSpiCsDirection = bsw::GpioDirection::kOutput;
    constexpr bsw::GpioPullMode kGpioSpiCsPullMode = bsw::GpioPullMode::kNone;
    constexpr bsw::GpioState kGpioSpiCsInitialState = bsw::GpioState::kHigh;
} // namespace gpio_cfg

namespace spi_cfg
{
    /* SPI configuration */
    constexpr bsw::Spi::Host kSpiHost = bsw::Spi::Host::kSpi2; // VSPI
    constexpr uint8_t kSpiSckPin = gpio_cfg::kGpioSpiSckId;
    constexpr uint8_t kSpiMosiPin = gpio_cfg::kGpioSpiMosiId;
    constexpr uint8_t kSpiMisoPin = gpio_cfg::kGpioSpiMisoId;
    constexpr uint8_t kSpiCsPin = gpio_cfg::kGpioSpiCsId;
    constexpr spi_dma_chan_t kSpiDmaChannel = SPI_DMA_DISABLED;
} // namespace spi_cfg

namespace uart_cfg
{
    /* UART configuration */
    constexpr bsw::Uart::Module kUartModule = bsw::Uart::Module::kUart0;
    constexpr bsw::Uart::DataBits kUartDataBits = bsw::Uart::DataBits::kDataBits8;
    constexpr bsw::Uart::StopBits kUartStopBits = bsw::Uart::StopBits::kStopBits1;
    constexpr bsw::Uart::Parity kUartParity = bsw::Uart::Parity::kParityDisable;
    constexpr uint8_t kUartTxPin = 1;
    constexpr uint8_t kUartRxPin = 3;
    constexpr uint32_t kUartBaudRate = 115200;
    constexpr uint16_t kUartRxBufferSize = 1024;
    bsw::Uart::Config getUartConfig();
} // namespace uart_cfg

namespace gpio
{
    extern bsw::GpioController gpio_controller;

    extern bsw::Gpio led_status;
    extern bsw::Gpio serial_clock;
    extern bsw::Gpio serial_data;
    extern bsw::Gpio serial_latch;
    extern bsw::Gpio serial_enable;

    extern bsw::Gpio reset_settings;
    extern bsw::Gpio radio_input;
    extern bsw::Gpio radio_output;
    extern bsw::Gpio uart_tx;
    extern bsw::Gpio uart_rx;
    extern bsw::Gpio spi_mosi;
    extern bsw::Gpio spi_miso;
    extern bsw::Gpio spi_sck;
    extern bsw::Gpio spi_cs;

    // put them all in vector for easy initialization
    extern std::vector<bsw::Gpio*> gpios;
}

} // namespace app

#endif /* __GPIO_CFG_H */