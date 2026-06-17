#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>

typedef struct RTC_Time RTC_Time_t;
struct RTC_Time
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

void RTC_Init(void);

void RTC_GetTime(RTC_Time_t* time);

void RTC_SetTime(const RTC_Time_t* time);

void RTC_Test(void);

void RTC_Print(const RTC_Time_t* time);

#endif // __RTC_H__
