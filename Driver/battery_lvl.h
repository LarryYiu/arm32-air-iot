#ifndef __BATTERY_LVL_H__
#define __BATTERY_LVL_H__

#include <stdint.h>

typedef enum
{
    BATTERY_LEVEL_0 = 0,
    BATTERY_LEVEL_1,
    BATTERY_LEVEL_2,
    BATTERY_LEVEL_3,
    BATTERY_LEVEL_4,
    BATTERY_LEVEL_NOT_INIT,
} BAT_LVL_t;

void BAT_Init(void);

float BAT_GetVoltage(void);

BAT_LVL_t BAT_GetLevel(float voltage);

void BAT_Test(void);

#endif // __BATTERY_LVL_H__
