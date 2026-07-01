#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp8684_driver.h"
#include "gd32f30x.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define USART1_DATA_ADDR (USART1 + 0x04U) // USART_DATA register address
#define PACKET_DATA_LEN (1024)
static char __dataRecvBuffer[PACKET_DATA_LEN];
static uint32_t __dataLen;
static SemaphoreHandle_t __dmaSemaphore;
static SemaphoreHandle_t __uartMutex;

static void __GPIO_Config(void)
{
    /* Enable pin of wifi module */
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_7);
    gpio_bit_set(GPIOA, GPIO_PIN_7); // set PA7 high to power on the WiFi module

    /* Configure UART pins */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GPIO_PIN_3);
}

static void __UART_Config(void)
{
    rcu_periph_clock_enable(RCU_USART1);
    usart_deinit(USART1);
    usart_baudrate_set(USART1, 115200U);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_interrupt_enable(USART1, USART_INT_IDLE);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_ENABLE); // enable DMA for USART1 reception
    // nvic_irq_enable(USART1_IRQn, 0, 0);
    nvic_irq_enable(USART1_IRQn, 6,
                    0); // Set the priority of USART1 interrupt to 6 (for FreeRTOS semaphore in ISR)
    usart_enable(USART1);
}

static void __DMA_Config(void)
{
    rcu_periph_clock_enable(RCU_DMA0);
    dma_deinit(DMA0, DMA_CH5);

    dma_parameter_struct dmaStruct;
    dma_struct_para_init(&dmaStruct);
    /* Direction */
    dmaStruct.direction = DMA_PERIPHERAL_TO_MEMORY;
    /* Source address */
    dmaStruct.periph_addr = USART1_DATA_ADDR;
    /* Source address increment */
    dmaStruct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    /* Source data width */
    dmaStruct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;

    /* Destination address */
    dmaStruct.memory_addr = (uint32_t)__dataRecvBuffer;
    /* Destination address increment */
    dmaStruct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    /* Destination data width */
    dmaStruct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    /* Max number of data to transfer */
    dmaStruct.number = PACKET_DATA_LEN;
    /* DMA channel priority */
    dmaStruct.priority = DMA_PRIORITY_HIGH;
    dma_init(DMA0, DMA_CH5, &dmaStruct);
    /* Enable DMA channel */
    dma_channel_enable(DMA0, DMA_CH5);
}

void ESP8684_EnableModule(void)
{
    gpio_bit_set(GPIOG, GPIO_PIN_7); // set PG7 high to power on the WiFi module
}

void ESP8684_DisableModule(void)
{
    gpio_bit_reset(GPIOG, GPIO_PIN_7); // set PG7 low to power off the WiFi module
}

void ESP8684_Init(void)
{
    __GPIO_Config();

    __UART_Config();

    __DMA_Config();

    __dmaSemaphore = xSemaphoreCreateBinary();
    __uartMutex    = xSemaphoreCreateMutex();

    configASSERT(__dmaSemaphore != NULL);
    configASSERT(__uartMutex != NULL);

    // ESP8684_DisableModule();
    // ESP8684_EnableModule();
}

void ESP8684_SendCommand(const char* cmd)
{
    for(uint8_t i = 0; cmd[i] != '\0'; i++)
    {
        while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
        usart_data_transmit(USART1, cmd[i]);
    }
}

void ESP8684_SnapshotResponse(char* buffer)
{
    memcpy(buffer, __dataRecvBuffer, __dataLen);
    memset(__dataRecvBuffer, 0, __dataLen); // clear the buffer after copying
}

void USART1_IRQHandler(void)
{
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE) != RESET)
    {
        usart_data_receive(USART1);
        __dataLen = PACKET_DATA_LEN - dma_transfer_number_get(DMA0, DMA_CH5);
        // xSemaphoreGiveFromISR(__dmaSemaphore, NULL);
        dma_channel_disable(DMA0, DMA_CH5);
        dma_transfer_number_config(DMA0, DMA_CH5, PACKET_DATA_LEN);
        dma_channel_enable(DMA0, DMA_CH5);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(__dmaSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

BaseType_t ESP8684_WaitForPacketSemaphore(uint32_t timeoutMs)
{
    return xSemaphoreTake(__dmaSemaphore, pdMS_TO_TICKS(timeoutMs));
}

void ESP8684_LockUART(void)
{
    xSemaphoreTake(__uartMutex, portMAX_DELAY);
}

void ESP8684_UnlockUART(void)
{
    xSemaphoreGive(__uartMutex);
}
