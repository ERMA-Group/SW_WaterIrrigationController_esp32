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
#include <atomic>
#include <string>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}
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
#include "cloud/server_sync_client.hpp"

#include "ca_bsw_esp32.hpp"

namespace app {

class Application {
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

    /* LED indicator states */
    enum class LedIndicatorState {
        kResetMode,          ///< Fast blink - AP setup mode
        kWifiConnectMode,    ///< Slow blink - connecting to Wi-Fi
        kServerPairingMode,  ///< 3:1 high:low - pairing to backend over connected Wi-Fi
        kConnected,          ///< Solid on - Wi-Fi connected and paired
    };
    LedIndicatorState current_led_state_ = LedIndicatorState::kResetMode;

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
    std::atomic<bool> paired_confirmed_ {false};
    bool server_tasks_allowed_ = true;
    bool reset_button_prev_pressed_ = false;
    uint64_t reset_button_pressed_since_ms_ = 0;
    uint32_t valve_count_ = 8;
    std::array<bool, app::sld_cfg::kSldNumberOfOutputs> valve_open_states_{};
    std::array<uint64_t, app::sld_cfg::kSldNumberOfOutputs> valve_close_deadlines_us_{};
    std::string device_hw_id_;
    cloud::ServerSyncClient server_sync_client_;

    bsw::Wifi wifi_;
    app::GlobalCredentials global_credentials_;
    bsw::Ota ota_;
    bsw::Time time_;

    bool isResetButtonPressed() const;
    void serviceResetButton();
    void serviceManualRuns();
    void runWifiStartupFlow();
    void startServerCommunicationTasks();

    static void cloudManualRunCb(void* context, uint32_t valve_index, uint32_t duration_sec);
    static void cloudStopRunCb(void* context, uint32_t valve_index);
    static void cloudRestartCb(void* context);
    static void cloudFactoryResetCb(void* context);
    static void cloudUpdateCb(void* context, const char* firmware_url);
    static void cloudSyncOkCb(void* context);
    void handleManualRunCommand(uint32_t valve_index, uint32_t duration_sec);
    void handleStopRunCommand(uint32_t valve_index);

    DeviceType getDeviceType() const;
    void openValve(uint16_t valve_index);
    void closeValve(uint16_t valve_index);

    command_addapter::CaBswEsp32 ca_bswEsp32_{uart, ota_, wifi_, global_credentials_};
};

} // namespace app