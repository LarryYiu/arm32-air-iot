#include "sht20_driver.h"
#include <math.h>
#include "gd32f30x.h"
#include "config.h"
#include "gpio_decoder.h"
#include "dwt_delay.h"
#include "RTT_Debug.h"
#include "systick.h"

#define __SCL_PORT GPIO_GetPeriphAddr(SHT20_SCL_PIN)
#define __SCL_PIN GPIO_GetPinAddr(SHT20_SCL_PIN)
#define __SDA_PORT GPIO_GetPeriphAddr(SHT20_SDA_PIN)
#define __SDA_PIN GPIO_GetPinAddr(SHT20_SDA_PIN)

#define __SDA_High() gpio_bit_set(__SDA_PORT, __SDA_PIN)
#define __SDA_Low() gpio_bit_reset(__SDA_PORT, __SDA_PIN)
#define __SCL_High() gpio_bit_set(__SCL_PORT, __SCL_PIN)
#define __SCL_Low() gpio_bit_reset(__SCL_PORT, __SCL_PIN)

#define __CMD_START_READ 0x81
#define __CMD_START_WRITE 0x80

#define __CMD_MEASURE_TEMPERATURE_HOLD 0xE3
#define __CMD_MEASURE_HUMIDITY_HOLD 0xE5
#define __CMD_MEASURE_TEMPERATURE_NO_HOLD 0xF3
#define __CMD_MEASURE_HUMIDITY_NO_HOLD 0xF5
#define __CMD_READ_REGISTER 0xE7
#define __CMD_WRITE_REGISTER 0xE6
#define __CMD_SOFT_RESET 0xFE

#define __CRC_POLYNOMIAL 0x131
// #define __InterpretTemp(msb, lsb) ((float)(((((uint16_t)msb) << 8) | lsb) >> 2) * 175.72f / 65536.0f - 46.85f)
// #define __InterpretHumidity(msb, lsb) ((float)(((((uint16_t)msb) << 8) | lsb) >> 2) * 125.0f / 65536.0f - 6.0f)

typedef enum __SHT20_STATE __SHT20_STATE_t;
enum __SHT20_STATE
{
    __STATE_IDLE,
    __STATE_CMD,
    __STATE_WAIT,
    __STATE_READ,
    __STATE_CRC,
    __STATE_INTERPRET
};

static __SHT20_STATE_t __state = __STATE_CMD;

static bool __isMeasuringTemp = true;

static float __lastTemp     = 0.0f;
static float __lastHumidity = 0.0f;

static uint64_t __lastTempUpdatedAt     = 0;
static uint64_t __lastHumidityUpdatedAt = 0;

static bool __tempUpdated     = false;
static bool __humidityUpdated = false;

void __SHT20_GPIO_Config()
{
    /* SCL pin, 0.4 MHz at max */
    rcu_periph_clock_enable(GPIO_GetRcuPeriph(SHT20_SCL_PIN));
    gpio_init(__SCL_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, __SCL_PIN);

    /* SDA pin*/
    rcu_periph_clock_enable(GPIO_GetRcuPeriph(SHT20_SDA_PIN));
    gpio_init(__SDA_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, __SDA_PIN);
}

void SHT20_Init(void)
{
    __SHT20_GPIO_Config();
    __lastTemp     = NAN;
    __lastHumidity = NAN;
}

void SHT20_Enable(void)
{
    rcu_periph_clock_enable(GPIO_GetRcuPeriph(SHT20_SCL_PIN));
    gpio_init(__SCL_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, __SCL_PIN);
    rcu_periph_clock_enable(GPIO_GetRcuPeriph(SHT20_SDA_PIN));
    gpio_init(__SDA_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, __SDA_PIN);
    __state           = __STATE_CMD;
    __isMeasuringTemp = true;
}

void SHT20_Disable(void)
{
    rcu_periph_clock_disable(GPIO_GetRcuPeriph(SHT20_SCL_PIN));
    rcu_periph_clock_disable(GPIO_GetRcuPeriph(SHT20_SDA_PIN));
    gpio_init(__SCL_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, __SCL_PIN);
    gpio_init(__SDA_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, __SDA_PIN);
    __state = __STATE_IDLE;
}

