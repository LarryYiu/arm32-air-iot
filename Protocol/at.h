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
    COMM_STATE_IDLE,
    COMM_STATE_OK,
    COMM_STATE_PROCESSING,
    COMM_STATE_FAILED_TIMER,
    COMM_STATE_FAILED_RESPONSE
};

COMM_STATE_t AT_CmdHandler(const AT_Cmd_t* cmd);
COMM_STATE_t _AT_CmdHandler(char* cmd, char* desiredResponse, uint32_t timeoutMs, uint8_t maxRetry);

COMM_STATE_t AT_Init(void);

char* AT_GetResponseSnapshot(void);
void AT_ClearResponseSnapshot(void);
#endif // __AT_H__
