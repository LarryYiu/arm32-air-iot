#include "wifi_app.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "at.h"
#include "rtc.h"
#include "RTT_Debug.h"
#include "sht20_driver.h"
#include "hk_a5_driver.h"
#include "led_driver.h"
#include "storage_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define DEBUG_PRINTING true

#ifndef WIFI_USE_SMART_CONFIG
#define WIFI_USE_SMART_CONFIG false
#endif

// clang-format off
#define __AT_CWJAP_SSID_PWD_CMD "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PWD "\"\r\n"
#define __AT_SNTPCFG_CMD "AT+CIPSNTPCFG=1," UTC(8) ",\"" SNTP_SERVER_1 "\",\"" SNTP_SERVER_2 "\"\r\n"
#define __AT_SUBSCRIBE_CMD "AT+MQTTSUB=0,\"$sys/" MQTT_PRODUCT_ID "/" MQTT_DEVICE_NAME "/thing/property/set\",0\r\n"
// clang-format on
#if WIFI_USE_SMART_CONFIG

static void __PrintCWStateResponse()
{
    //+CWSTATE:<state>,<"ssid">
    char* ptr         = strstr(AT_GetResponseSnapshot(), "+CWSTATE:");
    uint8_t stateCode = atoi(ptr + 9); // Skip "+CWSTATE:" to get the state code
    DBG_log("[WIFI] CWSTATE: %hhu\n", stateCode);

    char* ssidStart = strchr(ptr, '\"');
    if(ssidStart)
    {
        ssidStart++; // Move past the opening quote
        char* ssidEnd = strchr(ssidStart, '\"');
        if(ssidEnd && ssidEnd > ssidStart)
        {
            size_t ssidLength = ssidEnd - ssidStart;
            char SSID[32];
            memset(SSID, 0, 32);
            strncpy(SSID, ssidStart, ssidLength);
            SSID[ssidLength] = '\0';
            DBG_log("[WIFI] Connected SSID: %s\n", SSID);
        }
    }
}
#endif

#if WIFI_USE_SMART_CONFIG
typedef enum
{
    AT_START_SMART_CONFIG = 0,
    AT_SMART_CONFIG_DELAY,
    AT_REPLY_APP_DELAY,
    AT_STOP_SMART_CONFIG,
    AT_WIFI_CONNECTION_CHECK,
    AT_SMART_CONFIG_IDLE
} AT_CONFIG_WIFI_CMD_INDEX_t;

static const AT_Cmd_t __AT_CONFIG_WIFI_CMD[] = {
    [AT_START_SMART_CONFIG] = {.cmd = "AT+CWSTARTSMART\r\n", .desiredResponse = "OK", .timeoutMs = 1000, .maxRetry = 3},
    [AT_SMART_CONFIG_DELAY] = {.cmd = NULL, .desiredResponse = "GOT IP", .timeoutMs = 30000, .maxRetry = 0},
    [AT_REPLY_APP_DELAY]    = {.cmd = NULL, .desiredResponse = NULL, .timeoutMs = 6000, .maxRetry = 0},
    [AT_STOP_SMART_CONFIG]  = {.cmd = "AT+CWSTOPSMART\r\n", .desiredResponse = "OK", .timeoutMs = 1000, .maxRetry = 3},
    [AT_WIFI_CONNECTION_CHECK] = {.cmd = "AT+CWSTATE?\r\n", .desiredResponse = ":2", .timeoutMs = 1000, .maxRetry = 3}};

