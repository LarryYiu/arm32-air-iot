#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#include <stdint.h>

void SYSTICK_Init(void);

uint64_t SYSTICK_GetSysRunTime(void);

#endif // __SYSTICK_H__
