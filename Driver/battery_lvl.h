#ifndef __BATTERY_LVL_H__
#define __BATTERY_LVL_H__

#include <stdint.h>

void BAT_Init(void);

float BAT_GetVoltage(void);

uint8_t BAT_GetLevel(void);

void BAT_Test(void);

#endif // __BATTERY_LVL_H__
