#include <stdio.h>
#include "key_driver.h"
#include "gd32f30x.h"
#include "config.h"
#include "gpio_decoder.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "RTT_Debug.h"

#ifndef KEY_GPIO_FREQENCY
#define KEY_GPIO_FREQENCY GPIO_OSPEED_2MHZ
#endif

#ifndef KEY_RELEASE_TRIGGER_DEFAULT
#define KEY_RELEASE_TRIGGER_DEFAULT true
#endif

#ifndef KEY_DEBOUNCE_TIME_MS
#define KEY_DEBOUNCE_TIME_MS 10
#endif

#ifndef KEY_LONG_PRESS_THRESHOLD_MS
#define KEY_LONG_PRESS_THRESHOLD_MS 1000
#endif

#ifndef KEY_CONTINUOUS_PRESS_THRESHOLD_MS
#define KEY_CONTINUOUS_PRESS_THRESHOLD_MS 300
#endif

#define KEY_NUM len(__KEY_IDEN_LOOKUP)

typedef struct Key Key_t;
struct Key
{
    uint32_t gpioPort;
    uint32_t gpioPin;
    uint64_t pressedAt;
    uint8_t pressCount;
    uint64_t lastReleasedAt;
    bool isPressing;
    bool isTriggerOnRelease;
    bool isShortPressCalled;
    bool isLongPressCalled;
    void (*onShortPress)(void);
    void (*onLongPress)(void);
    void (*onContinuousPress)(uint8_t);
    void (*stateHandler)(Key_t* key);
};

static void __OnReleasedState(Key_t* key);
static void __OnDebounceState(Key_t* key);
static void __OnPressedState(Key_t* key);
static void __OnLongPressedState(Key_t* key);

static const uint8_t __KEY_IDEN_LOOKUP[] = {KEY_LOOK_UP};
static Key_t __KEYS[KEY_NUM];

void KEY_Init(void)
{
    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        __KEYS[i].gpioPort           = GPIO_GetPeriphAddr(__KEY_IDEN_LOOKUP[i]);
        __KEYS[i].gpioPin            = GPIO_GetPinAddr(__KEY_IDEN_LOOKUP[i]);
        __KEYS[i].pressedAt          = 0;
        __KEYS[i].pressCount         = 0;
        __KEYS[i].lastReleasedAt     = 0;
        __KEYS[i].isPressing         = false;
        __KEYS[i].isTriggerOnRelease = KEY_RELEASE_TRIGGER_DEFAULT;
        __KEYS[i].isShortPressCalled = false;
        __KEYS[i].isLongPressCalled  = false;
        __KEYS[i].onShortPress       = NULL;
        __KEYS[i].onLongPress        = NULL;
        __KEYS[i].onContinuousPress  = NULL;
        __KEYS[i].stateHandler       = __OnReleasedState;
        rcu_periph_clock_enable(GPIO_GetRcuPeriph(__KEY_IDEN_LOOKUP[i]));
        gpio_init(__KEYS[i].gpioPort, GPIO_MODE_IPU, KEY_GPIO_FREQENCY, __KEYS[i].gpioPin);
    }
}
void KEY_SetTriggerOnRelease(uint8_t keyIndex, bool triggerOnRelease)
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].isTriggerOnRelease = triggerOnRelease;
    }
}

void KEY_Enable(void)
{
    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        gpio_init(__KEYS[i].gpioPort, GPIO_MODE_IPU, KEY_GPIO_FREQENCY, __KEYS[i].gpioPin);
    }
}

void KEY_Disable(void)
{
    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        gpio_init(__KEYS[i].gpioPort, GPIO_MODE_AIN, KEY_GPIO_FREQENCY, __KEYS[i].gpioPin);
    }
}

void KEY_AddShortPressListener(uint8_t keyIndex, void (*listener)(void))
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onShortPress = listener;
    }
}

void KEY_AddLongPressListener(uint8_t keyIndex, void (*listener)(void))
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onLongPress = listener;
    }
}

void KEY_AddContinuousPressListener(uint8_t keyIndex, void (*listener)(uint8_t))
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onContinuousPress = listener;
    }
}

void KEY_RemoveShortPressListener(uint8_t keyIndex)
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onShortPress = NULL;
    }
}

void KEY_RemoveLongPressListener(uint8_t keyIndex)
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onLongPress = NULL;
    }
}

void KEY_RemoveContinuousPressListener(uint8_t keyIndex)
{
    if(keyIndex < KEY_NUM)
    {
        __KEYS[keyIndex].onContinuousPress = NULL;
    }
}

/**
 * @brief Check if a key is currently being pressed. This function reads the GPIO pin associated with the key and
 * returns true if the key is currently pressed (active low).
 * @param key A pointer to the Key_t structure representing the key to check.
 * @return true if the key is currently pressed, false otherwise.
 */
