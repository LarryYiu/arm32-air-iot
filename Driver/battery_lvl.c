#include "battery_lvl.h"
#include "gd32f30x.h"
#include "systick.h"
#include "dwt_delay.h"
#include "RTT_Debug.h"

#define __BAT_CHARGING_VOL_COMPENSATION \
    (0.13f) // charging voltage is about 0.13V higher than actual voltage, therefore take away 0.13V to get more
            // accurate voltage
#define __BAT_ADC_RESOLUTION (4096.0f)
#define __BAT_VOLTAGE_DIVIDER_RATIO (3.0f) // voltage divider 1:3, therefore the ratio is 4
#define __BAT_VOLTAGE_DIVIDER_TOTAL (4.0f)
#define __BAT_ADC_REF_VOLTAGE (3.3f)

#define __BATTERY_VOLTAGE_MAX (4.2f)
// #define __BATTERY_LEVEL_0 (__BATTERY_VOLTAGE_MAX * 0.2f) // 3.45V
// #define __BATTERY_LEVEL_1 (__BATTERY_VOLTAGE_MAX * 0.4f) // [3.60V, 3.75V)
// #define __BATTERY_LEVEL_2 (__BATTERY_VOLTAGE_MAX * 0.6f) // [3.75V, 3.90V)
// #define __BATTERY_LEVEL_3 (__BATTERY_VOLTAGE_MAX * 0.8f) // 3.90V

static const float __BATTERY_LEVEL_THRESHOLDS[] = {
    3.45f, // 3.45V
    3.60f, // [3.60V, 3.75V)
    3.75f, // [3.75V, 3.90V)
    3.90f, // 3.90V
};

#define __BAT_ADC_AVG_FILTER_SIZE (10)

// histeresis filter to stabilize the voltage reading
// https://www.microchip.com/en-us/about/media-center/blog/2021/implementing-hysteresis-in-the-adcc
#define __BAT_HYSTERESIS_THRESHOLD (0.03f) // 30mV

void __BAT_GPIO_Config()
{
    // PC5 - ADC1_CH15
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_5);
    gpio_init(GPIOC, GPIO_MODE_IPU, GPIO_OSPEED_2MHZ,
              GPIO_PIN_13); // pull up to give a default high level when USB is not plugged in, drag down to gnd through
                            // BJT when USB is plugged in
}

void __BAT_ADC_Config()
{
    /* enable ADC clock */
    rcu_periph_clock_enable(RCU_ADC1);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC1, ADC_SCAN_MODE, ENABLE);

    /* ADC channel length config */
    adc_channel_length_config(ADC1, ADC_REGULAR_CHANNEL, 1);
    /* ADC temperature sensor channel config */
    adc_regular_channel_config(ADC1, 0, ADC_CHANNEL_15, ADC_SAMPLETIME_55POINT5);

    /* ADC external trigger enable */
    adc_external_trigger_config(ADC1, ADC_REGULAR_CHANNEL, ENABLE);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC1, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE);

    /* enable ADC interface */
    adc_enable(ADC1);
    DWT_Delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC1);
    adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL);
}

void __BAT_DMA_Config() {}

void BAT_Init()
{
    __BAT_GPIO_Config();
    __BAT_ADC_Config();
    __BAT_DMA_Config();
}

bool __IsUsbPlugged(void)
{
    return gpio_input_bit_get(GPIOC, GPIO_PIN_13) ==
           RESET; // low level means USB is plugged in, drag down to gnd through BJT
}

float BAT_GetVoltage(void)
{
    float adcVal = 0.0f;
    for(uint8_t i = 0; i < __BAT_ADC_AVG_FILTER_SIZE; i++)
    {
        adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL);
        while(SET != adc_flag_get(ADC1, ADC_FLAG_EOC));
        adcVal += adc_regular_data_read(ADC1);
    }
    adcVal /= __BAT_ADC_AVG_FILTER_SIZE;

    adcVal = adcVal * __BAT_VOLTAGE_DIVIDER_TOTAL / __BAT_VOLTAGE_DIVIDER_RATIO; // voltage divider 1:3
    adcVal = adcVal * __BAT_ADC_REF_VOLTAGE / __BAT_ADC_RESOLUTION;              // convert adc value to voltage
    if(__IsUsbPlugged())
    {
        DBG_log("Battery is charging\n");
        adcVal -= __BAT_CHARGING_VOL_COMPENSATION; // charging voltage is about 0.13V higher than actual voltage,
                                                   // therefore take away 0.13V to get more accurate voltage
    }
    return adcVal;
}

BAT_LVL_t BAT_GetLevel(float voltage)
{
    static BAT_LVL_t currLevel = BATTERY_LEVEL_NOT_INIT;
    if(currLevel == BATTERY_LEVEL_NOT_INIT)
    {
        if(voltage < __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_0])
        {
            currLevel = BATTERY_LEVEL_0;
        }
        else if(voltage < __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_1])
        {
            currLevel = BATTERY_LEVEL_1;
        }
        else if(voltage < __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_2])
        {
            currLevel = BATTERY_LEVEL_2;
        }
        else if(voltage < __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_3])
        {
            currLevel = BATTERY_LEVEL_3;
        }
        else
        {
            currLevel = BATTERY_LEVEL_4;
        }
        return currLevel;
    }

    switch(currLevel)
    {
        case(BATTERY_LEVEL_0):
            if(voltage > __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_0] + __BAT_HYSTERESIS_THRESHOLD)
            {
                currLevel = BATTERY_LEVEL_1;
            }
            break;
        case(BATTERY_LEVEL_4):
            if(voltage < __BATTERY_LEVEL_THRESHOLDS[BATTERY_LEVEL_3] - __BAT_HYSTERESIS_THRESHOLD)
            {
                currLevel = BATTERY_LEVEL_3;
            }
            break;
        default:
            if(voltage < __BATTERY_LEVEL_THRESHOLDS[currLevel - 1] - __BAT_HYSTERESIS_THRESHOLD)
            {
                currLevel--;
            }
            else if(voltage > __BATTERY_LEVEL_THRESHOLDS[currLevel] + __BAT_HYSTERESIS_THRESHOLD)
            {
                currLevel++;
            }
            break;
    }
    return currLevel;
}

void BAT_Test(void)
{
    float voltage   = BAT_GetVoltage();
    BAT_LVL_t level = BAT_GetLevel(voltage);
    uint32_t adcVal = (uint32_t)(voltage * 100.0f);
    DBG_log("Battery Voltage: %01d.%02d V, Level: %d\n", adcVal / 100, adcVal % 100, (uint8_t)level);
}
