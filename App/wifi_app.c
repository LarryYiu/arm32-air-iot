#include "wifi_app.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "at.h"
#include "systick.h"
#include "rtc.h"
#include "RTT_Debug.h"
#include "sht20_driver.h"
#include "hk_a5_driver.h"
#include "led_driver.h"
#include "storage_app.h"

#define DEBUG_PRINTING true

#ifndef WIFI_USE_SMART_CONFIG
#define WIFI_USE_SMART_CONFIG false
#endif

#if WIFI_USE_SMART_CONFIG
static uint64_t __smartConfigStartsAt = 0;
static bool __flagRestartSmartConfig  = false;

bool WIFI_IsSmartConfigTimedOut(void)
{
#if false
    uint64_t sysRuntime = SYSTICK_GetSysRunTime();
    if((sysRuntime - __smartConfigStartsAt) % 1000 == 0)
    {
        DBG_log("[WIFI Smart CD] %d sec\n",
                (SMART_CONFIG_WAITING_TIME_MS - (sysRuntime - __smartConfigStartsAt)) / 1000);
    }
    return (sysRuntime - __smartConfigStartsAt > SMART_CONFIG_WAITING_TIME_MS);
#else
    return (SYSTICK_GetSysRunTime() - __smartConfigStartsAt > SMART_CONFIG_WAITING_TIME_MS);
#endif
}

void WIFI_RestartSmartConfig(void)
{
    __flagRestartSmartConfig = true;
}
#endif

#if WIFI_USE_SMART_CONFIG
static COMM_STATE_t __WIFI_SmartConfig(void);
#else
static COMM_STATE_t __WIFI_ConnectInternet(void);
#endif

static COMM_STATE_t __WIFI_ConnectMqttServer(void);
static COMM_STATE_t __WIFI_CommIotServer(void);
static COMM_STATE_t __WIFI_ConnectOtaServer(void);

typedef struct WIFI_FSM WIFI_FSM_t;
struct WIFI_FSM
{
    COMM_STATE_t (*stateHandler)(void);
};

#if WIFI_USE_SMART_CONFIG
static WIFI_FSM_t wifiFsm = {.stateHandler = __WIFI_SmartConfig};
#else
static WIFI_FSM_t wifiFsm = {.stateHandler = __WIFI_ConnectInternet};
#endif

// clang-format off
#define __AT_CWJAP_SSID_PWD_CMD "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PWD "\"\r\n"
#define __AT_SNTPCFG_CMD "AT+CIPSNTPCFG=1," UTC(8) ",\"" SNTP_SERVER_1 "\",\"" SNTP_SERVER_2 "\"\r\n"
#define __AT_SUBSCRIBE_CMD "AT+MQTTSUB=0,\"$sys/" MQTT_PRODUCT_ID "/" MQTT_DEVICE_NAME "/thing/property/set\",0\r\n"
#define __AT_CONNECT_OTA_CMD "AT+CIPSTART=\"TCP\",\"" HTTP_HOST "\",80\r\n"
#define __AT_OTA_CONNECTION_CHECK_CMD "AT+CIPSTATE?\r\n"
#define __AT_INIT_VERSION_UPLOAD_CMD "AT+CIPSEND=%d\r\n" // %d is the length of the version string
#define __AT_OTA_VERSION_UPLOAD_CMD \
    "POST http://" HTTP_HOST "/fuse-ota/" MQTT_PRODUCT_ID "/" MQTT_DEVICE_NAME "/version HTTP/1.1\r\nAuthorization:" MQTT_DEVICE_TOKEN "\r\nContent-Type:application/json\r\nHost:" HTTP_HOST "\r\nContent-Length:%d\r\n\r\n%s"
#define __IOT_SYS_VERSION_STR        "{\"s_version\":\"%s\", \"f_version\":\"1.0\"}"
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
    [AT_SMART_CONFIG_DELAY] = {.cmd = NULL, .desiredResponse = "GOT IP", .timeoutMs = 1, .maxRetry = 0},
    [AT_REPLY_APP_DELAY]    = {.cmd = NULL, .desiredResponse = "deadbeef", .timeoutMs = 6000, .maxRetry = 0},
    [AT_STOP_SMART_CONFIG]  = {.cmd = "AT+CWSTOPSMART\r\n", .desiredResponse = "OK", .timeoutMs = 1000, .maxRetry = 3},
    [AT_WIFI_CONNECTION_CHECK] = {.cmd = "AT+CWSTATE?\r\n", .desiredResponse = ":2", .timeoutMs = 1000, .maxRetry = 3},
    [AT_SMART_CONFIG_IDLE]     = {.cmd = NULL, .desiredResponse = "deadbeef", .timeoutMs = 1, .maxRetry = 0}};