#define __AT_CONFIG_WIFI_CMD_COUNT (sizeof(__AT_CONFIG_WIFI_CMD) / sizeof(__AT_CONFIG_WIFI_CMD[0]))
static COMM_STATE_t _WIFI_SmartConfig(void)
{
    COMM_STATE_t commState;
    uint8_t currentCmdIndex       = 0;
    uint64_t smartConfigStartTime = xTaskGetTickCount();

    while(currentCmdIndex < __AT_CONFIG_WIFI_CMD_COUNT)
    {
        if(xTaskGetTickCount() - smartConfigStartTime > SMART_CONFIG_WAITING_TIME_MS)
        {
            DBG_log("[WIFI Smart] Smart Config Timeout\n");
            goto STOP_SMART_CONFIG;
        }
        commState = AT_CmdHandler(
            __AT_CONFIG_WIFI_CMD[currentCmdIndex].cmd, __AT_CONFIG_WIFI_CMD[currentCmdIndex].desiredResponse,
            &__AT_CONFIG_WIFI_CMD[currentCmdIndex].timeoutMs, &__AT_CONFIG_WIFI_CMD[currentCmdIndex].maxRetry);
        if(commState == COMM_STATE_OK || commState == COMM_STATE_DELAY_DONE)
        {
            AT_ClearResponseSnapshot();
            currentCmdIndex++;
        }
        else
        {
            AT_ClearResponseSnapshot();
            goto STOP_SMART_CONFIG;
        }
    }

STOP_SMART_CONFIG:
    while(AT_CmdHandler(__AT_CONFIG_WIFI_CMD[AT_STOP_SMART_CONFIG].cmd,
                        __AT_CONFIG_WIFI_CMD[AT_STOP_SMART_CONFIG].desiredResponse,
                        &__AT_CONFIG_WIFI_CMD[AT_STOP_SMART_CONFIG].timeoutMs,
                        &__AT_CONFIG_WIFI_CMD[AT_STOP_SMART_CONFIG].maxRetry) != COMM_STATE_OK)
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI Smart] Stop smart config failed, retrying...\n");
#endif
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    return COMM_STATE_FAILED_TIMER; // Return a failure state to indicate that the smart config process has ended
}
#else
typedef enum
{
    AT_CWJAP_SSID_PWD,
    AT_CWJAP_DELAY,
} AT_CONNECT_INTERNET_CMD_INDEX_t;

static const AT_Cmd_t __AT_CONNECT_INTERNET_CMD[] = {
    [AT_CWJAP_SSID_PWD] = {.cmd             = __AT_CWJAP_SSID_PWD_CMD,
                           .desiredResponse = "GOT IP",
                           .timeoutMs       = 15000,
                           .maxRetry        = 3},
    [AT_CWJAP_DELAY]    = {.cmd = NULL, .desiredResponse = NULL, .timeoutMs = 2000, .maxRetry = 0},
};

#define __AT_CONNECT_INTERNET_CMD_COUNT (sizeof(__AT_CONNECT_INTERNET_CMD) / sizeof(__AT_CONNECT_INTERNET_CMD[0]))

static COMM_STATE_t __WIFI_ConnectInternet(void)
{
    uint8_t currentCmdIndex = 0;
    COMM_STATE_t commState;
    while(currentCmdIndex < __AT_CONNECT_INTERNET_CMD_COUNT)
    {
        commState = AT_CmdHandler(__AT_CONNECT_INTERNET_CMD[currentCmdIndex].cmd,
                                  __AT_CONNECT_INTERNET_CMD[currentCmdIndex].desiredResponse,
                                  &__AT_CONNECT_INTERNET_CMD[currentCmdIndex].timeoutMs,
                                  &__AT_CONNECT_INTERNET_CMD[currentCmdIndex].maxRetry);

        if(commState == COMM_STATE_OK || commState == COMM_STATE_DELAY_DONE)
        {
            AT_ClearResponseSnapshot();
            currentCmdIndex++;
        }
        else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
        {
            AT_ClearResponseSnapshot();
            vTaskDelay(pdMS_TO_TICKS(300)); // Delay for 300 ms before retrying
        }
    }
    return COMM_STATE_OK;
}
#endif

/* OTA Handler Here */
/* OTA Handler Ends */

typedef enum
{
    AT_MQTTUSERCFG = 0,
    AT_MQTTCONN,
    AT_SNTPCFG,
    AT_SUBSCRIBE,
} AT_CONNECT_MQTT_SERVER_CMD_INDEX_t;

static const AT_Cmd_t __AT_MQTT_CONNECT_SERVER_CMD[] = {
    [AT_MQTTUSERCFG] =
        {.cmd = MQTT_USERCFG_CMD("1", "0", "0"),
         //  .cmd = "AT+MQTTUSERCFG=0,1,\"GD32Board\",\"4m3RoDJR8n\",\"version=2018-10-31&res=products%2F4m3RoDJR8n%"
         //         "2Fdevices%2FGD32Board&et=1830268800&method=md5&sign=qIQ8T1heqVLOZ23gunAFjg%3D%3D\",0,0,\"\"\r\n",
         .desiredResponse = "OK",
         .timeoutMs       = 300,
         .maxRetry        = 3},
    [AT_MQTTCONN]  = {.cmd = MQTT_CONN_CMD("1"), .desiredResponse = "OK", .timeoutMs = 5000, .maxRetry = 3},
    [AT_SNTPCFG]   = {.cmd = __AT_SNTPCFG_CMD, .desiredResponse = "OK", .timeoutMs = 500, .maxRetry = 0},
    [AT_SUBSCRIBE] = {.cmd = __AT_SUBSCRIBE_CMD, .desiredResponse = "OK", .timeoutMs = 500, .maxRetry = 3}};

