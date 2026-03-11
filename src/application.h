/**
 * @file application.h
 * @brief Public interface for the Application module.
 *
 * Provides function declarations, macros, and data structures
 * that are exposed to other parts of the program.
 */

#ifndef __APPLICATION_H
#define __APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif
/* ---------------------------------------------------------------------------- */
/*  Public macros and constants                                                 */
/* ---------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------- */
/*  Public type definitions                                                     */
/* ---------------------------------------------------------------------------- */
typedef struct App_Handler App_Handle_t;

/* ---------------------------------------------------------------------------- */
/*  Public variables                                                            */
/* ---------------------------------------------------------------------------- */
extern App_Handle_t * g_app_handle;

/* ---------------------------------------------------------------------------- */
/*  Public function prototypes                                                  */
/* ---------------------------------------------------------------------------- */
App_Handle_t * App_Create(void);
void App_Start(const App_Handle_t * as_handler);
void App_InitGpioPwm(const App_Handle_t * as_handler,
                      const uint8_t au8_pin,
                      const uint8_t au8_level,
                      const uint8_t au8_pwm_timer,
                      const uint8_t au8_pwm_channel,
                      const uint16_t au16_pwm_frequency,
                      const uint8_t au8_pwm_duty_cycle);
void App_InitGpio(const App_Handle_t * as_handler,
                  const uint8_t au8_pin,
                  const uint8_t au8_level);
void App_SetGpio(const App_Handle_t * as_handler, const uint8_t au8_pin, const uint8_t au8_level);
uint8_t App_GetGpio(const App_Handle_t * as_handler, const uint8_t au8_pin);
void App_SetPwm(
    const App_Handle_t * as_handler, 
    const uint8_t au8_channel, 
    const uint8_t au8_duty, 
    const uint16_t au16_frequency, 
    bool is_brightness, 
    bool pwm_backwards
);

void App_Delay(uint64_t au64_delayUs);

void App_SpiTransfer(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t* au8_txData, uint8_t* au8_rxData, const uint16_t au16_size);
void App_SpiWrite(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t* au8_txData, const uint16_t au16_size);
void App_SpiRead(const App_Handle_t * as_handler, const uint8_t reg, uint8_t* au8_rxData, const uint16_t au16_size);
void App_SpiWriteByte(const App_Handle_t * as_handler, const uint8_t reg, const uint8_t au8_txData);
uint8_t App_SpiReadByte(const App_Handle_t * as_handler, const uint8_t reg);

#ifdef __cplusplus
}   // extern "C"
#endif

#endif // __APPLICATION_H