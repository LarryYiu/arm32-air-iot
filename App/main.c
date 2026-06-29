#include "app_driver.h"
#include "task_manager.h"

int main()
{
    APP_Init();
    TASK_MANAGER_Init();

    while(1)
    {
        // APP_Run();
    }
}