static bool __IsNowPressing(Key_t* key)
{
    return gpio_input_bit_get(key->gpioPort, key->gpioPin) == RESET;
}

void KEY_Scan(uint8_t keyIndex)
{
    if(keyIndex >= KEY_NUM)
        return;
    if(__KEYS[keyIndex].stateHandler != NULL)
    {
        __KEYS[keyIndex].stateHandler(__KEYS + keyIndex);
    }
}

/**
 * @brief Handle the released state of a key. This function is called when a key is detected as being released. It
 * checks whether the key was previously pressing and whether it should trigger on release. If the key should trigger on
 * release and was previously pressing, it checks the duration of the press to determine whether to trigger a short
 * press or a continuous press event. If the key should not trigger on release, it simply checks if the key is now
 * pressing to potentially transition to the debounce state.
 * @param key A pointer to the Key_t structure representing the key to handle.
 * @note This function does not handle long press event, which is handled in the `__OnLongPressedState` state handler.
 */
static void __OnReleasedState(Key_t* key)
{
    if(key->isPressing && key->isTriggerOnRelease)
    {
        uint64_t now = xTaskGetTickCount();
        if(now - key->lastReleasedAt >= KEY_CONTINUOUS_PRESS_THRESHOLD_MS)
        {
            key->lastReleasedAt = 0;
            if(key->pressCount >= 2 && key->onContinuousPress != NULL)
            {
                key->onContinuousPress(key->pressCount);
            }
            else if(key->onShortPress != NULL)
            {
                key->onShortPress();
            }
            key->pressCount = 0;
            key->isPressing = false;
        }
        else if(__IsNowPressing(key))
        {
            key->stateHandler = __OnDebounceState;
        }
    }
    else
    {
        if(__IsNowPressing(key))
        {
            key->stateHandler = __OnDebounceState;
        }
    }
}

/**
 * @brief Handle the debounce state of a key. If the key is confirmed to be
 * pressed after the debounce period, it transitions to the pressed state. If the key is not pressed, it transitions
 * back to the released state.
 * @param key A pointer to the Key_t structure representing the key to handle.
 */
static void __OnDebounceState(Key_t* key)
{
    vTaskDelay(pdMS_TO_TICKS(KEY_DEBOUNCE_TIME_MS));
    if(__IsNowPressing(key))
    {
        key->isPressing   = true;
        key->pressedAt    = xTaskGetTickCount();
        key->stateHandler = __OnPressedState;
    }
    else
    {
        key->isPressing   = false;
        key->stateHandler = __OnReleasedState;
    }
}

/**
 * @brief Handle the pressed state of a key. This function is called when a key is confirmed to be pressed after the
 * debounce period.
 *
 * @param key A pointer to the Key_t structure representing the key to handle.
 */
static void __OnPressedState(Key_t* key)
{
    if(!key->isTriggerOnRelease && !key->isShortPressCalled && key->onShortPress != NULL)
    {
        key->onShortPress();
        key->isShortPressCalled = true;
    }
    else
    {
        uint64_t now = xTaskGetTickCount();
        if(__IsNowPressing(key))
        {
            if(now - key->pressedAt >= KEY_LONG_PRESS_THRESHOLD_MS)
            {
                key->stateHandler = __OnLongPressedState;
            }
        }
        else
        {
            key->pressCount++;
            key->isShortPressCalled = false;
            key->lastReleasedAt     = now;
            key->stateHandler       = __OnReleasedState;
        }
    }
}
/**
 * @brief Handle the long pressed state of a key.
 *
 * @param key A pointer to the Key_t structure representing the key to handle.
 */
static void __OnLongPressedState(Key_t* key)
{
    if(!__IsNowPressing(key))
    {
        uint64_t now            = xTaskGetTickCount();
        key->isShortPressCalled = false;
        key->isLongPressCalled  = false;
        key->lastReleasedAt     = now;
        key->stateHandler       = __OnReleasedState;
        if(key->onLongPress != NULL && key->isTriggerOnRelease)
        {
            key->onLongPress();
        }
        key->pressCount     = 0;
        key->lastReleasedAt = now;
        key->isPressing     = false;
    }
    else
    {
        if(key->onLongPress != NULL && !key->isTriggerOnRelease && !key->isLongPressCalled)
        {
            key->onLongPress();
            key->isLongPressCalled = true;
        }
    }
}

void __KEY_RTOS_Task(void* arg)
{
    while(1)
    {
        if(arg == NULL)
        {
            KEY_Scan(0);
        }
        else
        {
            uint8_t keyIndex = *((uint8_t*)arg);
            KEY_Scan(keyIndex);
        }
    }
}

void KEY_RTOS_Init(void)
{
    BaseType_t ret = xTaskCreate(__KEY_RTOS_Task, "key", 512, NULL, tskIDLE_PRIORITY + 1, NULL);

    configASSERT(ret == pdPASS);
}
