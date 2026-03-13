/**
 * @file gpio_cfg.cpp
 * @brief GPIO configuration implementation
 */

#include "bsw_cfg.hpp"

/* ---------------- GPIO Configuration ---------------- */

namespace app::gpio {

bsw::GpioController gpio_controller;

bsw::Gpio led_status(gpio_controller,
                     gpio_cfg::kGpioLedStatusId,
                     gpio_cfg::kGpioLedStatusDirection,
                     gpio_cfg::kGpioLedStatusPullMode,
                     gpio_cfg::kGpioLedStatusInitialState);

bsw::Gpio serial_clock(gpio_controller,
                       gpio_cfg::kGpioSerialClockId,
                       gpio_cfg::kGpioSerialClockDirection,
                       gpio_cfg::kGpioSerialClockPullMode,
                       gpio_cfg::kGpioSerialClockInitialState);

bsw::Gpio serial_data(gpio_controller,
                      gpio_cfg::kGpioSerialDataId,
                      gpio_cfg::kGpioSerialDataDirection,
                      gpio_cfg::kGpioSerialDataPullMode,
                      gpio_cfg::kGpioSerialDataInitialState);

bsw::Gpio serial_latch(gpio_controller,
                       gpio_cfg::kGpioSerialLatchId,
                       gpio_cfg::kGpioSerialLatchDirection,
                       gpio_cfg::kGpioSerialLatchPullMode,
                       gpio_cfg::kGpioSerialLatchInitialState);

bsw::Gpio serial_enable(gpio_controller,
                        gpio_cfg::kGpioSerialEnableId,
                        gpio_cfg::kGpioSerialEnableDirection,
                        gpio_cfg::kGpioSerialEnablePullMode,
                        gpio_cfg::kGpioSerialEnableInitialState);

bsw::Gpio reset_settings(gpio_controller,
                         gpio_cfg::kGpioResetSettingsId,
                         gpio_cfg::kGpioResetSettingsDirection,
                         gpio_cfg::kGpioResetSettingsPullMode,
                         gpio_cfg::kGpioResetSettingsInitialState);

bsw::Gpio radio_input(gpio_controller,
                      gpio_cfg::kGpioRadioInputId,
                      gpio_cfg::kGpioRadioInputDirection,
                      gpio_cfg::kGpioRadioInputPullMode,
                      gpio_cfg::kGpioRadioInputInitialState);

bsw::Gpio radio_output(gpio_controller,
                       gpio_cfg::kGpioRadioOutputId,
                       gpio_cfg::kGpioRadioOutputDirection,
                       gpio_cfg::kGpioRadioOutputPullMode,
                       gpio_cfg::kGpioRadioOutputInitialState);

bsw::Gpio uart_tx(gpio_controller,
                  gpio_cfg::kGpioUartTxId,
                  gpio_cfg::kGpioUartTxDirection,
                  gpio_cfg::kGpioUartTxPullMode,
                  gpio_cfg::kGpioUartTxInitialState);

bsw::Gpio uart_rx(gpio_controller,
                  gpio_cfg::kGpioUartRxId,
                  gpio_cfg::kGpioUartRxDirection,
                  gpio_cfg::kGpioUartRxPullMode,
                  gpio_cfg::kGpioUartRxInitialState);

bsw::Gpio spi_mosi(gpio_controller,
                   gpio_cfg::kGpioSpiMosiId,
                   gpio_cfg::kGpioSpiMosiDirection,
                   gpio_cfg::kGpioSpiMosiPullMode,
                   gpio_cfg::kGpioSpiMosiInitialState);

bsw::Gpio spi_miso(gpio_controller,
                   gpio_cfg::kGpioSpiMisoId,
                   gpio_cfg::kGpioSpiMisoDirection,
                   gpio_cfg::kGpioSpiMisoPullMode,
                   gpio_cfg::kGpioSpiMisoInitialState);

bsw::Gpio spi_sck(gpio_controller,
                    gpio_cfg::kGpioSpiSckId,
                    gpio_cfg::kGpioSpiSckDirection,
                    gpio_cfg::kGpioSpiSckPullMode,
                    gpio_cfg::kGpioSpiSckInitialState);

bsw::Gpio spi_cs(gpio_controller,
                    gpio_cfg::kGpioSpiCsId,
                    gpio_cfg::kGpioSpiCsDirection,
                    gpio_cfg::kGpioSpiCsPullMode,
                    gpio_cfg::kGpioSpiCsInitialState);

// put them all in vector for easy initialization
std::vector<bsw::Gpio*> gpios = {
    &led_status,
    &serial_clock,
    &serial_data,
    &serial_latch,
    &serial_enable,
    &reset_settings,
    &radio_input,
    &radio_output,
    &uart_tx,
    &uart_rx,
    &spi_mosi,
    &spi_miso,
    &spi_sck,
    &spi_cs
};

} // namespace app::gpio