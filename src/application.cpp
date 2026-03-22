/**
 * @file application.cpp
 * @brief C++ class implementation
 */
#include "application.hpp"
#include "cloud/device_claim_client.hpp"
#include "asw_cfg/fw_version.hpp"
#include "asw_cfg/build_version.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <cstdio>
#include <cstring>

#include "asw_cfg/home_assistant_cfg.hpp"

extern "C" {
#include "application.h"
#include "freertos/idf_additions.h"
}

namespace app {

namespace {

constexpr const char* kDefaultOtaUrl = "http://www.erma.sk/fw/SW_WaterIrrigationController_esp32.bin";

std::string composeFirmwareVersion()
{
    char buffer[32] = {0};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%u.%u.%u+%u",
                  static_cast<unsigned int>(fw::kMajor),
                  static_cast<unsigned int>(fw::kMinor),
                  static_cast<unsigned int>(fw::kPatch),
                  static_cast<unsigned int>(fw::kBuild));
    return std::string(buffer);
}

} // namespace

    // create
Application::Application()
    : gpio_controller{},
      shift_register_(
        app::gpio_cfg::kGpioSerialDataId, // dataPin
        app::gpio_cfg::kGpioSerialClockId, // clockPin
        app::gpio_cfg::kGpioSerialLatchId, // latchPin
        app::gpio_cfg::kGpioSerialEnableId, // enablePin
        app::sld_cfg::kSldNumberOfOutputs, // number_of_outputs
        2,   // latch_hold_time_us
        0,   // reset_pin_hold_time_us
        5,    // data_pin_hold_time_us
        5     // clock_pin_hold_time_us
    ),
    uart(uart_cfg::getUartConfig())
{
    // Add tasks to scheduler
    _task_1ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task1ms();
        },
        1,    // priority
        1,   // run every 1 scheduler ticks (1 ms)
        this
    );
    scheduler.add_task(_task_1ms);

    _task_2ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task2ms();
        },
        1,    // priority
        2,   // run every 2 scheduler ticks (2 ms)
        this
    );
    scheduler.add_task(_task_2ms);

    _task_4ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task4ms();
        },
        1,    // priority
        4,   // run every 4 scheduler ticks (4 ms)
        this
    );
    scheduler.add_task(_task_4ms);

    _task_8ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task8ms();
        },
        1,    // priority
        8,   // run every 8 scheduler ticks (8 ms)
        this
    );
    scheduler.add_task(_task_8ms);

    _task_16ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task16ms();
        },
        1,    // priority
        16,   // run every 16 scheduler ticks (16 ms)
        this
    );
    scheduler.add_task(_task_16ms);
    _task_32ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task32ms();
        },
        1,    // priority
        32,   // run every 32 scheduler ticks (32 ms)
        this
    );
    scheduler.add_task(_task_32ms);
    _task_64ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task64ms();
        },
        1,    // priority
        64,   // run every 64 scheduler ticks (64 ms)
        this
    );
    scheduler.add_task(_task_64ms);
    _task_128ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->task128ms();
        },
        1,    // priority
        128,   // run every 128 scheduler ticks (128 ms)
        this
    );
    scheduler.add_task(_task_128ms);

    //scheduler 2
    _task_s2_10ms.construct(
        [](void* arg){
            static_cast<Application*>(arg)->c2task10ms();
        },
        1,    // priority
        10,   // run every 10 scheduler ticks (10 ms)
        this
    );
    scheduler_alt.add_task(_task_s2_10ms);
}

/************************************************************************************ */

/**
 * Entry point of the application. This function is called by the system after initialization.
 * It starts the main application logic.
 */
