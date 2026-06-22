#ifndef __STORAGE_APP_H__
#define __STORAGE_APP_H__

#include <stdint.h>
#include <stdbool.h>

#define SYS_PARAM_DATA_SIZE 16U
#define STORE_MAGIC_CODE (0xdeadbeefU)

typedef struct SysParam SysParam_t;

struct SysParam
{
    uint32_t magicCode;
    char version[10];
    uint8_t flags; // bit0: isRollback, bit1~bit7: reserved
    uint8_t crc;   // crc8 of all previous fields
};

void STORAGE_Init(void);

void STORAGE_GetSysVersion(char* version);

bool STORAGE_SetSysVersion(char* version);

#endif // __STORAGE_APP_H__
