#include "gpio_decoder.h"

uint32_t GPIO_GetPeriphAddr(uint8_t encodedGPIO)
{
    return GPIO_BASE + ((uint32_t)(encodedGPIO >> 4) * (0x00000400));
}

uint32_t GPIO_GetPinAddr(uint8_t encodedGPIO)
{
    return BIT(encodedGPIO & 0x0f);
}

rcu_periph_enum GPIO_GetRcuPeriph(uint8_t encodedGPIO)
{
    return (rcu_periph_enum)RCU_REGIDX_BIT(APB2EN_REG_OFFSET, (uint32_t)((encodedGPIO >> 4) + 2U));
}
