#ifndef __AT_H__
#define __AT_H__

#include <stdint.h>

#define AT_BUSY_DELAY_MS 3000

typedef struct AT_Cmd AT_Cmd_t;
struct AT_Cmd
{
    char* cmd;
    char* desiredResponse;
    uint32_t timeoutMs;
    uint8_t maxRetry;
};
typedef enum COMM_STATE COMM_STATE_t;
enum COMM_STATE
{
    COMM_STATE_IDLE = 0,
    COMM_STATE_OK,
    COMM_STATE_DELAY_DONE,
    COMM_STATE_PROCESSING,
    COMM_STATE_FAILED_TIMER,
    COMM_STATE_FAILED_RESPONSE
};

COMM_STATE_t AT_CmdHandler(const char* cmd, const char* desiredResponse, const uint32_t* timeoutMs,
                           const uint8_t* maxRetry);
COMM_STATE_t AT_Init(void);

char* AT_GetResponseSnapshot(void);
void AT_ClearResponseSnapshot(void);
#endif // __AT_H__
