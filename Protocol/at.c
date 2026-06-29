#include "at.h"
#include <string.h>
#include "esp8684_driver.h"
#include "RTT_Debug.h"
#include "FreeRTOS.h"
#include "task.h"

#define DEBUG_PRINTING true

static char __atResponseSnapshot[1024];
void AT_ClearResponseSnapshot(void)
{
    memset(__atResponseSnapshot, 0, 1024);
}

char* AT_GetResponseSnapshot(void)
{
    return __atResponseSnapshot;
}

static const char* __ASYNC_RESPONSE_LIST[] = {"CONNECTED", "DISCONNECT", "ready"};

static bool __IsAsyncResponse(void)
{
    for(uint8_t i = 0; i < sizeof(__ASYNC_RESPONSE_LIST) / sizeof(__ASYNC_RESPONSE_LIST[0]); i++)
    {
        if(strstr(__atResponseSnapshot, __ASYNC_RESPONSE_LIST[i]) != NULL)
        {
            return true;
        }
    }
    return false;
}

typedef enum
{
    AT_RST,
    AT_RST_WAIT,
    AT_E0,
    AT_CWMODE_1,
} AT_INIT_CMD_INDEX_t;

static const AT_Cmd_t __AT_INIT_CMD[] = {
    [AT_RST] =
        {
            .cmd             = "AT+RST\r\n",
            .desiredResponse = "OK",
            .timeoutMs       = 500,
            .maxRetry        = 3,
        },
    [AT_RST_WAIT] =
        {
            .cmd             = NULL,
            .desiredResponse = "ready",
            .timeoutMs       = 3000,
            .maxRetry        = 0,
        },
    [AT_E0] =
        {
            .cmd             = "ATE0\r\n",
            .desiredResponse = "OK",
            .timeoutMs       = 500,
            .maxRetry        = 3,
        },
    [AT_CWMODE_1] =
        {
            .cmd             = "AT+CWMODE=1\r\n",
            .desiredResponse = "OK",
            .timeoutMs       = 500,
            .maxRetry        = 0,
        },
};

#define __AT_INIT_CMD_COUNT (sizeof(__AT_INIT_CMD) / sizeof(__AT_INIT_CMD[0]))

COMM_STATE_t AT_Init(void)
{
    uint8_t currentCmdIndex = 0;

    while(currentCmdIndex < __AT_INIT_CMD_COUNT)
    {
        COMM_STATE_t state =
            AT_CmdHandler(__AT_INIT_CMD[currentCmdIndex].cmd, __AT_INIT_CMD[currentCmdIndex].desiredResponse,
                          &__AT_INIT_CMD[currentCmdIndex].timeoutMs, &__AT_INIT_CMD[currentCmdIndex].maxRetry);
        if(state == COMM_STATE_OK || state == COMM_STATE_DELAY_DONE)
        {
            AT_ClearResponseSnapshot();
            currentCmdIndex++;
        }
        else if(state == COMM_STATE_FAILED_TIMER || state == COMM_STATE_FAILED_RESPONSE)
        {
            AT_ClearResponseSnapshot();
            return state;
        }
    }
    AT_ClearResponseSnapshot();
    return COMM_STATE_OK;
}

COMM_STATE_t AT_CmdHandler(const char* cmd, const char* desiredResponse, const uint32_t* timeoutMs,
                           const uint8_t* maxRetry)
{
    if(!desiredResponse)
    {
#if DEBUG_PRINTING
        DBG_log("[AT] Delaying from cmd ...\n");
#endif
        vTaskDelay(*timeoutMs);
#if DEBUG_PRINTING
        DBG_log("[AT] Delay Done\n");
#endif
        return COMM_STATE_DELAY_DONE;
    }
    else
    {
        // wait
        uint32_t timeout = *timeoutMs;
        uint8_t retry    = 0;
        while(retry <= *maxRetry)
        {
            if(cmd)
            {
#if DEBUG_PRINTING
                DBG_log("[AT] Sending Command: %s\n", cmd);
#endif
                ESP8684_SendCommand(cmd);
            }
            while(timeout > 0)
            {
                if(ESP8684_IsPacketReceived())
                {
                    ESP8684_SnapshotResponse(__atResponseSnapshot);
                    if(__IsAsyncResponse())
                    {
#if DEBUG_PRINTING
                        DBG_log("[AT] Async response: %s\n", __atResponseSnapshot);
#endif
                        if(strstr(__atResponseSnapshot, "ERROR") != NULL)
                        {
#if DEBUG_PRINTING
                            DBG_log("[AT] Received ERROR \n");
#endif
                            AT_ClearResponseSnapshot();
                            break;
                        }
                        else if(strstr(__atResponseSnapshot, desiredResponse) != NULL)
                        {
                            return COMM_STATE_OK;
                        }
                        else
                        {
                            AT_ClearResponseSnapshot();
                            continue; // ignore async response and continue waiting for the desired response
                        }
                    }
                    else if(strstr(__atResponseSnapshot, "busy") != NULL)
                    {
#if DEBUG_PRINTING
                        DBG_log("[AT] Module Busy, delaying extra ...\n");
#endif
                        AT_ClearResponseSnapshot();
                        vTaskDelay(pdMS_TO_TICKS(AT_BUSY_DELAY_MS));
                    }
                    else if(strstr(__atResponseSnapshot, desiredResponse) != NULL)
                    {
#if DEBUG_PRINTING
                        DBG_log("[AT] Got expected response: %s\n", __atResponseSnapshot);
#endif
                        return COMM_STATE_OK;
                    }
                    else if(retry < *maxRetry)
                    {
#if DEBUG_PRINTING
                        DBG_log("[AT] Unexpected response: %s\n", __atResponseSnapshot);
                        DBG_log("[AT] Retry %d/%d\n", retry, *maxRetry);
#endif
                        retry++;
                        AT_ClearResponseSnapshot();
                        if(cmd)
                        {
#if DEBUG_PRINTING
                            DBG_log("[AT] Sending Command: %s\n", cmd);
#endif
                            ESP8684_SendCommand(cmd);
                        }
                        timeout = *timeoutMs;
                    }
                    else
                    {
#if DEBUG_PRINTING
                        DBG_log("[AT] Failed to get expected response\n");
#endif
                        return COMM_STATE_FAILED_RESPONSE;
                    }
                }
                timeout--;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            retry++;
        }
        return COMM_STATE_FAILED_TIMER;
    }
}
