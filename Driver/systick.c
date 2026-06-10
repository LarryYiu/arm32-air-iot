#include "systick.h"
#include "gd32f30x.h"

static uint64_t g_sysRunTime = 0;

void SYSTICK_Config(void)
{
    SysTick_Config(rcu_clock_freq_get(CK_SYS) / 1000);
}

void SysTick_Handler(void)
{
    g_sysRunTime++;
}

uint64_t SYSTICK_GetSysRunTime(void)
{
    return g_sysRunTime;
}
