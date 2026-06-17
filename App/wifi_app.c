#include "wifi_app.h"
#include <stdint.h>
#include <stdbool.h>
#include "at.h"
#include "iot_config.h"
#include "systick.h"
#include "rtc.h"
#include "RTT_Debug.h"

#define DEBUG_PRINTING true

static COMM_STATE_t __WIFI_ConnectInternet(void);
static COMM_STATE_t __WIFI_ConnectIotServer(void);
static COMM_STATE_t __WIFI_CommIotServer(void);

typedef struct WIFI_FSM WIFI_FSM_t;
struct WIFI_FSM
{
    COMM_STATE_t (*stateHandler)(void);
};

static WIFI_FSM_t wifiFsm = {.stateHandler = __WIFI_ConnectInternet};

#define __AT_CWJAP_SSID_PWD_CMD "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PWD "\"\r\n"
#define __AT_SNTPCFG_CMD "AT+CIPSNTPCFG=1," UTC(8) ",\"" SNTP_SERVER_1 "\",\"" SNTP_SERVER_2 "\"\r\n"

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
                DBG_log("[WIFI] Delaying\n");
#endif
                AT_ClearResponseSnapshot();
                // atCmd = AT_CWJAP_SSID_PWD;
                wifiFsm.stateHandler = __WIFI_ConnectIotServer;
                return COMM_STATE_OK;
            }
            else if(commState == COMM_STATE_FAILED_TIMER || commState == COMM_STATE_FAILED_RESPONSE)
            {
                // #if DEBUG_PRINTING
                //                 DBG_log("[WIFI] Delay Done\n");
                // #endif
                AT_ClearResponseSnapshot();
                // atCmd                = AT_CWJAP_SSID_PWD;
                wifiFsm.stateHandler = __WIFI_ConnectIotServer;
                return commState;
            }
            break;
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
}

typedef enum
{
    AT_MQTTUSERCFG = 0,
    AT_MQTTCONN,
    AT_SNTPCFG
} AT_CONNECT_SERVER_CMD_INDEX_t;

static const AT_Cmd_t __AT_CONNECT_SERVER_CMD[] = {
    [AT_MQTTUSERCFG] =
        {.cmd = MQTT_USERCFG_CMD("1", "0", "0"),
         //  .cmd = "AT+MQTTUSERCFG=0,1,\"GD32Board\",\"4m3RoDJR8n\",\"version=2018-10-31&res=products%2F4m3RoDJR8n%"
         //         "2Fdevices%2FGD32Board&et=1830268800&method=md5&sign=qIQ8T1heqVLOZ23gunAFjg%3D%3D\",0,0,\"\"\r\n",
         .desiredResponse = "OK",
         .timeoutMs       = 300,
         .maxRetry        = 3},
    [AT_MQTTCONN] = {.cmd = MQTT_CONN_CMD("1"), .desiredResponse = "OK", .timeoutMs = 2000, .maxRetry = 3},
    [AT_SNTPCFG]  = {.cmd = __AT_SNTPCFG_CMD, .desiredResponse = "OK", .timeoutMs = 500, .maxRetry = 0}};

static COMM_STATE_t __WIFI_ConnectIotServer(void)
{
    COMM_STATE_t commState;
    static AT_CONNECT_SERVER_CMD_INDEX_t atCmd = AT_MQTTUSERCFG;

    switch(atCmd)
    {
        case AT_MQTTUSERCFG:
            commState = AT_CmdHandler(__AT_CONNECT_SERVER_CMD + AT_MQTTUSERCFG);
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
            commState = AT_CmdHandler(__AT_CONNECT_SERVER_CMD + AT_MQTTCONN);
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
            commState = AT_CmdHandler(__AT_CONNECT_SERVER_CMD + AT_SNTPCFG);
            if(commState == COMM_STATE_OK)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] SNTP Connected\n");
#endif
                AT_ClearResponseSnapshot();
                // atCmd = AT_MQTTUSERCFG;
                wifiFsm.stateHandler = __WIFI_CommIotServer;
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
        default:
            break;
    }
    return COMM_STATE_PROCESSING;
}

typedef enum
{
    AT_IDLE,
    AT_SNTPTIME
} AT_BUSINESS_CMD_INDEX_t;

static AT_Cmd_t __AT_BUSINESS_CMD[] = {
    [AT_IDLE]     = {.cmd = NULL, .desiredResponse = "deadbeaf", .timeoutMs = 500, .maxRetry = 0},
    [AT_SNTPTIME] = {.cmd = "AT+CIPSNTPTIME?\r\n", .desiredResponse = "OK", .timeoutMs = 500, .maxRetry = 0},
};

static void __IncrementAtBusinessCmd(AT_BUSINESS_CMD_INDEX_t* cmd)
{
    *cmd = *cmd > AT_SNTPTIME ? AT_IDLE : (AT_BUSINESS_CMD_INDEX_t)(*cmd + 1);
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
    static AT_BUSINESS_CMD_INDEX_t atCmd = AT_IDLE;
    static uint64_t lastSntpSysTime      = 0;

    switch(atCmd)
    {
        case AT_IDLE:
            __IncrementAtBusinessCmd(&atCmd);
            break;
        case AT_SNTPTIME:
            if(currCommState != COMM_STATE_PROCESSING)
            {
                if((SYSTICK_GetSysRunTime() - lastSntpSysTime) < SNTP_REQ_PERIOD_MS)
                {
                    __IncrementAtBusinessCmd(&atCmd);
                    break;
                }
                else
                {
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
                __IncrementAtBusinessCmd(&atCmd);
                return COMM_STATE_OK;
            }
            if(currCommState == COMM_STATE_FAILED_TIMER || currCommState == COMM_STATE_FAILED_RESPONSE)
            {
#if DEBUG_PRINTING
                DBG_log("[WIFI] Time Update Failed\n");
#endif
                AT_ClearResponseSnapshot();
                __IncrementAtBusinessCmd(&atCmd);
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