#define __AT_CONNECT_MQTT_SERVER_CMD_COUNT \
    (sizeof(__AT_MQTT_CONNECT_SERVER_CMD) / sizeof(__AT_MQTT_CONNECT_SERVER_CMD[0]))

static COMM_STATE_t __WIFI_ConnectMqttServer(void)
{
    uint8_t currentCmdIndex = 0;
    COMM_STATE_t commState;
    while(currentCmdIndex < __AT_CONNECT_MQTT_SERVER_CMD_COUNT)
    {
        commState = AT_CmdHandler(__AT_MQTT_CONNECT_SERVER_CMD[currentCmdIndex].cmd,
                                  __AT_MQTT_CONNECT_SERVER_CMD[currentCmdIndex].desiredResponse,
                                  &__AT_MQTT_CONNECT_SERVER_CMD[currentCmdIndex].timeoutMs,
                                  &__AT_MQTT_CONNECT_SERVER_CMD[currentCmdIndex].maxRetry);

        if(commState == COMM_STATE_OK || commState == COMM_STATE_DELAY_DONE)
        {
            AT_ClearResponseSnapshot();
            currentCmdIndex++;
        }
        else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
        {
            AT_ClearResponseSnapshot();
            vTaskDelay(pdMS_TO_TICKS(300)); // Delay for 300 ms before retrying
        }
    }
#if DEBUG_PRINTING
    DBG_log("[WIFI] MQTT Connection done\n");
#endif
    return COMM_STATE_OK;
}

typedef struct
{
    char id[20];
    uint8_t ledVal;
} SUB_TOPIC_DATA_t;

typedef enum
{
    AT_LISTEN_SUB,
    AT_REPLY_SUB,
    AT_SNTPTIME,
    AT_MQTT_PUB_SENSOR_DATA
} AT_BUSINESS_CMD_INDEX_t;

static const AT_Cmd_t __AT_BUSINESS_CMD[] = {
    [AT_LISTEN_SUB] = {.cmd = NULL, .desiredResponse = "thing/property/set", .timeoutMs = 1, .maxRetry = 0},
    [AT_REPLY_SUB] =
        {.cmd = "AT+MQTTPUB=0,\"$sys/4m3RoDJR8n/GD32Air/thing/property/"
                "set_reply\",\"{\\\"id\\\":\\\"%s\\\"\\,\\\"code\\\": 200\\,\\\"msg\\\":\\\"success\\\"}\",0,0\r\n",
         .desiredResponse = "OK",
         .timeoutMs       = 3000,
         .maxRetry        = 2},
    [AT_SNTPTIME] = {.cmd = "AT+CIPSNTPTIME?\r\n", .desiredResponse = "OK", .timeoutMs = 500, .maxRetry = 0},
    [AT_MQTT_PUB_SENSOR_DATA] = {
        .cmd =
            "AT+MQTTPUB=0,\"$sys/4m3RoDJR8n/GD32Air/thing/property/post\",\"{\\\"id\\\":\\\"123\\\"\\,\\\"params\\\": "
            "{\\\"IotPropTemperature\\\": {\\\"value\\\":%.1f}\\,\\\"IotPropHumidity\\\": "
            "{\\\"value\\\":%d}\\,\\\"IotPropPm25\\\": {\\\"value\\\":%d}}}\",0,0\r\n",
        // the cmd is going into the sprintf, so extra backslash is needed to escape the double quote and comma
        .desiredResponse = "OK",
        .timeoutMs       = 500,
        .maxRetry        = 3}};