void Application::start()
{
    // Application start logic here
    earlyStart();
    // scheduler.init_timer();
    // scheduler.start();
    
    // configTICK_RATE_HZ has to be 1000!!!
    if (!scheduler.start_on_core(1, 1, 21))
    {
        printf("Failed to start main scheduler\n");
        return;
    }
    if (!scheduler_alt.start_on_core(0, 2, 24))
    {
        printf("Failed to start LED board scheduler\n");
        return;
    }

    TaskHandle_t late_start_task = nullptr;
    const BaseType_t late_start_rc = xTaskCreatePinnedToCore(
        &Application::lateStartTaskEntry,
        "late_start",
        8192,
        this,
        3,
        &late_start_task,
        1);
    if (late_start_rc != pdPASS)
    {
        printf("Failed to start late_start task. Restarting.\n");
        esp_restart();
    }

    while (true)
    {
        serviceResetButton();
        serviceManualRuns();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Application::lateStartTaskEntry(void* context)
{
    auto* self = static_cast<Application*>(context);
    if (self != nullptr)
    {
        self->lateStart();
    }
    vTaskDelete(nullptr);
}

void Application::earlyStart()
{
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    initGpio();

    wifi_.set_pairing_pin_callback([this](const std::string& pairing_pin) {
        if (!global_credentials_.set_pairing_pin(pairing_pin))
        {
            printf("Failed to persist pairing PIN from provisioning portal.\n");
        }
    });

    wifi_.set_operating_mode_callback([this](const std::string& mode_text) {
        const OperatingMode mode =
            (mode_text == "home_assistant" || mode_text == "pure_mqtt") ? OperatingMode::PureMqtt : OperatingMode::Standard;

        if (!global_credentials_.set_operating_mode(mode))
        {
            printf("Failed to persist operating mode.\n");
        }
        if (mode == OperatingMode::PureMqtt)
        {
            global_credentials_.set_pairing_pin("");
        }
    });

    bsw::Spi::Config spi_config {
        spi_cfg::kSpiHost,
        bsw::Spi::Mode::kMode0,
        spi_cfg::kSpiMosiPin,
        spi_cfg::kSpiMisoPin,
        spi_cfg::kSpiSckPin,
        // spi_cfg::kSpiCsPin,
        -1,
        5000000,
    };


    uart_initialized_ = uart.init();
    comm_manager_.init();
    shift_register_.init();
    ca_bswEsp32_.init(comm_manager_.getDispatcher());

    // wifi_.connect("ssid", "key_to_wifi");
    // time_.init();
    // for (uint8_t i = 0; !time_.isSynced() && i < 50; ++i)
    // {
    //     vTaskDelay(100 / portTICK_PERIOD_MS);
    // }
    // printf("Current Unix Timestamp: %u\n", time_.getUnixTimestamp());

    const std::string fw_version = composeFirmwareVersion();
    server_sync_client_.setFirmwareVersion(fw_version);
    printf("Application early start completed %s\n", fw_version.c_str());
    ota_.cancel_rollback();
}

/**
 * @brief Logic to run after the scheduler has started.
 * This method can be used to initialize components that depend on the scheduler.
 * It is called once after the scheduler has been started.
 */
void Application::lateStart()
{
    // Wi-Fi provisioning/connection can block; run it only after both schedulers are active.
    runWifiStartupFlow();
    const bool wifi_connected = wifi_.is_connected();
    printf("lateStart: wifi_connected=%s\n", wifi_connected ? "true" : "false");
    if (!wifi_connected)
    {
        printf("lateStart: starting server tasks anyway; clients will retry until Wi-Fi is available.\n");
    }
    startServerCommunicationTasks();
}

void Application::initGpio()
{
    // Initialize GPIOs using gpio_controller
    gpio_controller.init();
    for (auto gpio_ptr : app::gpio::gpios)
    {
        gpio_ptr->init();
    }
}

void Application::task1ms(void)
{
    uint16_t bytes_received = uart.receive(uart_rx_buffer_.data(), uart_rx_buffer_.size());
    if (bytes_received > 0)
    {
        uint8_t response_code = 0;
        comm_manager_.processIncomingData(uart_rx_buffer_.data(), bytes_received, response_code);
        if (response_code != 0)
        {
            uart.send_byte(0xAA); // Acknowledge byte
        }
    }
}

void Application::task2ms(void)
{

}
void Application::task4ms(void)
{
}
void Application::task8ms(void)
{
}

void Application::task16ms(void)
{
}
void Application::task32ms(void)
{
}
void Application::task64ms(void)
{
}
uint8_t output_data = 1;
void Application::task128ms(void)
{
    bool in_setup_mode = setup_mode_active_.load();
    bool wifi_connected = wifi_.is_connected();
    bool is_paired = paired_confirmed_.load();
    
    if (in_setup_mode)
    {
        current_led_state_ = LedIndicatorState::kResetMode;
    }
    else if (!wifi_connected)
    {
        current_led_state_ = LedIndicatorState::kWifiConnectMode;
    }
    else if (!is_paired)
    {
        current_led_state_ = LedIndicatorState::kServerPairingMode;
    }
    else
    {
        current_led_state_ = LedIndicatorState::kConnected;
    }

    static uint8_t blink_counter { 0 };
    
    switch (current_led_state_)
    {
        case LedIndicatorState::kResetMode:
        {
            gpio::led_status.toggleGpioState();
            break;
        }

        case LedIndicatorState::kWifiConnectMode:
        {
            blink_counter++;
            if (blink_counter >= 2)
            {
                blink_counter = 0;
                gpio::led_status.toggleGpioState();
            }
            break;
        }

        case LedIndicatorState::kServerPairingMode:
        {
            blink_counter = static_cast<uint8_t>((blink_counter + 1U) % 4U);
            if (blink_counter < 3U)
            {
                gpio::led_status.setState(bsw::GpioState::kHigh);
            }
            else
            {
                gpio::led_status.setState(bsw::GpioState::kLow);
            }
            break;
        }

        case LedIndicatorState::kConnected:
        {
            if (gpio::led_status.getState() != bsw::GpioState::kHigh)
            {
                gpio::led_status.setState(bsw::GpioState::kHigh);
            }
            blink_counter = 0;
            break;
        }
    }
}

uint16_t counter_10ms = 0;
uint32_t lastTick10ms = 0;
portMUX_TYPE myMux = portMUX_INITIALIZER_UNLOCKED;
bool renderedLastTick = false;
void Application::c2task10ms(void)
{
}

bool Application::isResetButtonPressed() const
{
    return gpio::reset_settings.getState() == bsw::GpioState::kHigh;
}

void Application::serviceResetButton()
{
    const bool pressed = isResetButtonPressed();
    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);

    if (pressed)
    {
        if (!reset_button_prev_pressed_)
        {
            reset_button_prev_pressed_ = true;
            reset_button_pressed_since_ms_ = now_ms;
            return;
        }

        const uint64_t held_ms = now_ms - reset_button_pressed_since_ms_;
        if (held_ms >= kResetHoldTimeMs)
        {
            printf("Reset button held > %lu ms. Clearing Wi-Fi credentials and rebooting to setup mode.\n",
                   static_cast<unsigned long>(kResetHoldTimeMs));
            wifi_.clear_wifi_credentials();
            global_credentials_.set_pairing_pin("");
            paired_confirmed_.store(false);
            esp_restart();
        }
    }
    else
    {
        reset_button_prev_pressed_ = false;
        reset_button_pressed_since_ms_ = 0;
    }
}

void Application::serviceManualRuns()
{
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    const uint32_t max_valves = (valve_count_ <= app::sld_cfg::kNumberOfWiredOutputs) ? valve_count_ : app::sld_cfg::kNumberOfWiredOutputs;
    for (uint32_t i = 0; i < max_valves; ++i)
    {
        bool should_close = false;
        taskENTER_CRITICAL(&valve_state_lock_);
        if (valve_open_states_[i] && now_us >= valve_close_deadlines_us_[i])
        {
            should_close = true;
        }
        taskEXIT_CRITICAL(&valve_state_lock_);

        if (!should_close)
        {
            continue;
        }

        closeValve(static_cast<uint16_t>(i));
        printf("Manual run timeout reached, valve %lu closed.\n", static_cast<unsigned long>(i));
    }
}

void Application::runWifiStartupFlow()
{
    wifi_.initialize();
    paired_confirmed_.store(false);
    server_tasks_allowed_ = true;
    app::GlobalCredentials::Credentials credentials{};
    // This call also loads the operating mode into the credentials struct
    if (global_credentials_.get(credentials))
    {
        device_hw_id_ = credentials.device_id;
        valve_count_ = credentials.valve_count;
        if (valve_count_ == 0U)
        {
            valve_count_ = app::sld_cfg::kNumberOfWiredOutputs;
        }
        if (valve_count_ > app::sld_cfg::kNumberOfWiredOutputs)
        {
            valve_count_ = app::sld_cfg::kNumberOfWiredOutputs;
        }
        sync_period_ms_ = credentials.sync_period_ms;
        printf("Configured mode: %s\n", credentials.operating_mode == OperatingMode::PureMqtt ? "Home Assistant" : "Cloud+HA");
    }
    else
    {
        device_hw_id_ = "UNKNOWN";
        valve_count_ = app::sld_cfg::kNumberOfWiredOutputs;
        sync_period_ms_ = 60000U;
    }

    const bool has_wifi_credentials = wifi_.has_wifi_credentials();
    if (!has_wifi_credentials)
    {
        printf("No stored Wi-Fi credentials. Starting provisioning AP.\n");
        setup_mode_active_.store(true);
        wifi_.start_provisioning_portal_blocking();
        setup_mode_active_.store(false);
        return;
    }

    if (!wifi_.connect_from_nvram(kWifiMaxConnectAttempts))
    {
        printf("Wi-Fi connection failed after %u attempts. Starting provisioning AP for recovery.\n", kWifiMaxConnectAttempts);
        setup_mode_active_.store(true);
        wifi_.start_provisioning_portal_blocking();
        setup_mode_active_.store(false);
        return;
    }

    if (credentials.operating_mode == OperatingMode::PureMqtt)
    {
        paired_confirmed_.store(true);
        server_tasks_allowed_ = true;
        printf("Startup mode: Pure MQTT. Skipping cloud pairing claim.\n");
        return;
    }

    std::string pairing_pin;
    if (global_credentials_.get_pairing_pin(pairing_pin) && !pairing_pin.empty() && !device_hw_id_.empty())
    {
        const auto claim_result = cloud::DeviceClaimClient::Claim(device_hw_id_, pairing_pin);
        printf("device_claim result: %s\n", cloud::DeviceClaimClient::ResultToString(claim_result));
        const bool claim_success =
            (claim_result == cloud::DeviceClaimClient::ClaimResult::kSuccess) ||
            (claim_result == cloud::DeviceClaimClient::ClaimResult::kAlreadyClaimed);
        if (claim_success)
        {
            global_credentials_.set_pairing_pin("");
        }
        paired_confirmed_.store(claim_success);
        server_tasks_allowed_ = true;
    }
    else
    {
        // No pending pairing PIN means there is nothing to claim right now.
        // Keep Cloud+HA mode operational and avoid indefinite pairing-pending LED state.
        paired_confirmed_.store(true);
        printf("No pairing PIN pending. Assuming already paired for Cloud+HA mode.\n");
        server_tasks_allowed_ = true;
    }
}

void Application::startServerCommunicationTasks()
{
    app::GlobalCredentials::Credentials credentials{};
    global_credentials_.get(credentials);

    printf("startServerCommunicationTasks: mode=%s hw_id='%s' sync_period_ms=%lu\n",
           credentials.operating_mode == OperatingMode::PureMqtt ? "Home Assistant" : "Cloud+HA",
           device_hw_id_.c_str(),
           static_cast<unsigned long>(sync_period_ms_));

    const bool cloud_mode_enabled = (credentials.operating_mode != OperatingMode::PureMqtt);

    if (cloud_mode_enabled)
    {
        if (!server_tasks_allowed_)
        {
            printf("Skipping server communication tasks until pairing claim succeeds.\n");
        }
        else
        {
            server_sync_client_.setCommandHandlers({
                this,
                &Application::cloudManualRunCb,
                &Application::cloudStopRunCb,
                &Application::cloudRestartCb,
                &Application::cloudFactoryResetCb,
                &Application::cloudUpdateCb,
                &Application::cloudSyncOkCb,
            });
            server_sync_client_.setDeviceHwId(device_hw_id_);
            server_sync_client_.setValveConnectionType(getDeviceType() == DeviceType::Wired ? "Wired" : "Radio");
            server_sync_client_.configure(nullptr, nullptr, sync_period_ms_);
            server_sync_client_.setValveCount(valve_count_);
            for (uint32_t i = 0; i < valve_count_; ++i)
            {
                server_sync_client_.setValveState(i, valve_open_states_[i]);
            }
            printf("startServerCommunicationTasks: invoking server_sync_client_.start()\n");
            server_sync_client_.start();
        }
    }

    home_assistant_bridge_.configure(device_hw_id_, valve_count_);
    home_assistant_bridge_.setCommandCallback(this, &Application::haSetValveStateCb);
    const bool ha_started = home_assistant_bridge_.start(app::ha_cfg::kMqttBrokerUri,
                                                         app::ha_cfg::kMqttUsername,
                                                         app::ha_cfg::kMqttPassword,
                                                         app::ha_cfg::kDiscoveryPrefix);
    if (ha_started)
    {
        for (uint32_t i = 0; i < valve_count_; ++i)
        {
            home_assistant_bridge_.publishValveState(i, valve_open_states_[i]);
        }
    }
    else
    {
        printf("HA MQTT bridge failed to start. Continuing with current mode tasks.\n");
    }

    if (!cloud_mode_enabled)
    {
        printf("Operating mode is Home Assistant. Cloud sync polling task is intentionally disabled.\n");
        return;
    }
    printf("Operating mode is Standard. Cloud sync and MQTT/HA bridge are enabled.\n");
}

void Application::cloudManualRunCb(void* context, uint32_t valve_index, uint32_t duration_sec)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }
    self->handleManualRunCommand(valve_index, duration_sec);
}