void __forceinline __Start(void)
{
    __SDA_High();
    __SCL_High();
    DWT_Delay_us(20);
    __SDA_Low();
    DWT_Delay_us(20);
    __SCL_Low();
}

void __forceinline __Stop(void)
{
    __SDA_Low();
    DWT_Delay_us(10);
    __SCL_High();
    DWT_Delay_us(20);
    __SDA_High();
    DWT_Delay_us(20);
}

void __forceinline __SendAck(bool isAck)
{
    if(isAck)
    {
        __SDA_Low();
    }
    else
    {
        __SDA_High();
    }
    DWT_Delay_us(20);
    __SCL_High();
    DWT_Delay_us(20);
    __SCL_Low();
    DWT_Delay_us(20);
}

bool __forceinline __RecvAck()
{
    uint8_t timeout = 0;
    __SDA_High(); // Release SDA for ACK
    DWT_Delay_us(4);
    __SCL_High();
    while(gpio_input_bit_get(__SDA_PORT, __SDA_PIN))
    {
        if(++timeout > SHT20_ACK_TIMEOUT)
        {
            return false;
        }
    }
    __SCL_Low();
    return true;
}

bool __SHT20_WriteByte(uint8_t byte)
{
    for(int i = 0; i < 8; i++)
    {
        if(byte & 0x80)
        {
            __SDA_High();
        }
        else
        {
            __SDA_Low();
        }
        DWT_Delay_us(20);
        __SCL_High();
        DWT_Delay_us(20);
        __SCL_Low();
        DWT_Delay_us(20);
        byte <<= 1;
    }

    return __RecvAck();
}

uint8_t __SHT20_ReadByte(bool isCRC)
{
    uint8_t byte = 0;

    __SDA_High();
    for(int i = 0; i < 8; i++)
    {
        DWT_Delay_us(20);
        __SCL_High();
        DWT_Delay_us(20);
        byte <<= 1;
        if(gpio_input_bit_get(__SDA_PORT, __SDA_PIN))
        {
            byte |= 0x01;
        }
        DWT_Delay_us(20);
        __SCL_Low();
        DWT_Delay_us(20);
    }

    __SendAck(!isCRC);

    return byte;
}

bool __CRC_Check(uint8_t MSB, uint8_t LSB, uint8_t checksum)
{
    uint8_t crc = 0;
    uint8_t i;

    crc ^= MSB;
    for(i = 8; i > 0; --i)
    {
        if(crc & 0x80)
            crc = (crc << 1) ^ __CRC_POLYNOMIAL;
        else
            crc = (crc << 1);
    }
    crc ^= LSB;
    for(i = 8; i > 0; --i)
    {
        if(crc & 0x80)
            crc = (crc << 1) ^ __CRC_POLYNOMIAL;
        else
            crc = (crc << 1);
    }

    if(crc != checksum)
        return false;
    else
        return true;
}

static __forceinline float __InterpretTemp(uint8_t msb, uint8_t lsb)
{
    return ((float)((((uint16_t)msb << 8) | lsb) & 0xFFFC) * 175.72f / 65536.0f) - 46.85f;
}

static __forceinline float __InterpretHumidity(uint8_t msb, uint8_t lsb)
{
    return ((float)((((uint16_t)msb << 8) | lsb) & 0xFFFC) * 125.0f / 65536.0f) - 6.0f;
}

float SHT20_test(void)
{
    __SCL_High();
    __Start();
    if(!__SHT20_WriteByte(__CMD_START_WRITE))
    {
        DBG_log("Write cmd start write failed.\n");
        return NAN;
    }
    if(!__SHT20_WriteByte(__CMD_MEASURE_HUMIDITY_NO_HOLD))
    {
        DBG_log("Write cmd read temp failed.\n");
        return NAN;
    }
    DWT_Delay_us(20);
    __Start();
    while(!__SHT20_WriteByte(__CMD_START_READ))
    {
        DBG_log("data not ready.\n");
        __Stop();
        DWT_Delay_us(20);
        __Start();
    }
    //__SCL_High();
    // while(gpio_input_bit_get(__SCL_PORT, __SCL_PIN) == RESET);
    uint8_t temp_msb = __SHT20_ReadByte(false);
    uint8_t temp_lsb = __SHT20_ReadByte(false);
    uint8_t temp_crc = __SHT20_ReadByte(true);
    __Stop();

    /* crc check */
    if(!__CRC_Check(temp_msb, temp_lsb, temp_crc))
    {
        DBG_log("CRC check failed.\n");
        return NAN;
    }

    int32_t temp = __InterpretHumidity(temp_msb, temp_lsb) * 100.0f;

    DBG_log("msb: 0x%08x\n", temp_msb);
    DBG_log("lsb: 0x%08x\n", temp_lsb);
    DBG_log("temp: %d.%02d C\n", temp / 100, temp % 100);
    return temp / 100.0f;
}