static COMM_STATE_t __WIFI_SmartConfig(void)
{
    COMM_STATE_t commState;
    static AT_CONFIG_WIFI_CMD_INDEX_t atCmd = AT_START_SMART_CONFIG;

    switch(atCmd)
    {
        case AT_SMART_CONFIG_IDLE:
            if(__flagRestartSmartConfig)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_STOP_SMART_CONFIG;
            }
            return COMM_STATE_IDLE;
        case AT_START_SMART_CONFIG:
            commState = AT_CmdHandler(__AT_CONFIG_WIFI_CMD + AT_START_SMART_CONFIG);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI Smart] Smart Config Started\n");
#endif
                AT_ClearResponseSnapshot();
                __smartConfigStartsAt = SYSTICK_GetSysRunTime();
                atCmd                 = AT_SMART_CONFIG_DELAY;
                return COMM_STATE_OK;
            }
            if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        case AT_SMART_CONFIG_DELAY:
            if(__flagRestartSmartConfig)
            {
                __flagRestartSmartConfig = false;
                atCmd                    = AT_START_SMART_CONFIG;
                break;
            }

            if(WIFI_IsSmartConfigTimedOut())
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI Smart] Timed Out\n");
#endif
                atCmd = AT_SMART_CONFIG_IDLE;
                return COMM_STATE_FAILED_TIMER;
            }
            else
            {
                commState = AT_CmdHandler(__AT_CONFIG_WIFI_CMD + AT_SMART_CONFIG_DELAY);
                if(commState == COMM_STATE_OK)
                {
#if DEBUG_PRINTING
                    DBG_log("[WIFI Smart] WIFI Config done\n");
#endif
                    AT_ClearResponseSnapshot();
                    atCmd = AT_REPLY_APP_DELAY;
                    return COMM_STATE_OK;
                }
                else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
                {
                    AT_ClearResponseSnapshot();
                    return commState;
                }
            }
            break;
        case AT_REPLY_APP_DELAY:
            commState = AT_CmdHandler(__AT_CONFIG_WIFI_CMD + AT_REPLY_APP_DELAY);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI Smart] REPLY APP DONE\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = AT_STOP_SMART_CONFIG;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_STOP_SMART_CONFIG;
                return commState;
            }
            break;
        case AT_STOP_SMART_CONFIG:
            commState = AT_CmdHandler(__AT_CONFIG_WIFI_CMD + AT_STOP_SMART_CONFIG);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI Smart] Stop smart config done\n");
#endif
                AT_ClearResponseSnapshot();
                if(__flagRestartSmartConfig)
                {
                    __flagRestartSmartConfig = false;
                    atCmd                    = AT_START_SMART_CONFIG;
                }
                else
                {
                    atCmd = AT_WIFI_CONNECTION_CHECK;
                }

                return COMM_STATE_OK;
            }
            if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        case AT_WIFI_CONNECTION_CHECK:
            commState = AT_CmdHandler(__AT_CONFIG_WIFI_CMD + AT_WIFI_CONNECTION_CHECK);
            if(commState == COMM_STATE_OK)
            {
#if WIFI_USE_SMART_CONFIG
                __PrintCWStateResponse();
#endif
                AT_ClearResponseSnapshot();
                wifiFsm.stateHandler = __WIFI_ConnectMqttServer;
                return COMM_STATE_OK;
            }
            if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI Smart] State check failed\n");
#endif
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
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
    [AT_CWJAP_DELAY]    = {.cmd = NULL, .desiredResponse = "OK", .timeoutMs = 2000, .maxRetry = 0},
};

