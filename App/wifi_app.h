#ifndef __WIFI_APP_H__
#define __WIFI_APP_H__

#include <stdbool.h>
#include "iot_config.h"

#if WIFI_USE_SMART_CONFIG
bool WIFI_IsSmartConfigTimedOut(void);
void WIFI_RestartSmartConfig(void);
#endif

void WIFI_RTOS_Init(void);

#endif // __WIFI_APP_H__
