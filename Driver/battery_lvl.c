#include "battery_lvl.h"
#include "gd32f30x.h"
#include "systick.h"
#include "dwt_delay.h"
#include "RTT_Debug.h"

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
    adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL);
    float adcVal;
    while(SET != adc_flag_get(ADC1, ADC_FLAG_EOC));
    adcVal = (float)adc_regular_data_read(ADC1);
    adcVal += adcVal / 3; // voltage divider 1:3
    if(__IsUsbPlugged())
    {
        DBG_log("Battery is charging\n");
        adcVal -= 0.13f; // charging voltage is about 0.13V higher than actual voltage, therefore take away 0.13V to get
                         // more accurate voltage
    }
    return adcVal * 3.3f / 4096.0f;
}

void BAT_Test(void)
{
    uint32_t adcVal = (uint32_t)(BAT_GetVoltage() * 100.0f);
    DBG_log("Battery Voltage: %01d.%02d V\n", adcVal / 100, adcVal % 100);
}
