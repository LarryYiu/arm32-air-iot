#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "gd32f30x.h"
// clang-format off

/*LED CONFIGURATION START*/

#ifndef NULL
#define NULL ((void*)0)
#endif

#define LED_GPIO_LOOKUP \
    GPIO_PIN(A,1)

#define LED_GPIO_FREQENCY GPIO_OSPEED_50MHZ

/*LED CONFIGURATION END*/

/*KEY CONFIGURATION START*/

#define KEY_LOOK_UP \
    GPIO_PIN(C,4) \

#define KEY_GPIO_FREQENCY GPIO_OSPEED_2MHZ
#define KEY_RELEASE_TRIGGER_DEFAULT true
#define KEY_LONG_PRESS_THRESHOLD_MS 1000
#define KEY_CONTINUOUS_PRESS_THRESHOLD_MS 200
#define KEY_DEBOUNCE_TIME_MS 10

/*KEY CONFIGURATION END*/

/* SHT20 CONFIGURATION START*/
#define SHT20_SCL_PIN GPIO_PIN(A,5)
#define SHT20_SDA_PIN GPIO_PIN(A,4)
#define SHT20_ADDRESS 0x80
#define SHT20_ERROR_TIMEOUT_MS 5000
// clang-format on
#define len(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif // __CONFIG_H__

// void SHT20_Run(void)
// {
//     static uint8_t msb, lsb, crc;
//     switch(__state)
//     {
//         case __STATE_IDLE:
//             break;
//         case __STATE_CMD:
//             __SCL_High();
//             __Start();
//             if(!__SHT20_WriteByte(__CMD_START_WRITE))
//             {
//                 DBG_log("[SHT20 Error] Write cmd START.\n");
//                 __isMeasuringTemp = !__isMeasuringTemp;
//             }
//             if(!__SHT20_WriteByte(__isMeasuringTemp ? __CMD_MEASURE_TEMPERATURE_HOLD : __CMD_MEASURE_HUMIDITY_HOLD))
//             {
//                 DBG_log("[SHT20 Error] Write cmd READ.\n");
//                 __isMeasuringTemp = !__isMeasuringTemp;
//             }
//             DWT_Delay_us(20);
//             __Start();
//             if(!__SHT20_WriteByte(__CMD_START_READ))
//             {
//                 DBG_log("[SHT20 Error] Write cmd START READ.\n");
//                 __isMeasuringTemp = !__isMeasuringTemp;
//             }
//             __state = __STATE_WAIT;
//             break;
//         case __STATE_WAIT:
//             if(gpio_input_bit_get(__SCL_PORT, __SCL_PIN))
//                 __state = __STATE_READ;
//             break;
//         case __STATE_READ:
//             __Start();
//             __SHT20_WriteByte(__CMD_START_READ);
//             msb     = __SHT20_ReadByte(false);
//             lsb     = __SHT20_ReadByte(false);
//             crc     = __SHT20_ReadByte(true);
//             __state = __STATE_CRC;
//             break;
//         case __STATE_CRC:
//             if(__CRC_Check(msb, lsb, crc))
//             {
//                 __state = __STATE_INTERPRET;
//             }
//             else
//             {
//                 DBG_log("[SHT20 Error] CRC check failed.\n");
//                 __isMeasuringTemp = !__isMeasuringTemp;
//                 __state           = __STATE_CMD;
//             }
//             break;
//         case __STATE_INTERPRET:
//             if(__isMeasuringTemp)
//             {
//                 __lastTemp          = __InterpretTemp(msb, lsb);
//                 __lastTempUpdatedAt = SYSTICK_GetSysRunTime();
//                 __tempUpdated       = true;
//             }
//             else
//             {
//                 __lastHumidity          = __InterpretHumidity(msb, lsb);
//                 __lastHumidityUpdatedAt = SYSTICK_GetSysRunTime();
//                 __humidityUpdated       = true;
//             }
//             __isMeasuringTemp = !__isMeasuringTemp;
//             msb = lsb = 0;
//             crc       = 0xff;
//             __state   = __STATE_CMD;
//             break;
//         default:
//             break;
//     }
// }

// float SHT20_GetTemp(bool handleError)
// {
//     if(!handleError)
//     {
//         return __lastTemp;
//     }
//     else
//     {
//         if(SYSTICK_GetSysRunTime() - __lastTempUpdatedAt > SHT20_ERROR_TIMEOUT_MS)
//         {
//             DBG_log("[SHT20 Error] Temperature reading timeout.\n");
//             return NAN;
//         }
//         else if(!__tempUpdated)
//         {
//             return NAN;
//         }
//         else
//         {
//             __tempUpdated = false;
//             return __lastTemp;
//         }
//     }
// }

// float SHT20_GetHumidity(bool handleError)
// {
//     if(!handleError)
//     {
//         return __lastHumidity;
//     }
//     else
//     {
//         if(SYSTICK_GetSysRunTime() - __lastHumidityUpdatedAt > SHT20_ERROR_TIMEOUT_MS)
//         {
//             DBG_log("[SHT20 Error] Humidity reading timeout.\n");
//             return NAN;
//         }
//         else if(!__humidityUpdated)
//         {
//             return NAN;
//         }
//         else
//         {
//             __humidityUpdated = false;
//             return __lastHumidity;
//         }
//     }
// }