static bool __HandleSubTopicPayload(const char* payload, SUB_TOPIC_DATA_t* subTopicData)
{
    // parse id
    char* idStart = strstr(payload, "\"id\":\"");
    if(idStart)
    {
        idStart += strlen("\"id\":\"");
        char* idEnd = strchr(idStart, '\"');
        if(idEnd && idEnd > idStart)
        {
            size_t idLength = idEnd - idStart;
            char idBuffer[20];
            if(idLength < sizeof(idBuffer))
            {
                strncpy(subTopicData->id, idStart, idLength);
                subTopicData->id[idLength] = '\0';
                DBG_log("[WIFI] Sub ID: %s\n", subTopicData->id);
                // send reply
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    // parse param
    char* ledParamPtr = strstr(payload, "IotPropLeds\":");
    if(ledParamPtr)
    {
        ledParamPtr += strlen("IotPropLeds\":");
        uint8_t ledVal = atoi(ledParamPtr);
        if(ledVal <= 7)
            subTopicData->ledVal = ledVal;
        else
            return false;
    }
    else
    {
        return false;
    }
    return true;
}

static void __UpdateRtcTimeBySntpResponse(const char* sntpResponse)
{
    RTC_Time_t time;
    time.month = sntpResponse[17] + sntpResponse[18] + sntpResponse[19] - 200;
    switch(time.month)
    {
        case 81: // Jan
            time.month = 1;
            break;
        case 69: // Feb
            time.month = 2;
            break;
        case 88: // Mar
            time.month = 3;
            break;
        case 91: // Apr
            time.month = 4;
            break;
        case 95: // May
            time.month = 5;
            break;
        case 101: // Jun
            time.month = 6;
            break;
        case 99: // Jul
            time.month = 7;
            break;
        case 85: // Aug
            time.month = 8;
            break;
        case 96: // Sep
            time.month = 9;
            break;
        case 94: // Oct
            time.month = 10;
            break;
        case 107: // Nov
            time.month = 11;
            break;
        case 68: // Dec
            time.month = 12;
            break;
        default:
            time.month = 0;
            break;
    }

    time.day    = (sntpResponse[20] - '0') * 10 + (sntpResponse[21] - '0');
    time.hour   = (sntpResponse[23] - '0') * 10 + (sntpResponse[24] - '0');
    time.minute = (sntpResponse[26] - '0') * 10 + (sntpResponse[27] - '0');
    time.second = (sntpResponse[29] - '0') * 10 + (sntpResponse[30] - '0');
    time.year   = (sntpResponse[32] - '0') * 1000 + (sntpResponse[33] - '0') * 100 + (sntpResponse[34] - '0') * 10 +
                  (sntpResponse[35] - '0');
    RTC_SetTime(&time);
}

static COMM_STATE_t __WIFI_HandleSubscription(void)
{
    COMM_STATE_t subListenCommState =
        AT_CmdHandler(__AT_BUSINESS_CMD[AT_LISTEN_SUB].cmd, __AT_BUSINESS_CMD[AT_LISTEN_SUB].desiredResponse,
                      &__AT_BUSINESS_CMD[AT_LISTEN_SUB].timeoutMs, &__AT_BUSINESS_CMD[AT_LISTEN_SUB].maxRetry);
    if(subListenCommState == COMM_STATE_OK)
    {
        SUB_TOPIC_DATA_t subTopicData;
#if DEBUG_PRINTING
        DBG_log("[WIFI] Recv Sub Topic\n");
#endif
        if(!__HandleSubTopicPayload(AT_GetResponseSnapshot(), &subTopicData))
        {
#if DEBUG_PRINTING
            DBG_log("[WIFI] Sub Topic Payload Parsing Failed\n");
#endif
            AT_ClearResponseSnapshot();
            return COMM_STATE_FAILED_RESPONSE;
        }
        AT_ClearResponseSnapshot();
#if DEBUG_PRINTING
        DBG_log("[WIFI] parsed data: id %s, led %d\n", subTopicData.id, subTopicData.ledVal);
#endif
        if(subTopicData.ledVal >= 1) // handle the LED state based on the received value
        {
            LED_SetState(0, SET);
        }
        else
        {
            LED_SetState(0, RESET);
        }

        // respond to the subscription message
        char cmdStrBuf[256] = {0};
        snprintf(cmdStrBuf, 256, __AT_BUSINESS_CMD[AT_REPLY_SUB].cmd, subTopicData.id);
        COMM_STATE_t replyCommState =
            AT_CmdHandler(cmdStrBuf, __AT_BUSINESS_CMD[AT_REPLY_SUB].desiredResponse,
                          &__AT_BUSINESS_CMD[AT_REPLY_SUB].timeoutMs, &__AT_BUSINESS_CMD[AT_REPLY_SUB].maxRetry);
        if(replyCommState == COMM_STATE_OK)
        {
#if DEBUG_PRINTING
            DBG_log("[WIFI] Reply Sub Topic Success\n");
#endif
            AT_ClearResponseSnapshot();
            return COMM_STATE_OK;
        }
        else if(replyCommState == COMM_STATE_FAILED_TIMER || replyCommState == COMM_STATE_FAILED_RESPONSE)
        {
#if DEBUG_PRINTING
            DBG_log("[WIFI] Reply Sub Topic Failed\n");
#endif
            AT_ClearResponseSnapshot();
            return replyCommState;
        }
        return COMM_STATE_OK;
    }
    else // did not get the subscription message
    {
        AT_ClearResponseSnapshot();
        return subListenCommState;
    }
}

static COMM_STATE_t __WIFI_HandleSntpTimeUpdate(void)
{
    COMM_STATE_t sntpCommState =
        AT_CmdHandler(__AT_BUSINESS_CMD[AT_SNTPTIME].cmd, __AT_BUSINESS_CMD[AT_SNTPTIME].desiredResponse,
                      &__AT_BUSINESS_CMD[AT_SNTPTIME].timeoutMs, &__AT_BUSINESS_CMD[AT_SNTPTIME].maxRetry);
    if(sntpCommState == COMM_STATE_OK)
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] Time Updated\n");
#endif
        __UpdateRtcTimeBySntpResponse(AT_GetResponseSnapshot());
        AT_ClearResponseSnapshot();
        return COMM_STATE_OK;
    }
    else if(sntpCommState == COMM_STATE_FAILED_TIMER || sntpCommState == COMM_STATE_FAILED_RESPONSE)
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] Time Update Failed\n");
#endif
        AT_ClearResponseSnapshot();
        return sntpCommState;
    }
    return COMM_STATE_OK;
}

