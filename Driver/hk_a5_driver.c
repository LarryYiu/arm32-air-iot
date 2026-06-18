#include "hk_a5_driver.h"
#include "uart.h"
#include "config.h"
#include "systick.h"
#include "RTT_Debug.h"

#define DEBUG_LOG true

#define __PA6_SET() gpio_bit_set(GPIOA, GPIO_PIN_6)
#define __PA6_RESET() gpio_bit_reset(GPIOA, GPIO_PIN_6)
#define __PA6_GET() gpio_input_bit_get(GPIOA, GPIO_PIN_6)

static uint16_t __pm25Value      = 0;
static bool __isPm25ValueUpdated = false;

typedef enum __HKA5_STATE __HKA5_STATE_t;
enum __HKA5_STATE
{
    HKA5_STATE_OFF,
    HKA5_STATE_READ,
    HKA5_STATE_VALIDATE,
    HKA5_STATE_PROCESS,
};

static __HKA5_STATE_t __state = HKA5_STATE_READ;
static uint64_t __timeout     = 0;

void HK_A5_Init(void)
{
    // PA9 TxD - RxD
    // PA10 RxD - TxD
    // PA6 - SET  1 = ON 0 = OFF
    UART_Config(HK_A5_UART_BAUDRATE); // this enables the GPIOA clock already
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_6);
}

void HK_A5_Enable(void)
{
    __PA6_SET();
    __state   = HKA5_STATE_READ;
    __timeout = SYSTICK_GetSysRunTime();
}

void HK_A5_Disable(void)
{
    __PA6_RESET();
    __state = HKA5_STATE_OFF;
}

void HK_A5_Toggle(void)
{
    if(__PA6_GET())
    {
        HK_A5_Disable();
    }
    else
    {
        HK_A5_Enable();
    }
}

#define __FRAME_HEADER_H (0x42)
#define __FRAME_HEADER_L (0x4d)
#define __INDEX_FRAME_LEN_H (3)
#define __INDEX_FRAME_LEN_L (4)
#define __INDEX_PM25_H (6)
#define __INDEX_PM25_L (7)
#define __INDEX_PARTICAL25_H (22)
#define __INDEX_PARTICAL25_L (23)

static uint16_t __GetCheckSum(const uint8_t* data)
{
    uint16_t checksum = 0;
    for(uint8_t i = 0; i < HK_A5_UART_BUFFER_SIZE - 2; i++)
    {
        checksum += data[i];
    }
    return checksum;
}

bool __ValidateDat(uint8_t* dat)
{
    if(dat[0] != __FRAME_HEADER_H || dat[1] != __FRAME_HEADER_L)
    {
        // DBG_log("[ERROR PM25]: Header dismatched, got 0x%02X 0x%02X\n", dat[0], dat[1]);
        return false;
    }
    else
    {
        uint16_t checksum = __GetCheckSum(dat);
        if((uint8_t)(checksum >> 8) != dat[HK_A5_UART_BUFFER_SIZE - 2] ||
           (uint8_t)(checksum & 0xFF) != dat[HK_A5_UART_BUFFER_SIZE - 1])
        {
#if (DEBUG_LOG)
            DBG_log("[ERROR PM25]: Checksum dismatched, got 0x%02X%02X, expected 0x%04X\n",
                    dat[HK_A5_UART_BUFFER_SIZE - 2], dat[HK_A5_UART_BUFFER_SIZE - 1], checksum);
#endif
            return false;
        }
        else
        {
            return true;
        }
    }
}