void SHT20_Run(void)
{
    static uint8_t msb, lsb, crc;
    switch(__state)
    {
        case __STATE_IDLE:
            break;
        case __STATE_CMD:
            __SCL_High();
            __Start();
            if(!__SHT20_WriteByte(__CMD_START_WRITE))
            {
                DBG_log("Write cmd start write failed.\n");
                return;
            }
            if(!__SHT20_WriteByte(__isMeasuringTemp ? __CMD_MEASURE_TEMPERATURE_NO_HOLD
                                                    : __CMD_MEASURE_HUMIDITY_NO_HOLD))
            {
                DBG_log("Write cmd read %s failed.\n", __isMeasuringTemp ? "temp" : "humidity");
                return;
            }
            DWT_Delay_us(20);
            __state = __STATE_WAIT;
            break;
        case __STATE_WAIT:
            __Start();
            if(!__SHT20_WriteByte(__CMD_START_READ))
            {
                __Stop();
                DWT_Delay_us(20);
            }
            else
            {
                __state = __STATE_READ;
            }
            break;
        case __STATE_READ:
            msb = __SHT20_ReadByte(false);
            lsb = __SHT20_ReadByte(false);
            crc = __SHT20_ReadByte(true);
            __Stop();
            __state = __STATE_CRC;
            break;
        case __STATE_CRC:
            if(__CRC_Check(msb, lsb, crc))
            {
                // DBG_log("[SHT20 raw] msb: 0x%08x, lsb: 0x%08x, crc: 0x%08x\n", msb, lsb, crc);
                __state = __STATE_INTERPRET;
            }
            else
            {
                DBG_log("[SHT20 Error] CRC check failed.\n");
                __isMeasuringTemp = !__isMeasuringTemp;
                __state           = __STATE_CMD;
            }
            break;
        case __STATE_INTERPRET:
            if(__isMeasuringTemp)
            {
                __lastTemp          = __InterpretTemp(msb, lsb);
                __lastTempUpdatedAt = SYSTICK_GetSysRunTime();
                __tempUpdated       = true;
            }
            else
            {
                __lastHumidity          = __InterpretHumidity(msb, lsb);
                __lastHumidityUpdatedAt = SYSTICK_GetSysRunTime();
                __humidityUpdated       = true;
            }
            __isMeasuringTemp = !__isMeasuringTemp;
            msb = lsb = 0;
            crc       = 0xff;
            __state   = __STATE_CMD;
            break;
        default:
            break;
    }
}

float SHT20_GetTemp(bool handleError)
{
    if(!handleError)
    {
        return __lastTemp;
    }
    else
    {
        if(SYSTICK_GetSysRunTime() - __lastTempUpdatedAt > SHT20_ERROR_TIMEOUT_MS)
        {
            DBG_log("[SHT20 Error] Temperature reading timeout.\n");
            return NAN;
        }
        else if(!__tempUpdated)
        {
            return NAN;
        }
        else
        {
            __tempUpdated = false;
            return __lastTemp;
        }
    }
}

float SHT20_GetHumidity(bool handleError)
{
    if(!handleError)
    {
        return __lastHumidity;
    }
    else
    {
        if(SYSTICK_GetSysRunTime() - __lastHumidityUpdatedAt > SHT20_ERROR_TIMEOUT_MS)
        {
            DBG_log("[SHT20 Error] Humidity reading timeout.\n");
            return NAN;
        }
        else if(!__humidityUpdated)
        {
            return NAN;
        }
        else
        {
            __humidityUpdated = false;
            return __lastHumidity;
        }
    }
}
