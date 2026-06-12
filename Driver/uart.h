#ifndef __UART_H__
#define __UART_H__
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "config.h"
#include "gpio_decoder.h"

// TX PP_out, RX floating/pullup input, pull up for idle high
void UART_Config(uint32_t baudrate);

#define UART_Enable(baudrate) UART_Config(baudrate)
void UART_Disable(void);

bool UART_IsDataReady(void);
const uint8_t* UART_GetDataBuffer(void);

#endif // __UART_H__
