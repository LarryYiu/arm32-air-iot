#ifndef __INTERNAL_FLASH_H__
#define __INTERNAL_FLASH_H__

#include <stdint.h>
#include <stdbool.h>

#define FLASH_PAGE_SIZE (2048U)              // 2K
#define FLASH_DEADLINE_ADDRESS (0x80800000U) // 512K
#define FLASH_SIZE (0x80000U)                // 0x08080000 - 0x08000000
#define PARAMETER_ADDR_IN_FLASH (0x807E800U) // must be dividaible by 2048
#define FLASH_SYS_PARAM_ADDR (0x807F800U)

bool FLASH_Read(uint32_t addr, uint8_t* buffer, uint32_t lenReading);

bool FLASH_Write(uint32_t addr, uint8_t* buffer, uint32_t lenWriting);

bool FLASH_Erase(uint32_t addr, uint32_t lenErasing);

void FLASH_Test(void);

#endif // __INTERNAL_FLASH_H__
