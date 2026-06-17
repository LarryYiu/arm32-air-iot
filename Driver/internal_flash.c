#include "internal_flash.h"
#include <string.h>
#include "gd32f30x.h"
#include "RTT_Debug.h"
#include "dwt_delay.h"

static uint8_t backupBuffer[FLASH_PAGE_SIZE];

static void __Unlock()
{
    __disable_irq();
    fmc_unlock();
}

static void __Lock()
{
    fmc_lock();
    __enable_irq();
}

static bool __IsAddrValid(uint32_t addr, uint32_t lenOperation)
{
    return (addr + lenOperation) <= FLASH_DEADLINE_ADDRESS;
}

bool FLASH_Read(uint32_t addr, uint8_t* buffer, uint32_t lenReading)
{
    if(!__IsAddrValid(addr, lenReading))
    {
        DBG_log("[ERROR: Flash Read] addr out of range\n");
        return false;
    }
    memcpy(buffer, (const void*)addr, lenReading);
    return true;
}

static fmc_state_enum __Erase(uint32_t addr)
{
    DBG_log("[Flash Erase] erasing 0x%08x\r\n", addr);
    __Unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    fmc_state_enum fmcState = fmc_page_erase(addr);
    __Lock();
    DBG_log("[Flash Erase] erasing 0x%08x done\r\n", addr);
    return fmcState;
}

/**
 * @brief Handle erasing of a single uneven page, inlcudes backup and resote
 *
 * @param addrPageStart the starting address of the page
 * @param addrEraseStart the starting address of the area to be erased
 * @param lenErasing the length to be erased
 * @return erase success or fail
 */
static bool __ProcessEraseSinglePage(uint32_t addrPageStart, uint32_t addrEraseStart, uint32_t lenErasing)
{
    if(addrPageStart == addrEraseStart && lenErasing == FLASH_PAGE_SIZE)
    {
        DBG_log("[Flash Erase] Case whole page\r\n");
        return __Erase(addrPageStart) == FMC_READY;
    }
    else
    {
        /* Back up the page */
        memcpy(backupBuffer, (const void*)addrPageStart, FLASH_PAGE_SIZE);
        DBG_log("[Flash Backup] backup finished\r\n");

        /* Process the page data, remove erased data */
        memset(backupBuffer + (addrEraseStart - addrPageStart), 0xff, lenErasing);

        /* Erase the page */
        __Erase(addrEraseStart);

        /* Restore the page */
        bool res = FLASH_Write(addrPageStart, backupBuffer, FLASH_PAGE_SIZE);
        DBG_log("[Flash Backup] restore finished\r\n");
        return res;
    }
}

