#include "led_driver.h"
#include "key_driver.h"
#include "dwt_delay.h"
#include "systick.h"
#include "RTT_Debug.h"

void ShortPressListener(void)
{
    DBG_log("Short press detected.\n");
    LED_Toggle(0);
}

int main()
{
    LED_Config();
    DWT_Init();
    KEY_Config();
    SYSTICK_Config();

    KEY_AddShortPressListener(0, ShortPressListener);

    while(1)
    {
        KEY_Scan(0);
    }
}
