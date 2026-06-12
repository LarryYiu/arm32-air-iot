#include "uart.h"
#include "RTT_Debug.h"

#define USART0_DATA_ADDR (USART0 + 0x04U)
static uint8_t __dataBuffer[HK_A5_UART_BUFFER_SIZE];
static bool __isPacketReady   = false;
static uint16_t __lenReceived = 0;

void UART_Config(uint32_t baudrate)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GPIO_PIN_10);

    rcu_periph_clock_enable(RCU_USART0);
    usart_deinit(USART0);

    /* 通过USART_CTL0寄存器的WL设置字长；*/
    /* 通过USART_CTL0寄存器的PCEN设置校验位；*/
    /* 在USART_CTL1寄存器中写STB[1:0]位来设置停止位的长度；*/
    /* 以上保持默认，8位数据位，1位停止位，没有奇偶校验位 */

    /* 在USART_BAUD寄存器中设置波特率；*/
    usart_baudrate_set(USART0, baudrate);
    usart_transmit_config(USART0, USART_TRANSMIT_DISABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_interrupt_enable(USART0, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 0, 0);

    usart_enable(USART0);
}

void UART_Disable(void)
{
    rcu_periph_clock_disable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, GPIO_PIN_10);

    rcu_periph_clock_disable(RCU_DMA0);
    dma_deinit(DMA0, DMA_CH4);

    rcu_periph_clock_disable(RCU_USART0);
    usart_disable(USART0);
    usart_deinit(USART0);
}

void USART0_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE))
    {
        /* read one byte from the receive data register */
        __dataBuffer[__lenReceived++] = (uint8_t)usart_data_receive(USART0);
        // DBG_log("[USART0_IRQHandler] Byte[%d]: 0x%02X\n", __lenReceived - 1, __dataBuffer[__lenReceived - 1]);

        if(__lenReceived >= HK_A5_UART_BUFFER_SIZE) // 4 bytes for header and length
        {
            /* disable the USART0 receive interrupt */
            usart_interrupt_disable(USART0, USART_INT_RBNE);
            __isPacketReady = true;
            __lenReceived   = 0;
        }
    }
}

bool UART_IsDataReady(void)
{
    return __isPacketReady;
}
const uint8_t* UART_GetDataBuffer(void)
{
    __isPacketReady = false;
    return __dataBuffer;
}

// int fputc(int ch, FILE* f)
// {
//     usart_data_transmit(USART0, (uint8_t)ch);
//     while(usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
//     // add timeout here to avoid blocking, not necessary
//     return ch;
// }
