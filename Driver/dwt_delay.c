#include "dwt_delay.h"

static uint32_t g_sysClock;

void DWT_Init(void)
{
    /* TRC OFF */
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    /* TRC ON */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* COUNTER OFF */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    /* COUNTER ON */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* RESET COUNTER */
    DWT->CYCCNT = 0;

    g_sysClock = rcu_clock_freq_get(CK_SYS);
}

void DWT_On(void)
{
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void DWT_Off(void)
{
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
}

void DWT_Delay_us(uint32_t us)
{
    uint32_t tickStart = DWT->CYCCNT;
    us *= g_sysClock / 1000000;
    while((DWT->CYCCNT - tickStart) < us);
}

void DWT_Delay_ms(uint16_t ms)
{
    for(uint32_t i = 0; i < ms; i++)
    {
       DWT_Delay_us(1000);
    }
}
