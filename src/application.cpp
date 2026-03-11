/**
 * @file application.cpp
 * @brief C++ class implementation
 */
#include "application.hpp"
extern "C" {
#include "application.h"
}

namespace app {
    // create
Application::Application()
    : gpio_controller{},
      shift_register_(
        app::gpio_cfg::kGpioSerialDataId, // dataPin
        app::gpio_cfg::kGpioSerialClockId, // clockPin
        app::gpio_cfg::kGpioSerialLatchId, // latchPin
        app::sld_cfg::kSldNumberOfOutputs, // number_of_outputs
        app::sld_cfg::kSldNumberOfPwmChannels,   // number_of_pwm_channels
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
    lateStart();
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Application::earlyStart()
{
    initGpio();

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

    wifi_.connect("ssid", "key_to_wifi");
    time_.init();
    for (uint8_t i = 0; !time_.isSynced() && i < 50; ++i)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("Current Unix Timestamp: %u\n", time_.getUnixTimestamp());

    printf("Application early start completed %u.%u.%u\n", kMajorVersion, kMinorVersion, kPatchVersion);
    ota_.cancel_rollback();
}

/**
 * @brief Logic to run after the scheduler has started.
 * This method can be used to initialize components that depend on the scheduler.
 * It is called once after the scheduler has been started.
 */
void Application::lateStart()
{
}

void Application::initGpio()
{
    // Initialize GPIOs using gpio_controller
    gpio_controller.init();
    for (auto gpio_ptr : app::gpio::gpios)
    {
        gpio_ptr->init();
    }
    for (auto pwm_gpio_ptr : app::gpio::pwm_gpios)
    {
        pwm_gpio_ptr->init();
        pwm_gpio_ptr->initPwm(pwm_gpio_ptr->getPwmFrequency(), pwm_gpio_ptr->getPwmDutyCycle(), pwm_gpio_ptr->getPwmChannel(), pwm_gpio_ptr->getPwmTimer());
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
void Application::task128ms(void)
{
    // toggle every 5th tick
    static uint8_t counter { 0 };
    counter++;
    if (counter < 5)
    {
        return;
    }
    counter = 0;
    gpio::led_status.toggleGpioState();
}

uint16_t counter_10ms = 0;
uint32_t lastTick10ms = 0;
portMUX_TYPE myMux = portMUX_INITIALIZER_UNLOCKED;
bool renderedLastTick = false;
void Application::c2task10ms(void)
{
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

void App_SetSldBrightness(const App_Handle_t * as_handler, const uint8_t au8_channel, const uint8_t au8_brightness)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    as_handler->instance->shift_register_.setBrightness(au8_channel, au8_brightness);
}

void App_LedBoardTimerExpired(const App_Handle_t * as_handler, const uint8_t au8_timerId)
{
    if (as_handler == nullptr || as_handler->instance == nullptr)
    {
        return;
    }
    // do someaction when a timer expires on the led board
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