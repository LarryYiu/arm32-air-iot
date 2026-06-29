#include "task_manager.h"
#include "key_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void TASK_MANAGER_Init(void)
{
    KEY_RTOS_Init();

    vTaskStartScheduler();
}
