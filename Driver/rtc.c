#include "rtc.h"
#include <time.h>
#include "gd32f30x.h"
#include "RTT_Debug.h"
#include "dwt_delay.h"

#define MAGIC_CODE 0x5A5A

void RTC_Init(void)
{
    if(bkp_read_data(BKP_DATA_0) != MAGIC_CODE) // check if RTC is already configured
    {
        /* Need to enable both RTC and BKP clock */
        rcu_periph_clock_enable(RCU_PMU);
        rcu_periph_clock_enable(RCU_BKPI);

        /* enable write access to PMU backup registers */
        pmu_backup_write_enable();

        /* reset backup registers */
        bkp_deinit();
        /* enable LXTAL and wait for it to stabilize */
        rcu_osci_on(RCU_LXTAL);
        rcu_osci_stab_wait(RCU_LXTAL);
        /* set RTC clock source to LXTAL */
        rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
        /* enable RTC clock */
        rcu_periph_clock_enable(RCU_RTC);
        /* wait for APB1 clock and RTC clock to sync*/
        rtc_register_sync_wait();
        /* wait */
        rtc_lwoff_wait();
        /* set prescaler to 32767 */
        rtc_prescaler_set(32767);
        /* wait */
        rtc_lwoff_wait();
        /* set time to 1970-01-01 0:0:0 */
        rtc_counter_set(0);
        bkp_write_data(BKP_DATA_0, MAGIC_CODE);
        return;
    }
    /* enable access to RTC, enable PMU and BKP clock */
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_periph_clock_enable(RCU_RTC);
    /* enable write access to PMU backup registers */
    pmu_backup_write_enable();
    /* wait for APB1 clock and RTC clock to sync */
    rtc_register_sync_wait();
    /* wait */
    rtc_lwoff_wait();
}

void RTC_GetTime(RTC_Time_t* time)
{
    time_t timestamp;
    timestamp = rtc_counter_get();

    struct tm* ctime = localtime(&timestamp);

    time->year   = ctime->tm_year + 1900;
    time->month  = ctime->tm_mon + 1;
    time->day    = ctime->tm_mday;
    time->hour   = ctime->tm_hour;
    time->minute = ctime->tm_min;
    time->second = ctime->tm_sec;
}

void RTC_SetTime(const RTC_Time_t* time)
{
    time_t timestamp;
    struct tm ctime;

    ctime.tm_year = time->year - 1900;
    ctime.tm_mon  = time->month - 1;
    ctime.tm_mday = time->day;
    ctime.tm_hour = time->hour;
    ctime.tm_min  = time->minute;
    ctime.tm_sec  = time->second;

    timestamp = mktime(&ctime);

    rtc_lwoff_wait();
    rtc_counter_set(timestamp);
}

void RTC_Print(const RTC_Time_t* time)
{
    DBG_log("Year: %d, Month: %d, Day: %d, Hour: %d, Minute: %d, Second: %d\n", time->year, time->month, time->day,
            time->hour, time->minute, time->second);
}

void RTC_Test(void)
{
    RTC_Time_t time;
    time.year   = 2025;
    time.month  = 4;
    time.day    = 15;
    time.hour   = 17;
    time.minute = 30;
    time.second = 0;
    RTC_SetTime(&time);
    DWT_Delay_ms(2000);
    RTC_GetTime(&time);
    RTC_Print(&time);
}