void Application::cloudStopRunCb(void* context, uint32_t valve_index)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }
    self->handleStopRunCommand(valve_index);
}

void Application::cloudRestartCb(void* context)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }
    esp_restart();
}

void Application::cloudFactoryResetCb(void* context)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }
    printf("Factory reset triggered by cloud command. Clearing Wi-Fi credentials and restarting.\n");
    self->wifi_.clear_wifi_credentials();
    self->paired_confirmed_.store(false);
    esp_restart();
}

void Application::cloudUpdateCb(void* context, const char* firmware_url)
{
    printf("cloudUpdateCb: command received. context=%p firmware_url=%s\n",
           context,
           (firmware_url != nullptr && firmware_url[0] != '\0') ? firmware_url : "<empty>");

    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        printf("cloudUpdateCb: ignored, context is null.\n");
        return;
    }

    if (firmware_url != nullptr && firmware_url[0] != '\0')
    {
        printf("Cloud update command provided firmware_url, but default OTA URL is enforced.\n");
    }

    printf("cloudUpdateCb: starting OTA with enforced URL: %s\n", kDefaultOtaUrl);
    const esp_err_t ota_result = self->ota_.start_update(kDefaultOtaUrl);
    if (ota_result == ESP_OK)
    {
        printf("cloudUpdateCb: ota_.start_update returned ESP_OK (device should reboot on successful OTA).\n");
    }
    else
    {
        printf("cloudUpdateCb: ota_.start_update failed: %s (%ld)\n",
               esp_err_to_name(ota_result),
               static_cast<long>(ota_result));
    }
}

