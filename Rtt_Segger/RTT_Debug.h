#ifndef _RTT_H_
#define _RTT_H_

#include "SEGGER_RTT.h"

#define STR_DEBUG
//#define USART_DEBUG

#define DBGLOG
#define DBGWARNING
#define DBGERROR

#if defined(STR_DEBUG)
#define _S_LINE(x) #x
#define __S_LINE(x) _S_LINE(x)
#define __S_LINE__ __S_LINE(__LINE__)

#if defined(DBGLOG)
#define DBG_log(format, ...) SEGGER_RTT_printf(0,RTT_CTRL_TEXT_GREEN"[log]"RTT_CTRL_RESET format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_log(format, ...)
#endif

#if defined(DBGWARNING)
#define DBG_Warning(format, ...) SEGGER_RTT_printf(0,RTT_CTRL_TEXT_YELLOW"[Warning!]"RTT_CTRL_RESET format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_Warning(format, ...)
#endif

#if defined(DBGERROR)
#define DBG_Error(format, ...) SEGGER_RTT_printf(0,RTT_CTRL_TEXT_RED"[!Error!]"RTT_CTRL_RESET format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_Error(format, ...)
#endif

#else
#if defined(USART_DEBUG)

#if defined(DBGLOG)
#define DBG_log(format, ...) printf("[log]"format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_log(format, ...)
#endif

#if defined(DBGWARNING)
#define DBG_Warning(format, ...) printf("[Warning!]"format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_Warning(format, ...)
#endif

#if defined(DBGERROR)
#define DBG_Error(format, ...) printf("[!Error!]"format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_Error(format, ...)
#endif

#else
#define DBG_log(format, ...)
#define DBG_Warning(format, ...)
#define DBG_Error(format, ...)

#endif

#endif

#endif

