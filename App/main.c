#include <math.h>
#include "gd32f30x.h"
#include "led_driver.h"
#include "key_driver.h"
#include "dwt_delay.h"
#include "systick.h"
#include "sht20_driver.h"
#include "RTT_Debug.h"
#include "hk_a5_driver.h"
#include "battery_lvl.h"
#include "rtc.h"
#include "wifi_app.h"
#include "esp8684_driver.h"
#include "internal_flash.h"
#include "storage_app.h"

void ShortPressListener(void)
{
    // float _temp     = SHT20_GetTemp(false);
    // float _humidity = SHT20_GetHumidity(false);
    // if(!isnan(_temp))
    // {
    //     int32_t temp = (int32_t)(_temp * 100.0f);
    //     DBG_log("[SHT20] Temp: %d.%02d C\n", temp / 100, ((temp % 100) > 0 ? abs(temp % 100) : 0));
    // }

    // if(!isnan(_humidity))
    // {
    //     uint32_t humidity = (uint32_t)(_humidity * 100.0f);
    //     DBG_log("[SHT20] Humidity: %d.%02d %%\n", humidity / 100, humidity % 100);
    // }
    // BAT_Test();
    // RTC_Test();
    // FLASH_Test();

    char version[10];
    STORAGE_GetSysVersion(version);
    DBG_log("[APP] Current sys version: %s\n", version);

    // char version[10] = "1.0.1";
    // STORAGE_SetSysVersion(version);

    // FLASH_Erase(FLASH_SYS_PARAM_ADDR, FLASH_PAGE_SIZE);
}

int main()
{
    __enable_irq();
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

    KEY_AddShortPressListener(0, ShortPressListener);

    HK_A5_Disable();
    DBG_log("RTT running...\n");
    while(1)
    {
        KEY_Scan(0);
        // SHT20_Run();
        // HK_A5_Run();
        // WIFI_Run();
    }
}
