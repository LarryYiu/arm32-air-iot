#ifndef __WIFI_APP_H__
#define __WIFI_APP_H__

#include <stdbool.h>
#include "iot_config.h"

void WIFI_Run(void);

#if WIFI_USE_SMART_CONFIG
bool WIFI_IsSmartConfigTimedOut(void);
void WIFI_RestartSmartConfig(void);
#endif

#endif // __WIFI_APP_H__
