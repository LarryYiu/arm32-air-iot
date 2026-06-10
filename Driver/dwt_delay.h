#ifndef __DWT_H__
#define __DWT_H__

#include <stdint.h>
#include <stdbool.h>
#include "gd32f30x.h"

void DWT_Init(void);

void DWT_On(void);

void DWT_Off(void);

void DWT_Delay_us(uint32_t us);

// BAD: busy waiting
void DWT_Delay_ms(uint16_t ms);

#endif // __DWT_H__
