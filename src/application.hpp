/**
 * @file application.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <array>
#include "scheduler.hpp"
#include "scheduler_task.hpp"
#include "gpio.hpp"
#include "gpio_controller.hpp"
#include "bsw_cfg.hpp"
#include "watchdog.hpp"
#include "uart.hpp"
#include "comm_manager.hpp"
#include "ota.hpp"
#include "wifi.hpp"
#include "global_credentials.hpp"
#include "time.hpp"
#include "shift_register.hpp"

#include "ca_bsw_esp32.hpp"

namespace app {

class Application {
    constexpr static uint16_t kMajorVersion {1} ;
    constexpr static uint16_t kMinorVersion {0} ;
    constexpr static uint16_t kPatchVersion {1} ;

    // configTICK_RATE_HZ has to be 1000!!!
    static constexpr uint16_t kSchedulerPeriodUs {1000}; // 1 ms tick

public:
    Application();
    ~Application() = default;

    /* Starting methods */
    void earlyStart();
    void start();
    void lateStart();

    /* Initialization methods */
    void initGpio();

    /* Task functions */
    void task1ms(void);
    void task2ms(void);
    void task4ms(void);
    void task8ms(void);
    void task16ms(void);
    void task32ms(void);
    void task64ms(void);
    void task128ms(void);
    void c2task10ms(void);

    /* Tasks */
    bsw::SchedulerTask _task_blink_led;
    bsw::SchedulerTask _task_1ms;
    bsw::SchedulerTask _task_2ms;
    bsw::SchedulerTask _task_4ms;
    bsw::SchedulerTask _task_8ms;
    bsw::SchedulerTask _task_16ms;
    bsw::SchedulerTask _task_32ms;
    bsw::SchedulerTask _task_64ms;
    bsw::SchedulerTask _task_128ms;
    // Scheduler 2 tasks
    bsw::SchedulerTask _task_s2_10ms;

    /* BSW modules */
    bsw::GpioController gpio_controller;
    bsw::Scheduler scheduler{kSchedulerPeriodUs}; // 1 ms tick
    bsw::Scheduler scheduler_alt{kSchedulerPeriodUs}; // 1 ms tick
    bsw::Uart uart;
    bsw::Spi spi_;
    m_shiftregister::ShiftRegister shift_register_;

private:
    static constexpr uint32_t kResetHoldTimeMs {5000};
    static constexpr uint8_t kWifiMaxConnectAttempts {3};

    command_addapter::CommManager<128, 1024> comm_manager_;
    std::array<uint8_t, 1024> uart_rx_buffer_;
    bool uart_initialized_ = false;
    bool setup_mode_active_ = false;
    bool reset_button_prev_pressed_ = false;
    uint64_t reset_button_pressed_since_ms_ = 0;

    bsw::Wifi wifi_;
    app::GlobalCredentials global_credentials_;
    bsw::Ota ota_;
    bsw::Time time_;

    bool isResetButtonPressed() const;
    void serviceResetButton();
    void runWifiStartupFlow();

    command_addapter::CaBswEsp32 ca_bswEsp32_{uart, ota_, wifi_, global_credentials_};
};

} // namespace app