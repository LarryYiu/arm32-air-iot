#ifndef __ESP8684_DRIVER_H__
#define __ESP8684_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

void ESP8684_Init(void);

void ESP8684_EnableModule(void);

void ESP8684_DisableModule(void);

bool ESP8684_IsPacketReceived(void);

void ESP8684_SendCommand(const char* cmd);

void ESP8684_SnapshotResponse(char* buffer);

#endif // __ESP8684_DRIVER_H__