bool FLASH_Erase(uint32_t addr, uint32_t lenErasing) // better not to do recurrsion as limited resources
{
    if(!__IsAddrValid(addr, lenErasing))
    {
        DBG_log("[Error: Flash Erase] addr out of range\r\n");
        return false;
    }
    else
    {
        // | ------------------|----------------------------------|---------|
        // page 0          erase start      (len remain)        page 2

        uint32_t addrEraseStart = addr % FLASH_PAGE_SIZE;
        uint32_t lenRemain      = FLASH_PAGE_SIZE - addrEraseStart;
        if(lenErasing > lenRemain)
        {
            // deal with the first page
            if(!__ProcessEraseSinglePage(addr - addrEraseStart, addr, lenRemain))
            {
                goto erase_err;
            }
            lenErasing -= lenRemain;
            uint32_t addrPage = addr - addrEraseStart + FLASH_PAGE_SIZE;
            // deal with the middle pages
            while(lenErasing > FLASH_PAGE_SIZE)
            {
                if(!__ProcessEraseSinglePage(addrPage, addrPage, FLASH_PAGE_SIZE))
                {
                    goto erase_err;
                }
                addrPage += FLASH_PAGE_SIZE;
                lenErasing -= FLASH_PAGE_SIZE;
            }
            // deal with the leftoever
            if(lenErasing > 0)
            {
                if(!__ProcessEraseSinglePage(addrPage, addrPage, lenErasing))
                {
                    goto erase_err;
                }
            }
            else
            {
                __Lock(); // for insurence
                return true;
            }
        }
        else
        {
            DBG_log("[Flash Erase]: single page\r\n");
            if(!__ProcessEraseSinglePage(addr - addrEraseStart, addr, lenErasing))
            {
                goto erase_err;
            }
        }
        return true;
    }
erase_err:
    __Lock();
    return false;
}
#define WRITE_DEBUG false
bool FLASH_Write(uint32_t addr, uint8_t* buffer, uint32_t lenWriting)
{
    if((addr + lenWriting) > FLASH_DEADLINE_ADDRESS)
    {
        DBG_log("[ERROR: Flash Write] addr out of range\n");
        return false;
    }

    if(addr % 2 == 1) // addr must matches a half-word
    {
        DBG_log("[ERROR: Flash Write] addr must be half-word aligned\n");
        return false;
    }

    DBG_log("[Flash Writing]: Writing Start\n");
#if WRITE_DEBUG
    uint8_t validateBuffer[2];
#endif
    __Unlock();
    for(uint32_t i = 0; i < lenWriting / 2; i++)
    {
#if WRITE_DEBUG
        DBG_log("[Flash Writing Data]: %02x %02x\r\n", buffer[0], buffer[1]);
#endif
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
        if(fmc_halfword_program(addr, *(uint16_t*)buffer) != FMC_READY)
        {
            __Lock();
            DBG_log("[Error: Flash Writing]: invalid writing, FMC_READY not ready\r\n");
            return false;
        }
#if WRITE_DEBUG
        __Lock();
        memcpy(validateBuffer, (const void*)addr, 2);
        DBG_log("[Flash Writing Validate]: %02x %02x\r\n", validateBuffer[0], validateBuffer[1]);
        __Unlock();
#endif
        buffer += 2;
        addr += 2;
    }
    /* Handle the case that the last piece writing is a byte instead of a half-word */
    if(lenWriting % 2 != 0)
    {
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
        uint16_t last = *buffer | 0xff00; // fill in the MSBs
        if(fmc_halfword_program(addr, last) != FMC_READY)
        {
            __Lock();
            DBG_log("[ERROR: Flash Writing]: Writing Failed\n");
            return false;
        }
    }
    __Lock();
    DBG_log("[Flash Writing]: Writing Finished\n");
    return true;
}

void __PrintStr(uint32_t addr, char* str, uint8_t len)
{
    FLASH_Read(addr, (uint8_t*)str, len);
    for(uint8_t i = 0; i < len; i++)
    {
        DBG_log("%02x ", str[i]);
        DWT_Delay_us(500);
    }
    DBG_log("\n");
}

void FLASH_Test(void)
{
    char str[128];
    char s1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a};

    DBG_log("================CLear flash======================\r\n");
    FLASH_Erase(PARAMETER_ADDR_IN_FLASH, FLASH_PAGE_SIZE);
    FLASH_Erase(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE, FLASH_PAGE_SIZE);
    FLASH_Erase(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE * 2, FLASH_PAGE_SIZE);
    DBG_log("================Write Initial Data===============\r\n");
    FLASH_Write(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE - 10, (uint8_t*)s1, (sizeof(s1) / sizeof((s1)[0])));
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE - 10, str, 10);
    FLASH_Write(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE, (uint8_t*)s1, (sizeof(s1) / sizeof((s1)[0])));
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE, str, 10);
    FLASH_Write(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE * 2, (uint8_t*)s1, (sizeof(s1) / sizeof((s1)[0])));
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE * 2, str, 10);
    DBG_log("================Case 1======================\r\n");
    FLASH_Erase(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE - 2, 4 + 2048);
    DBG_log("================page 1======================\r\n");
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE - 10, str, 10);
    DBG_log("================page 2======================\r\n");
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE, str, 10);
    DBG_log("================page 3======================\r\n");
    __PrintStr(PARAMETER_ADDR_IN_FLASH + FLASH_PAGE_SIZE * 2, str, 10);
    DBG_log("================End======================\r\n");
}
