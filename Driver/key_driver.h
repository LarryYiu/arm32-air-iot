#ifndef __KEY_DRV_H__
#define __KEY_DRV_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Configure the key driver. This function initializes the GPIO pins for the keys and sets up the initial state
 * for each key.
 */
void KEY_Config(void);

/**
 * @brief Enable the key driver. This function configures the GPIO pins for the keys to be in input mode, allowing
 * them to be read.
 *
 * @note The keys on the dev board have external pull-up resistors, so they will read as high when not pressed and low
 * when pressed. GPIO will be set to floating input mode when enabled.
 */
void KEY_Enable(void);

/**
 * @brief Disable the key driver. This function configures the GPIO pins for the keys to be in analog input mode
 * to reduces power consumption.
 */
void KEY_Disable(void);

/**
 * @brief Set whether the key should trigger events on release.
 * @param keyIndex The index of the key to configure.
 * @param triggerOnRelease If true, the key will trigger events when released. If false, it will trigger events when
 * pressed. By default, keys are set to trigger on press (KEY_RELEASE_TRIGGER_DEFAULT is false).
 *
 * @note continuous press events does not support trigger on press mode, If triggerOnRelease is set to false for a key,
 * continuous press events will not be triggered for that key.
 */
void KEY_SetTriggerOnRelease(uint8_t keyIndex, bool triggerOnRelease);

/**
 * @brief Add a listener for short press events on a key.
 * @param keyIndex The index of the key to add the listener to.
 * @param listener A function pointer to the listener function that will be called when a short press event occurs.
 * The listener function should have the following signature: void listener(void);
 */
void KEY_AddShortPressListener(uint8_t keyIndex, void (*listener)(void));

/**
 * @brief Add a listener for long press events on a key.
 * @param keyIndex The index of the key to add the listener to.
 * @param listener A function pointer to the listener function that will be called when a long press event occurs.
 * The listener function should have the following signature: void listener(void);
 */
void KEY_AddLongPressListener(uint8_t keyIndex, void (*listener)(void));

/**
 * @brief Add a listener for continuous press events on a key. A continuous press event is triggered when a key is
 * pressed and released multiple times within a short period (defined by KEY_CONTINUOUS_PRESS_THRESHOLD).
 * @param keyIndex The index of the key to add the listener to.
 * @param listener A function pointer to the listener function that will be called when a continuous press event occurs.
 * The listener function should have the following signature: void listener(uint8_t count); where count is the number of
 * presses detected in the continuous press event.
 */
void KEY_AddContinuousPressListener(uint8_t keyIndex, void (*listener)(uint8_t));

/**
 * @brief Remove the listener for short press events on a key.
 * @param keyIndex The index of the key to remove the listener from.
 */
void KEY_RemoveShortPressListener(uint8_t keyIndex);

/**
 * @brief Remove the listener for long press events on a key.
 * @param keyIndex The index of the key to remove the listener from.
 */
void KEY_RemoveLongPressListener(uint8_t keyIndex);

/**
 * @brief Remove the listener for continuous press events on a key.
 * @param keyIndex The index of the key to remove the listener from.
 */
void KEY_RemoveContinuousPressListener(uint8_t keyIndex);

/**
 * @brief Scan the state of a key. This function should be called periodically to update the state of the keys and
 * trigger the appropriate events, may be used in the main loop of the application.
 * @param keyIndex The index of the key to scan.
 */
void KEY_Scan(uint8_t keyIndex);

#endif // __KEY_DRV_H__