void Application::cloudSyncOkCb(void* context)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }

    if (!self->paired_confirmed_.load())
    {
        self->paired_confirmed_.store(true);
        printf("Pairing confirmed by successful device_sync.\n");
    }
}

void Application::haSetValveStateCb(void* context, uint32_t valve_index, bool is_open)
{
    auto* self = static_cast<Application*>(context);
    if (self == nullptr)
    {
        return;
    }

    if (is_open)
    {
        self->openValve(static_cast<uint16_t>(valve_index));
    }
    else
    {
        self->closeValve(static_cast<uint16_t>(valve_index));
    }
}

void Application::handleManualRunCommand(uint32_t valve_index, uint32_t duration_sec)
{
    if (valve_index >= valve_count_ || valve_index >= app::sld_cfg::kNumberOfWiredOutputs)
    {
        printf("Unsupported valve index: %lu\n", static_cast<unsigned long>(valve_index));
        return;
    }

    openValve(static_cast<uint16_t>(valve_index));
    taskENTER_CRITICAL(&valve_state_lock_);
    valve_close_deadlines_us_[valve_index] = static_cast<uint64_t>(esp_timer_get_time()) +
                                             static_cast<uint64_t>(duration_sec) * 1000000ULL;
    taskEXIT_CRITICAL(&valve_state_lock_);
    printf("Manual run started for valve %lu, duration %u s\n",
           static_cast<unsigned long>(valve_index),
           duration_sec);
}

