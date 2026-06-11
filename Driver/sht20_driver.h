#ifndef __SHT20_DRIVER_H__
#define __SHT20_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

void SHT20_Init(void);

void SHT20_Enable(void);

void SHT20_Disable(void);

void SHT20_Run(void);

float SHT20_GetTemp(bool handleError);

float SHT20_GetHumidity(bool handleError);

float SHT20_test(void);

#endif // __SHT20_DRIVER_H__
