#include "led_driver.h"

static const uint8_t __LED_GPIO_LOOKUP[] = {LED_GPIO_LOOKUP};
static Led_t __Leds[len(__LED_GPIO_LOOKUP)];

void LED_Config(void)
{
    for(uint8_t i = 0; i < len(__LED_GPIO_LOOKUP); i++)
    {
        __Leds[i].gpioPort = GPIO_GetPeriphAddr(__LED_GPIO_LOOKUP[i]);
        __Leds[i].gpioPin  = GPIO_GetPinAddr(__LED_GPIO_LOOKUP[i]);
        __Leds[i].state    = RESET;
        rcu_periph_clock_enable(GPIO_GetRcuPeriph(__LED_GPIO_LOOKUP[i]));
        gpio_init(__Leds[i].gpioPort, GPIO_MODE_OUT_PP, LED_GPIO_FREQENCY, __Leds[i].gpioPin);
    }
}

void LED_Enable(uint8_t index)
{
    gpio_init(__Leds[index].gpioPort, GPIO_MODE_OUT_PP, LED_GPIO_FREQENCY, __Leds[index].gpioPin);
}

void LED_Disable(uint8_t index)
{
    gpio_init(__Leds[index].gpioPort, GPIO_MODE_AIN, LED_GPIO_FREQENCY, __Leds[index].gpioPin);
}

void LED_EnableAll(void)
{
    for(uint8_t i = 0; i < len(__LED_GPIO_LOOKUP); i++)
    {
        LED_Enable(i);
    }
}

void LED_DisableAll(void)
{
    for(uint8_t i = 0; i < len(__LED_GPIO_LOOKUP); i++)
    {
        LED_Disable(i);
    }
}

Led_t* LED_GetLed(uint8_t index)
{
    return &__Leds[index];
}

uint8_t LED_GetEncodedGPIO(uint8_t index)
{
    return __LED_GPIO_LOOKUP[index];
}

void LED_Toggle(uint8_t index)
{
    gpio_bit_write(__Leds[index].gpioPort, __Leds[index].gpioPin, __Leds[index].state ? RESET : SET);
    __Leds[index].state = __Leds[index].state ? RESET : SET;
}

FlagStatus LED_GetState(uint8_t index)
{
    return __Leds[index].state;
}

void LED_SetState(uint8_t index, bit_status stateIn)
{
    gpio_bit_write(__Leds[index].gpioPort, __Leds[index].gpioPin, stateIn);
    __Leds[index].state = stateIn;
}
