#include "app_driver.h"

void APP_Init(void)
{
    __enable_irq();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); // for FreeRTOS
    LED_Config();
    DWT_Init();
    KEY_Config();
    SHT20_Init();
    HK_A5_Init();
    BAT_Init();
    RTC_Init();
    ESP8684_Init();
    SYSTICK_Config();
    STORAGE_Init();
}