static COMM_STATE_t __WIFI_ConnectInternet(void)
{
    COMM_STATE_t commState;
    static AT_CONNECT_INTERNET_CMD_INDEX_t atCmd = AT_CWJAP_SSID_PWD;
    switch(atCmd)
    {
        case AT_CWJAP_SSID_PWD:
            commState = AT_CmdHandler(__AT_CONNECT_INTERNET_CMD + AT_CWJAP_SSID_PWD);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Wifi Connected\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = AT_CWJAP_DELAY;
            }
            if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Wifi Connection Failed\n");
#endif
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        case AT_CWJAP_DELAY:
            commState = AT_CmdHandler(__AT_CONNECT_INTERNET_CMD + AT_CWJAP_DELAY);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Delaying for wifi connection...\n");
#endif
                AT_ClearResponseSnapshot();
                // atCmd = AT_CWJAP_SSID_PWD;
                wifiFsm.stateHandler = __WIFI_ConnectMqttServer;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                // #if DEBUG_PRINTING
                //                 DBG_log("[WIFI] Delay Done\n");
                // #endif
                AT_ClearResponseSnapshot();
                // atCmd                = AT_CWJAP_SSID_PWD;
                wifiFsm.stateHandler = __WIFI_ConnectMqttServer;
                return commState;
            }
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
}
#endif

/*OTA Handler Here*/
/*
1. connect to OTA server HTTP TCP
2. Upload current version
3. check if newer version available
4. if newer version available, download the new version
5. set the new version to sys param then write to flash
6. set the update flag to 1 then reset the system
*/

static char __httpPostBuf[500];
typedef enum
{
    AT_OTA_CONNECT_SERVER = 0,
    AT_OTA_CHECK_CONNECTION,
    AT_INTIT_VERSION_UPLOAD,
    AT_UPLOAD_CURR_VERSION,

} AT_OTA_CONNECT_INDEX_t;

static const AT_Cmd_t __AT_OTA_CONNECT_SERVER_CMD[] = {
    [AT_OTA_CONNECT_SERVER]   = {.cmd             = __AT_CONNECT_OTA_CMD,
                                 .desiredResponse = "CONNECT",
                                 .timeoutMs       = 5000,
                                 .maxRetry        = 3},
    [AT_OTA_CHECK_CONNECTION] = {.cmd             = __AT_OTA_CONNECTION_CHECK_CMD,
                                 .desiredResponse = "OK",
                                 .timeoutMs       = 5000,
                                 .maxRetry        = 3},
    [AT_INTIT_VERSION_UPLOAD] = {.cmd             = __AT_INIT_VERSION_UPLOAD_CMD,
                                 .desiredResponse = ">",
                                 .timeoutMs       = 5000,
                                 .maxRetry        = 0},
    [AT_UPLOAD_CURR_VERSION]  = {.cmd             = __AT_OTA_VERSION_UPLOAD_CMD,
                                 .desiredResponse = "succ",
                                 .timeoutMs       = 10000,
                                 .maxRetry        = 0}};

static COMM_STATE_t __WIFI_ConnectOtaServer(void)
{
    COMM_STATE_t commState;
    static AT_OTA_CONNECT_INDEX_t atCmd = AT_OTA_CONNECT_SERVER;
    char __cmdBuffer[50];
    uint16_t strLen = 0;

    switch(atCmd)
    {
        case AT_OTA_CONNECT_SERVER:
            commState = AT_CmdHandler(__AT_OTA_CONNECT_SERVER_CMD + AT_OTA_CONNECT_SERVER);
            if(commState == COMM_STATE_OK)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_OTA_CHECK_CONNECTION;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        case AT_OTA_CHECK_CONNECTION:
            commState = AT_CmdHandler(__AT_OTA_CONNECT_SERVER_CMD + AT_OTA_CHECK_CONNECTION);
            if(commState == COMM_STATE_OK)
            {
                AT_ClearResponseSnapshot();
                char versionStr[50] = {0};
                STORAGE_GetSysVersion(__cmdBuffer); // borrow the cmdBuffer to store the version string
                /* "{\"s_version\":\"[real version]\", \"f_version\":\"[does not matter , leave 1.0]\"}" */
                sprintf(versionStr, __IOT_SYS_VERSION_STR, __cmdBuffer);
                strLen = strlen(versionStr);
                memset(__httpPostBuf, 0, sizeof(__httpPostBuf));
                sprintf(__httpPostBuf, __AT_OTA_CONNECT_SERVER_CMD[AT_INTIT_VERSION_UPLOAD].cmd, strLen, versionStr);
                atCmd = AT_INTIT_VERSION_UPLOAD;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_OTA_CONNECT_SERVER;
                return commState;
            }
            break;
        case AT_INTIT_VERSION_UPLOAD:
            sprintf(__cmdBuffer, __AT_OTA_CONNECT_SERVER_CMD[AT_INTIT_VERSION_UPLOAD].cmd, strLen);
            commState =
                _AT_CmdHandler(__cmdBuffer, __AT_OTA_CONNECT_SERVER_CMD[AT_INTIT_VERSION_UPLOAD].desiredResponse,
                               __AT_OTA_CONNECT_SERVER_CMD[AT_INTIT_VERSION_UPLOAD].timeoutMs,
                               __AT_OTA_CONNECT_SERVER_CMD[AT_INTIT_VERSION_UPLOAD].maxRetry);
            if(commState == COMM_STATE_OK)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_UPLOAD_CURR_VERSION;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_OTA_CONNECT_SERVER;
                return commState;
            }
            break;
        case AT_UPLOAD_CURR_VERSION:
            // TODO: upload the current version string to the OTA server
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
}

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