void Application::handleStopRunCommand(uint32_t valve_index)
{
    if (valve_index >= valve_count_ || valve_index >= app::sld_cfg::kNumberOfWiredOutputs)
    {
        printf("Unsupported valve index: %lu\n", static_cast<unsigned long>(valve_index));
        return;
    }

    closeValve(static_cast<uint16_t>(valve_index));
    printf("Manual run stopped for valve %lu\n", static_cast<unsigned long>(valve_index));
}

DeviceType Application::getDeviceType() const
{
    return global_credentials_.get_device_type();
}

void Application::openValve(uint16_t valve_index)
{
    if (valve_index >= valve_count_ || valve_index >= app::sld_cfg::kNumberOfWiredOutputs)
    {
        printf("openValve: unsupported index %u\n", valve_index);
        return;
    }

    if (getDeviceType() == DeviceType::Wired)
    {
        const uint8_t mask = static_cast<uint8_t>(1U << valve_index);
        const uint8_t new_state = static_cast<uint8_t>(shift_register_.getData(0) | mask);
        shift_register_.setOutput(0, new_state);
        shift_register_.updateOutputs();
        taskENTER_CRITICAL(&valve_state_lock_);
        valve_open_states_[valve_index] = true;
        taskEXIT_CRITICAL(&valve_state_lock_);
        server_sync_client_.setValveState(valve_index, true);
        home_assistant_bridge_.publishValveState(valve_index, true);
        printf("Valve %u opened.\n", valve_index);
    }
    else
    {
        printf("Open valve command received for valve %u, but radio control is not implemented yet.\n", valve_index);
    }
}

