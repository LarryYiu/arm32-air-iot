#ifndef __APP_DRIVER_H__
#define __APP_DRIVER_H__

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gd32f30x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

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

void APP_Init(void);

#endif // __APP_DRIVER_H__