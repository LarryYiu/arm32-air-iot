#ifndef __ESP8684_DRIVER_H__
#define __ESP8684_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

void ESP8684_Init(void);

void ESP8684_EnableModule(void);

void ESP8684_DisableModule(void);

BaseType_t ESP8684_WaitForPacketSemaphore(void);

void ESP8684_LockUART(void);

void ESP8684_UnlockUART(void);

void ESP8684_SendCommand(const char* cmd);

void ESP8684_SnapshotResponse(char* buffer);

#endif // __ESP8684_DRIVER_H__
