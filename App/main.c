#include <math.h>
#include "led_driver.h"
#include "key_driver.h"
#include "dwt_delay.h"
#include "systick.h"
#include "sht20_driver.h"
#include "RTT_Debug.h"

void ShortPressListener(void)
{
    float _temp     = SHT20_GetTemp(false);
    float _humidity = SHT20_GetHumidity(false);
    if(!isnan(_temp))
    {
        int32_t temp = (int32_t)(_temp * 100.0f);
        DBG_log("[SHT20] Temp: %d.%02d C\n", temp / 100, ((temp % 100) > 0 ? abs(temp % 100) : 0));
    }

    if(!isnan(_humidity))
    {
        uint32_t humidity = (uint32_t)(_humidity * 100.0f);
        DBG_log("[SHT20] Humidity: %d.%02d %%\n", humidity / 100, humidity % 100);
    }
    // SHT20_test();
}

int main()
{
    LED_Config();
    DWT_Init();
    KEY_Config();
    SHT20_Init();
    SYSTICK_Config();

    KEY_AddShortPressListener(0, ShortPressListener);

    while(1)
    {
        KEY_Scan(0);
        SHT20_Run();
    }
}