static COMM_STATE_t __WIFI_ConnectMqttServer(void)
{
    COMM_STATE_t commState;
    static AT_CONNECT_MQTT_SERVER_CMD_INDEX_t atCmd = AT_MQTTUSERCFG;

    switch(atCmd)
    {
        case AT_MQTTUSERCFG:
            commState = AT_CmdHandler(__AT_MQTT_CONNECT_SERVER_CMD + AT_MQTTUSERCFG);
            if(commState == COMM_STATE_OK)
            {
                AT_ClearResponseSnapshot();
                atCmd = AT_MQTTCONN;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
                return commState;
            }
            break;
        case AT_MQTTCONN:
            commState = AT_CmdHandler(__AT_MQTT_CONNECT_SERVER_CMD + AT_MQTTCONN);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] IoT Connected\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = AT_SNTPCFG;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] IoT Connection Failed\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = AT_MQTTUSERCFG;
                return commState;
            }
            break;
        case AT_SNTPCFG:
            commState = AT_CmdHandler(__AT_MQTT_CONNECT_SERVER_CMD + AT_SNTPCFG);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] SNTP Connected\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = AT_SUBSCRIBE;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
#if DEBUG_PRINTING
                DBG_log("[WIFI] SNTP Connection Failed\n");
#endif
                return commState;
            }
            break;
        case AT_SUBSCRIBE:
            commState = AT_CmdHandler(__AT_MQTT_CONNECT_SERVER_CMD + AT_SUBSCRIBE);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] MQTT Subscribed\n");
#endif
                AT_ClearResponseSnapshot();
                wifiFsm.stateHandler = __WIFI_CommIotServer;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                AT_ClearResponseSnapshot();
#if DEBUG_PRINTING
                DBG_log("[WIFI] MQTT Subscription Failed\n");
#endif
                return commState;
            }
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
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

static __forceinline AT_BUSINESS_CMD_INDEX_t __IncrementAtBusinessCmd(AT_BUSINESS_CMD_INDEX_t cmd)
{
    return (AT_BUSINESS_CMD_INDEX_t)((cmd + 1) % (AT_MQTT_PUB_SENSOR_DATA + 1));
}

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

