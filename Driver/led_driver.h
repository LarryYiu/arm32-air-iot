#ifndef __LED_DRV_H__
#define __LED_DRV_H__

#include <stdint.h>
#include <stdbool.h>
#include "gd32f30x.h"
#include "gpio_decoder.h"
#include "config.h"

#ifndef LED_GPIO_FREQENCY
#define LED_GPIO_FREQENCY GPIO_OSPEED_2MHZ
#endif

struct Led
{
    uint32_t gpioPort;
    uint32_t gpioPin;
    bit_status state;
};
typedef struct Led Led_t;

enum LedIndex
{
    LED1 = 0,
    LED2,
    LED3,
};
typedef enum LedIndex LedIndex_t;

/**
 * **********************************************************************
 * @brief  Configures LED GPIO.
 * **********************************************************************
 */
void LED_Config(void);

/**
 * **********************************************************************
 * @brief  Enables ALL LED GPIO (sets to output push-pull mode).
 * **********************************************************************
 */
void LED_EnableAll(void);

/**
 * **********************************************************************
 * @brief  Disables ALL LED GPIO (sets to analog input mode).
 * **********************************************************************
 */
void LED_DisableAll(void);

/**
 * **********************************************************************
 * @brief  Enables the specified LED GPIO (sets to output push-pull mode).
 * @param  index: LED index (0-based)
 * **********************************************************************
 */
void LED_Enable(uint8_t index);

/**
 * **********************************************************************
 * @brief  Disables the specified LED GPIO (sets to analog input mode).
 * @param  index: LED index (0-based)
 * **********************************************************************
 */
void LED_Disable(uint8_t index);

/**
 * **********************************************************************
 * @brief  Get LED struct by index.
 * @param  index: LED index (0-based)
 *
 * @return Led_t struct with identifier and initial state (RESET)
 *
 * @note The index is indicated by the order in config.h's LED_LOOK_UP macro, starting from 0.
 * **********************************************************************
 */
Led_t* LED_GetLed(uint8_t index);

/**
 * **********************************************************************
 * @brief  Get the encoded GPIO identifier for the specified LED index.
 * @param  index: LED index (0-based)
 * @return Encoded GPIO identifier (e.g., GPIO_PIN(A,8))
 */
uint8_t LED_GetEncodedGPIO(uint8_t index);

/**
 * **********************************************************************
 * @brief  Toggle the state of the specified LED.
 * @param  index: LED index (0-based)
 * **********************************************************************
 */
void LED_Toggle(uint8_t index);

/**
 * **********************************************************************
 * @brief  Get the current state of the specified LED.
 * @param  index: LED index (0-based)
 * @return Current state of the LED (SET for on, RESET for off)
 * **********************************************************************
 */
FlagStatus LED_GetState(uint8_t index);

/**
 * **********************************************************************
 * @brief  Set the state of the given LED.
 * @param  index: LED index (0-based)
 * @param  stateIn: Desired state (SET for on, RESET for off)
 * **********************************************************************
 */
void LED_SetState(uint8_t index, bit_status stateIn);

#endif // __LED_DRV_H__