void Application::closeValve(uint16_t valve_index)
{
    if (valve_index >= valve_count_ || valve_index >= app::sld_cfg::kNumberOfWiredOutputs)
    {
        printf("closeValve: unsupported index %u\n", valve_index);
        return;
    }

    if (getDeviceType() == DeviceType::Wired)
    {
        const uint8_t mask = static_cast<uint8_t>(1U << valve_index);
        const uint8_t new_state = static_cast<uint8_t>(shift_register_.getData(0) & static_cast<uint8_t>(~mask));
        shift_register_.setOutput(0, new_state);
        shift_register_.updateOutputs();
    }

    taskENTER_CRITICAL(&valve_state_lock_);
    valve_open_states_[valve_index] = false;
    valve_close_deadlines_us_[valve_index] = 0;
    taskEXIT_CRITICAL(&valve_state_lock_);
    server_sync_client_.setValveState(valve_index, false);
    home_assistant_bridge_.publishValveState(valve_index, false);
}

} // namespace app

struct App_Handler { app::Application* instance; };

/** C facade ***************************************************/
extern "C" {
/**
 * @brief Creates and initializes an instance of the Application.
 * @return Pointer to the created handler, or nullptr on failure.
 */
App_Handle_t * App_Create(void)
{
    App_Handle_t * handler = new App_Handle_t;
    if (handler == nullptr)
    {
        return nullptr;
    }
    
    handler->instance = new app::Application();
    
    if (handler->instance == nullptr)
    {
        delete handler;
        return nullptr;
    }
    
    return handler;
}

/**
 * @brief Starts the Application.
 * @param as_handler Pointer to the handler.
 */
void App_Start(const App_Handle_t * as_handler)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->start();
}

