#include "task_manager.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "key_driver.h"
#include "wifi_app.h"

void TASK_MANAGER_Init(void)
{
    KEY_RTOS_Init();
    // WIFI_RTOS_Init();

    vTaskStartScheduler();
}