void HK_A5_Run(void)
{
    static uint8_t dataSnapshot[HK_A5_UART_BUFFER_SIZE];
    switch(__state)
    {
        case HKA5_STATE_OFF:
            HK_A5_Disable();
            break;
        case HKA5_STATE_READ:
            if(!UART_IsDataReady())
            {
                if(SYSTICK_GetSysRunTime() - __timeout > HK_A5_NO_DATA_TIMEOUT_MS)
                {
#if (DEBUG_LOG)
                    DBG_log("[HK_A5] No data received for a while, turning off HK-A5.\n");
#endif
                    __state = HKA5_STATE_OFF;
                }
            }
            else
            {
                UART_GetDataSnapshot(dataSnapshot);
                __state = HKA5_STATE_VALIDATE;
            }
            break;
        case HKA5_STATE_VALIDATE:
            if(__ValidateDat(dataSnapshot))
            {
                __state = HKA5_STATE_PROCESS;
            }
            else
            {
#if (DEBUG_LOG)
                DBG_log("[HK_A5] Data validation failed, retrying\n");
#endif
                __timeout = SYSTICK_GetSysRunTime();
                __state   = HKA5_STATE_READ;
            }
            break;
        case HKA5_STATE_PROCESS:
        {
            // Process the validated data, test case below
            // uint16_t pm25 = (uint16_t)dataSnapshot[__INDEX_PM25_H] << 8 | dataSnapshot[__INDEX_PM25_L];
            // uint16_t partical25 =
            //     (uint16_t)dataSnapshot[__INDEX_PARTICAL25_H] << 8 | dataSnapshot[__INDEX_PARTICAL25_L];
            // DBG_log("[HK_A5] PM2.5: %d ug/m3, Partical 2.5: %d ug/m3\n", pm25, partical25);
            // __timeout = SYSTICK_GetSysRunTime();
            // __state   = HKA5_STATE_READ;

            __pm25Value          = (uint16_t)dataSnapshot[__INDEX_PM25_H] << 8 | dataSnapshot[__INDEX_PM25_L];
            __isPm25ValueUpdated = true;
            __timeout            = SYSTICK_GetSysRunTime();
            __state              = HKA5_STATE_READ;
        }
        break;
        default:
            break;
    }
}

bool HK_A5_IsPm25ValueUpdated(void)
{
    return __isPm25ValueUpdated;
}

uint16_t HK_A5_GetPm25Value(void)
{
    __isPm25ValueUpdated = false;
    return __pm25Value;
}

void HK_A5_Test(void)
{
    if(!UART_IsDataReady())
    {
        return;
    }
    HK_A5_Disable();
    const uint8_t* g_rcvDataBuf = UART_GetDataBuffer();
    if(g_rcvDataBuf[0] != __FRAME_HEADER_H || g_rcvDataBuf[1] != __FRAME_HEADER_L)
    {
        // DBG_log("[ERROR PM25]: Header dismatched, got 0x%02X 0x%02X\n", g_rcvDataBuf[0], g_rcvDataBuf[1]);
        return;
    }
    else
    {
        uint16_t checksum = __GetCheckSum(g_rcvDataBuf);
        if((uint8_t)(checksum >> 8) != g_rcvDataBuf[HK_A5_UART_BUFFER_SIZE - 2] ||
           (uint8_t)(checksum & 0xFF) != g_rcvDataBuf[HK_A5_UART_BUFFER_SIZE - 1])
        {
            DBG_log("[ERROR PM25]: Checksum dismatched, got 0x%02X%02X, expected 0x%04X\n",
                    g_rcvDataBuf[HK_A5_UART_BUFFER_SIZE - 2], g_rcvDataBuf[HK_A5_UART_BUFFER_SIZE - 1], checksum);
            return;
        }
        else
        {
            // for(uint8_t i = 0; i < HK_A5_UART_BUFFER_SIZE; i++)
            // {
            //     DBG_log("[%d] %02X ", i, g_rcvDataBuf[i]);
            // }
            uint16_t pm25 = (uint16_t)g_rcvDataBuf[__INDEX_PM25_H] << 8 | g_rcvDataBuf[__INDEX_PM25_L];
            uint16_t partical25 =
                (uint16_t)g_rcvDataBuf[__INDEX_PARTICAL25_H] << 8 | g_rcvDataBuf[__INDEX_PARTICAL25_L];
            DBG_log("[PM2.5] PM2.5: %d ug/m3, Partical 2.5: %d ug/m3\n", pm25, partical25);
        }
    }
    HK_A5_Enable();
}
