#include "storage_app.h"
#include <string.h>
#include "internal_flash.h"
#include "RTT_Debug.h"

#define BIT_MAGIC_CODE_0 0
#define BIT_MAGIC_CODE_1 1
#define BIT_MAGIC_CODE_2 2
#define BIT_MAGIC_CODE_3 3
#define BIT_VERSION 4
#define BIT_FLAGS 13
#define BIT_RESERVED 14
#define BIT_CRC 15

#define VERSION_LEN 10
#define MAGIC_CODE_LEN 4

#define DEBUG_PRINT true

static const SysParam_t __SYS_PARAM_DEFAULT = {.magicCode = STORE_MAGIC_CODE,
                                               .version   = "default",
                                               .flags     = 0,
                                               .crc       = 0};
static SysParam_t __currSysParam;

static uint8_t __flashIndex = 0;

static uint8_t __CalcCrc8(const uint8_t* buf, uint32_t len)
{
    uint8_t crc = 0xFF;

    for(uint8_t byte = 0; byte < len; byte++)
    {
        crc ^= (buf[byte]);
        for(uint8_t i = 8; i > 0; --i)
        {
            if(crc & 0x80)
            {
                crc = (crc << 1) ^ 0x31;
            }
            else
            {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

bool __STORAGE_SerializeSysParam(const SysParam_t* sysParam, uint8_t* buffer)
{
    if(sysParam == NULL || buffer == NULL)
    {
        return false;
    }
    buffer[BIT_MAGIC_CODE_0] = (sysParam->magicCode >> 24) & 0xFF;
    buffer[BIT_MAGIC_CODE_1] = (sysParam->magicCode >> 16) & 0xFF;
    buffer[BIT_MAGIC_CODE_2] = (sysParam->magicCode >> 8) & 0xFF;
    buffer[BIT_MAGIC_CODE_3] = (sysParam->magicCode) & 0xFF;
    memcpy(&buffer[BIT_VERSION], sysParam->version, VERSION_LEN);
    buffer[BIT_FLAGS] = sysParam->flags;
    buffer[BIT_CRC]   = sysParam->crc;
    return true;
}

bool __STORAGE_DeserializeSysParam(SysParam_t* sysParam, const uint8_t* buffer)
{
    if(sysParam == NULL || buffer == NULL)
    {
        return false;
    }
    sysParam->magicCode = ((uint32_t)buffer[BIT_MAGIC_CODE_0] << 24) | ((uint32_t)buffer[BIT_MAGIC_CODE_1] << 16) |
                          ((uint32_t)buffer[BIT_MAGIC_CODE_2] << 8) | (uint32_t)buffer[BIT_MAGIC_CODE_3];
    memcpy(sysParam->version, &buffer[BIT_VERSION], VERSION_LEN);
    sysParam->flags = buffer[BIT_FLAGS];
    sysParam->crc   = buffer[BIT_CRC];
    return true;
}

bool __STORAGE_IsAnySysParamWritten(void)
{
    uint8_t buffer[MAGIC_CODE_LEN];
    if(!FLASH_Read(FLASH_SYS_PARAM_ADDR, buffer, MAGIC_CODE_LEN))
    {
        return false;
    }
    if((((uint32_t)buffer[BIT_MAGIC_CODE_0] << 24) | ((uint32_t)buffer[BIT_MAGIC_CODE_1] << 16) |
        ((uint32_t)buffer[BIT_MAGIC_CODE_2] << 8) | (uint32_t)buffer[BIT_MAGIC_CODE_3]) != STORE_MAGIC_CODE)
    {
        DBG_log("[Storage Touched] No sys param found in flash, got 0x%02X%02X%02X%02X.\n", buffer[BIT_MAGIC_CODE_0],
                buffer[BIT_MAGIC_CODE_1], buffer[BIT_MAGIC_CODE_2], buffer[BIT_MAGIC_CODE_3]);
        __flashIndex = 0;
        return false;
    }
    return true;
}

bool __STORAGE_ReadSysParam(SysParam_t* sysParam)
{
    uint8_t start = 0;
    uint8_t end   = FLASH_PAGE_SIZE / SYS_PARAM_DATA_SIZE - 1;
    uint8_t index = FLASH_PAGE_SIZE / SYS_PARAM_DATA_SIZE / 2;
    uint8_t buffer[SYS_PARAM_DATA_SIZE];
    bool writen = false;

    if(!__STORAGE_IsAnySysParamWritten())
    {
        __flashIndex = 0;
        return false;
    }

    while(1)
    {
        memset(buffer, 0, MAGIC_CODE_LEN);
        if(!FLASH_Read(FLASH_SYS_PARAM_ADDR + index * SYS_PARAM_DATA_SIZE, buffer,
                       MAGIC_CODE_LEN)) // only read magic code for checking
        {
            return false;
        }
        if((((uint32_t)buffer[BIT_MAGIC_CODE_0] << 24) | ((uint32_t)buffer[BIT_MAGIC_CODE_1] << 16) |
            ((uint32_t)buffer[BIT_MAGIC_CODE_2] << 8) | (uint32_t)buffer[BIT_MAGIC_CODE_3]) == STORE_MAGIC_CODE)
        {
            writen = true;
            start  = index;
            index += (end - index) / 2;
#if DEBUG_PRINT
            DBG_log("[Storage APP] index update true: %d\n", index);
#endif
        }
        else
        {
            writen = false;
            end    = index;
            index -= (index - start) / 2;
#if DEBUG_PRINT
            DBG_log("[Storage APP] index update false: %d\n", index);
#endif
        }
        if(index == start || index == end)
        {
            if(!writen)
            {
                index--;
            }
            __flashIndex = index;
#if DEBUG_PRINT
            DBG_log("[Storage APP] sys param index found: %d\n", index);
#endif
            break;
        }
    }
    if(!FLASH_Read(FLASH_SYS_PARAM_ADDR + __flashIndex * SYS_PARAM_DATA_SIZE, buffer, SYS_PARAM_DATA_SIZE))
    {
#if DEBUG_PRINT
        DBG_log("[ERR: Storage READ] Failed to read sys param from flash.\n");
#endif
        return false;
    }
    if(!__STORAGE_DeserializeSysParam(sysParam, buffer))
    {
#if DEBUG_PRINT
        DBG_log("[ERR: Storage DESERIALIZE] Failed to deserialize sys param.\n");
#endif
        return false;
    }
    // check crc
    uint8_t crcVal = __CalcCrc8(buffer, SYS_PARAM_DATA_SIZE - 1); // exclude crc field
    if(crcVal != sysParam->crc)
    {
#if DEBUG_PRINT
        DBG_log("[ERR: Storage CRC] Sys param crc check failed. Calculated crc: 0x%02X, Stored crc: 0x%02X\n", crcVal,
                sysParam->crc);
#endif
        return false;
    }
    return true;
}

bool __STORAGE_WriteSysParam(SysParam_t* sysParam)
{
    if(sysParam == NULL)
    {
        return false;
    }
    uint8_t buffer[SYS_PARAM_DATA_SIZE];
    sysParam->magicCode = STORE_MAGIC_CODE;
    if(!__STORAGE_SerializeSysParam(sysParam, buffer))
    {
#if DEBUG_PRINT
        DBG_log("[ERR: Storage SERIALIZE] Failed to serialize sys param.\n");
#endif
        return false;
    }
    sysParam->crc   = __CalcCrc8(buffer, SYS_PARAM_DATA_SIZE - 1); // exclude crc field
    buffer[BIT_CRC] = sysParam->crc;

    if(__flashIndex >= SYS_PARAM_DATA_SIZE / 8)
    {
        if(!FLASH_Erase(FLASH_SYS_PARAM_ADDR, FLASH_PAGE_SIZE))
        {
#if DEBUG_PRINT
            DBG_log("[ERR: Storage ERASE] Failed to erase flash page for sys param.\n");
#endif
            return false;
        }
        __flashIndex = 0;
    }
    else
    {
        if(!__STORAGE_IsAnySysParamWritten())
        {
            __flashIndex = 0;
        }
        else
        {
            __flashIndex++;
        }
    }
    if(!FLASH_Write(FLASH_SYS_PARAM_ADDR + __flashIndex * SYS_PARAM_DATA_SIZE, buffer, SYS_PARAM_DATA_SIZE))
    {
#if DEBUG_PRINT
        DBG_log("[ERR: Storage WRITE] Failed to write sys param to flash.\n");
#endif
        return false;
    }
    return true;
}

void STORAGE_Init(void)
{
    SysParam_t sysParam;
    if(__STORAGE_ReadSysParam(&sysParam))
    {
        __currSysParam = sysParam;
#if DEBUG_PRINT
        DBG_log("[Storage APP] Sys param read from flash successfully. Version: %s, CRC: 0x%02X, Rollback: %s\n",
                __currSysParam.version, __currSysParam.crc, (__currSysParam.flags & 0x01) ? "true" : "false");
#endif
    }
    else
    {
#if DEBUG_PRINT
        DBG_log("[Storage APP] Failed to read sys param. using default values.\n");
#endif
        __currSysParam = __SYS_PARAM_DEFAULT;
    }
}

void STORAGE_GetSysVersion(char* version)
{
    if(version != NULL)
    {
        strcpy(version, __currSysParam.version);
    }
}

bool STORAGE_SetSysVersion(char* version)
{
    if(version != NULL)
    {
        if(strcmp(version, __currSysParam.version) == 0)
        {
#if DEBUG_PRINT
            DBG_log("[Storage APP] New version is the same as current version. No need to update.\n");
#endif
            return true;
        }
        SysParam_t bufferSysParam = __currSysParam;
        memset(bufferSysParam.version, 0, VERSION_LEN);
        strcpy(bufferSysParam.version, version);
        if(__STORAGE_WriteSysParam(&bufferSysParam))
        {
            __currSysParam = bufferSysParam;
#if DEBUG_PRINT
            DBG_log("[Storage APP] Sys param written to flash successfully. Version: %s, CRC: 0x%02X, Rollback: %s\n",
                    __currSysParam.version, __currSysParam.crc, (__currSysParam.flags & 0x01) ? "true" : "false");
#endif
            return true;
        }
        else
        {
#if DEBUG_PRINT
            DBG_log("[Storage APP] Failed to write sys param.\n");
#endif
        }
    }
    return false;
}