static COMM_STATE_t __WIFI_CommIotServer(void)
{
    static COMM_STATE_t currCommState    = COMM_STATE_OK;
    static AT_BUSINESS_CMD_INDEX_t atCmd = AT_SNTPTIME;
    static uint64_t lastSntpSysTime      = 0;
    static uint64_t lastPubSysTime       = 0;
    static char cmdStrBuf[256];
    static SUB_TOPIC_DATA_t subTopicData;

    switch(atCmd)
    {
        case AT_LISTEN_SUB:
            currCommState = AT_CmdHandler(__AT_BUSINESS_CMD + atCmd);
            if(currCommState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Recv Sub Topic\n");
#endif
                if(!__HandleSubTopicPayload(AT_GetResponseSnapshot(), &subTopicData))
                {
#if DEBUG_PRINTING
                    DBG_log("[WIFI] Sub Topic Payload Parsing Failed\n");
#endif
                    AT_ClearResponseSnapshot();
                    atCmd = AT_SNTPTIME;
                    return COMM_STATE_FAILED_RESPONSE;
                }
                AT_ClearResponseSnapshot();
#if DEBUG_PRINTING
                DBG_log("[WIFI] parsed data: id %s, led %d\n", subTopicData.id, subTopicData.ledVal);
#endif
                if(subTopicData.ledVal >= 1)
                {
                    LED_SetState(0, SET);
                }
                else
                {
                    LED_SetState(0, RESET);
                }
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return COMM_STATE_OK;
            }
            if(currCommState == COMM_STATE_FAILED_TIMER || currCommState == COMM_STATE_FAILED_RESPONSE)
            {
                /* case where no sub topic received */
                AT_ClearResponseSnapshot();
                atCmd = AT_SNTPTIME;
                return currCommState;
            }
            break;
        case AT_REPLY_SUB:
            if(currCommState != COMM_STATE_PROCESSING)
            {
                memset(cmdStrBuf, 0, 256);
                snprintf(cmdStrBuf, 256, __AT_BUSINESS_CMD[AT_REPLY_SUB].cmd, subTopicData.id);
            }
            currCommState =
                _AT_CmdHandler(cmdStrBuf, __AT_BUSINESS_CMD[AT_REPLY_SUB].desiredResponse,
                               __AT_BUSINESS_CMD[AT_REPLY_SUB].timeoutMs, __AT_BUSINESS_CMD[AT_REPLY_SUB].maxRetry);
            if(currCommState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Reply Sub Topic Success\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return COMM_STATE_OK;
            }
            else if(currCommState == COMM_STATE_FAILED_TIMER || currCommState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Reply Sub Topic Failed\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return currCommState;
            }
            break;
        case AT_SNTPTIME:
            if(currCommState != COMM_STATE_PROCESSING)
            {
                if((SYSTICK_GetSysRunTime() - lastSntpSysTime) < SNTP_REQ_PERIOD_MS)
                {
                    atCmd = __IncrementAtBusinessCmd(atCmd);
                    break;
                }
                else
                {
#if DEBUG_PRINTING
                    DBG_log("[WIFI] Period request Time\n");
#endif
                    lastSntpSysTime = SYSTICK_GetSysRunTime();
                }
            }
            currCommState = AT_CmdHandler(__AT_BUSINESS_CMD + atCmd);
            if(currCommState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Time Updated\n");
#endif
                __UpdateRtcTimeBySntpResponse(AT_GetResponseSnapshot());
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return COMM_STATE_OK;
            }
            if(currCommState == COMM_STATE_FAILED_TIMER || currCommState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Time Update Failed\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return currCommState;
            }
            break;
        case AT_MQTT_PUB_SENSOR_DATA:
            if(currCommState != COMM_STATE_PROCESSING)
            {
                if((SYSTICK_GetSysRunTime() - lastPubSysTime) < MQTT_DAT_UPLOAD_PERIOD_MS)
                {
                    atCmd = __IncrementAtBusinessCmd(atCmd);
                    break;
                }
                else
                {
#if DEBUG_PRINTING
                    DBG_log("[WIFI] Period request Sensor Data\n");
#endif
                    lastPubSysTime = SYSTICK_GetSysRunTime();
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
                        lastPubSysTime = SYSTICK_GetSysRunTime();
                        atCmd          = __IncrementAtBusinessCmd(atCmd);
                        break;
                    }
                }
            }

            currCommState = _AT_CmdHandler(cmdStrBuf, __AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].desiredResponse,
                                           __AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].timeoutMs,
                                           __AT_BUSINESS_CMD[AT_MQTT_PUB_SENSOR_DATA].maxRetry);

            if(currCommState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Sensor Data Uploaded, waiting on next upload period\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return COMM_STATE_OK;
            }
            if(currCommState == COMM_STATE_FAILED_TIMER || currCommState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Sensor Data Upload Failed\n");
#endif
                AT_ClearResponseSnapshot();
                atCmd = __IncrementAtBusinessCmd(atCmd);
                return currCommState;
            }
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
}

void WIFI_Run(void)
{
    static bool isAtInitSuccess = false;
    if(!isAtInitSuccess)
    {
        if(AT_Init() == COMM_STATE_OK)
        {
            isAtInitSuccess = true;
        }
        else
        {
            return;
        }
    }
    else
        wifiFsm.stateHandler();
}