void App_InitGpioPwm(const App_Handle_t * as_handler,
                  const uint8_t au8_pin,
                  const uint8_t au8_level,
                  const uint8_t au8_pwm_timer,
                  const uint8_t au8_pwm_channel,
                  const uint16_t au16_pwm_frequency,
                  const uint8_t au8_pwm_duty_cycle)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->gpio_controller.setDirection(au8_pin, bsw::GpioDirection::kOutput);
    if (au8_level == 0)
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kLow);
    }
    else
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kHigh);
    }
    as_handler->instance->gpio_controller.initPwm(au8_pin, au16_pwm_frequency, au8_pwm_duty_cycle, au8_pwm_channel, au8_pwm_timer);
}

void App_InitGpio(const App_Handle_t * as_handler, const uint8_t au8_pin, const uint8_t au8_level)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->gpio_controller.setDirection(au8_pin, bsw::GpioDirection::kOutput);
    if (au8_level == 0)
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kLow);
    }
    else
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kHigh);
    }
}

void App_SetGpio(const App_Handle_t * as_handler, const uint8_t au8_pin, const uint8_t au8_level)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    if (au8_level == 0)
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kLow);
    }
    else
    {
        as_handler->instance->gpio_controller.setGpioState(au8_pin, bsw::GpioState::kHigh);
    }
}

uint8_t App_GetGpio(const App_Handle_t * as_handler, const uint8_t au8_pin)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return -1; // Return -1 to indicate error
    }
    bsw::GpioState state = as_handler->instance->gpio_controller.getState(au8_pin);
    return (state == bsw::GpioState::kHigh) ? 1 : 0;
}

void App_SetPwm(const App_Handle_t * as_handler, const uint8_t au8_channel, const uint8_t au8_duty, const uint16_t au16_frequency, bool is_brightness, bool pwm_backwards)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->gpio_controller.setPwmDuty(au8_channel, au8_duty, is_brightness, pwm_backwards);
    as_handler->instance->gpio_controller.setPwmFreq(au8_channel, au16_frequency);
}

void App_SetSldData(const App_Handle_t * as_handler, const uint8_t* au8_data, const uint8_t au8_size)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->shift_register_.setData(au8_data, au8_size);
    as_handler->instance->shift_register_.updateOutputs();
}

void App_Delay(uint64_t au64_delayUs)
{
    uint32_t start = 0;
    uint32_t ticks = au64_delayUs * 240; // 240 ticks per us at 240MHz
    
    // Get current cycle count (XTHAL_GET_CCOUNT is an intrinsic)
    asm volatile("rsr %0, ccount" : "=a"(start));
    
    uint32_t current = start;
    while ((current - start) < ticks) {
        asm volatile("rsr %0, ccount" : "=a"(current));
    }
}

void App_SpiTransfer(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t* au8_txData, uint8_t* au8_rxData, const uint16_t au16_size)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->spi_.transfer(reg, au8_txData, au8_rxData, au16_size);
}
void App_SpiWrite(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t* au8_txData, const uint16_t au16_size)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->spi_.write_burst(reg, au8_txData, au16_size);
}
void App_SpiRead(const App_Handle_t * as_handler, const uint8_t reg, uint8_t* au8_rxData, const uint16_t au16_size)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->spi_.read_burst(reg, au8_rxData, au16_size);
}
void App_SpiWriteByte(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t au8_txData)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->spi_.write_byte(reg, au8_txData);
}
uint8_t App_SpiReadByte(const App_Handle_t * as_handler, const uint8_t reg)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return 0;
    }
    return as_handler->instance->spi_.read_byte(reg); // send dummy byte to read
}

} // extern "C"