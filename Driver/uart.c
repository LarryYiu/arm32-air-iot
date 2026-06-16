#include "uart.h"
#include "RTT_Debug.h"

#define USART0_DATA_ADDR (USART0 + 0x04U)
static uint8_t __dataBuffer[HK_A5_UART_BUFFER_SIZE];
static bool __isPacketReady = false;
// static uint16_t __lenReceived = 0;

void UART_Config(uint32_t baudrate)
{
    /* configure GPIO for USART0 TX and RX */
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GPIO_PIN_10);

    /* configure USART0 */
    rcu_periph_clock_enable(RCU_USART0);
    usart_deinit(USART0);

    usart_baudrate_set(USART0, baudrate);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_interrupt_enable(USART0, USART_INT_IDLE);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
    nvic_irq_enable(USART0_IRQn, 0, 0);
    usart_enable(USART0);

    /* configure DMA for USART0 RX */
    rcu_periph_clock_enable(RCU_DMA0);
    dma_deinit(DMA0, DMA_CH4);

    dma_parameter_struct dmaStruct;
    dma_struct_para_init(&dmaStruct);
    dmaStruct.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dmaStruct.periph_addr  = USART0_DATA_ADDR;
    dmaStruct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dmaStruct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;

    dmaStruct.memory_addr  = (uint32_t)__dataBuffer;
    dmaStruct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dmaStruct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dmaStruct.number       = HK_A5_UART_BUFFER_SIZE;
    dmaStruct.priority     = DMA_PRIORITY_HIGH;
    dma_init(DMA0, DMA_CH4, &dmaStruct);
    dma_channel_enable(DMA0, DMA_CH4);
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
    // if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE))
    // {
    //     /* read one byte from the receive data register */
    //     __dataBuffer[__lenReceived++] = (uint8_t)usart_data_receive(USART0);
    //     // DBG_log("[USART0_IRQHandler] Byte[%d]: 0x%02X\n", __lenReceived - 1, __dataBuffer[__lenReceived - 1]);

    //     if(__lenReceived >= HK_A5_UART_BUFFER_SIZE) // 4 bytes for header and length
    //     {
    //         /* disable the USART0 receive interrupt */
    //         usart_interrupt_disable(USART0, USART_INT_RBNE);
    //         __isPacketReady = true;
    //         __lenReceived   = 0;
    //     }
    // }
    if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET)
    {
        usart_data_receive(USART0);
        if(HK_A5_UART_BUFFER_SIZE == (HK_A5_UART_BUFFER_SIZE - dma_transfer_number_get(DMA0, DMA_CH4)))
        {
            __isPacketReady = true;
        }
        dma_channel_disable(DMA0, DMA_CH4);
        dma_transfer_number_config(DMA0, DMA_CH4, HK_A5_UART_BUFFER_SIZE);
        dma_channel_enable(DMA0, DMA_CH4);
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
