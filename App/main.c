#include <math.h>
#include "led_driver.h"
#include "key_driver.h"
#include "dwt_delay.h"
#include "systick.h"
#include "sh20t_driver.h"
#include "RTT_Debug.h"

void ShortPressListener(void)
{
    // float temp     = SH20T_GetTemp(true);
    // float humidity = SH20T_GetHumidity(true);
    // if(!isnan(temp))
    // {
    //     int32_t temp = (int32_t)(SH20T_GetTemp(true) * 100.0f);
    //     DBG_log("[SH20T] Temp: %d.%02d C\n", temp / 100, ((temp % 100) > 0 ? abs(temp % 100) : 0));
    // }

    // if(!isnan(humidity))
    // {
    //     int32_t humidity = (int32_t)(SH20T_GetHumidity(true) * 100.0f);
    //     DBG_log("[SH20T] Humidity: %d.%02d %%\n", humidity / 100, humidity % 100);
    // }
    SH20T_test();
}

int main()
{
    LED_Config();
    DWT_Init();
    KEY_Config();
    SH20T_Init();
    SYSTICK_Config();

    KEY_AddShortPressListener(0, ShortPressListener);

    while(1)
    {
        KEY_Scan(0);
        // SH20T_Run();
    }
}