static COMM_STATE_t __WIFI_HandleSensorDataUpload(void)
{
    char cmdStrBuf[256];
    // check if sensor data is updated
    if(SHT20_IsUpdated() || HK_A5_IsPm25ValueUpdated())
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] New Sensor Data Detected\n");
#endif
        memset(cmdStrBuf, 0, 256);
        sprintf(cmdStrBuf, __AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].cmd, SHT20_GetTemp(false),
                (int32_t)SHT20_GetHumidity(false), (int32_t)HK_A5_GetPm25Value());
    }
    else
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] Sensor Data not detected\n");
#endif
        return COMM_STATE_FAILED_HARDWARE;
    }

    COMM_STATE_t sensorDataCommState = AT_CmdHandler(
        cmdStrBuf, __AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].desiredResponse,
        &__AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].timeoutMs, &__AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].maxRetry);
    if(sensorDataCommState == COMM_STATE_OK)
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] Sensor Data Uploaded, waiting on next upload period\n");
#endif
        AT_ClearResponseSnapshot();
        return COMM_STATE_OK;
    }
    else if(sensorDataCommState == COMM_STATE_FAILED_TIMER || sensorDataCommState == COMM_STATE_FAILED_RESPONSE)
    {
#if DEBUG_PRINTING
        DBG_log("[WIFI] Sensor Data Upload Failed\n");
#endif
        AT_ClearResponseSnapshot();
        return sensorDataCommState;
    }
    return COMM_STATE_OK;
}

void __WIFI_RTOS_Task(void* arg)
{
    COMM_STATE_t commState;
    do
    {
        commState = AT_Init();
        if(commState > COMM_STATE_DELAY_DONE)
        {
            DBG_log("[WIFI] AT Init Failed: [%d], Retrying...\n", commState);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while(commState > COMM_STATE_DELAY_DONE);

#if DEBUG_PRINTING
    DBG_log("[WIFI] WIFI module init done\n");
#endif

    while(__WIFI_ConnectInternet() > COMM_STATE_DELAY_DONE)
    {
        DBG_log("[WIFI] Internet Connection Failed, Retrying...\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

#if DEBUG_PRINTING
    DBG_log("[WIFI] Wifi Connection done\n");
#endif

    while(__WIFI_ConnectMqttServer() > COMM_STATE_DELAY_DONE)
    {
        DBG_log("[WIFI] MQTT Server Connection Failed, Retrying...\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

#if DEBUG_PRINTING
    DBG_log("[WIFI] MQTT Connection done\n");
#endif

    uint64_t lastSntpUpdateTime   = 0;
    uint64_t lastSensorUploadTime = 0;
    uint64_t currentTime          = 0;

    while(1)
    {
        __WIFI_HandleSubscription();
        currentTime = xTaskGetTickCount();
        if(currentTime - lastSntpUpdateTime >= SNTP_REQ_PERIOD_MS)
        {
            commState = __WIFI_HandleSntpTimeUpdate();
            if(commState == COMM_STATE_OK)
            {
                lastSntpUpdateTime = currentTime;
            }
        }
        if(currentTime - lastSensorUploadTime >= MQTT_DAT_UPLOAD_PERIOD_MS)
        {
            commState = __WIFI_HandleSensorDataUpload();
            if(commState == COMM_STATE_OK || commState == COMM_STATE_FAILED_HARDWARE)
            {
                lastSensorUploadTime = currentTime;
            }
        }
    }
}

void WIFI_RTOS_Init(void)
{
    BaseType_t ret = xTaskCreate(__WIFI_RTOS_Task, "wifi", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);

    configASSERT(ret == pdPASS);
}
