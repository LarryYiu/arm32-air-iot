#ifndef __HK_A5_DRIVER_H__
#define __HK_A5_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

void HK_A5_Init(void);
void HK_A5_Run(void);
void HK_A5_Test(void);

void HK_A5_Enable(void);
void HK_A5_Disable(void);
void HK_A5_Toggle(void);

bool HK_A5_IsPm25ValueUpdated(void);
uint16_t HK_A5_GetPm25Value(void);

#endif // __HK_A5_DRIVER_H__
