/*
 * GPIO Expander Tasks Module Header
 * Provides functions for TCA9534A GPIO expander in a separate task
 */

 #ifndef GPIO_EXPANDER_TASKS_H
 #define GPIO_EXPANDER_TASKS_H
 
 #include <Arduino.h>
 #include <Wire.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/semphr.h"
 #include "freertos/queue.h"
 
 // TCA9534A I2C address (default is 0x38)
 #define TCA9534A_ADDR 0x38
 
 // TCA9534A register addresses
 #define TCA9534A_REG_INPUT      0x00
 #define TCA9534A_REG_OUTPUT     0x01
 #define TCA9534A_REG_POLARITY   0x02
 #define TCA9534A_REG_CONFIG     0x03
 
 // ESP32 Interrupt pin for TCA9534A
 #define GPIO_EXPANDER_INT_PIN 11
 
 // TCA9534A GPIO pin definitions
 #define GPIO_EXPANDER_BTN0      0x01  // Port 0: Button 0
 #define GPIO_EXPANDER_BTN1      0x02  // Port 1: Button 1
 #define GPIO_EXPANDER_BTN2      0x04  // Port 2: Button 2
 #define GPIO_EXPANDER_BTN3      0x08  // Port 3: Button 3
 #define GPIO_EXPANDER_BATT_ALRT 0x10  // Port 4: Battery Alert input
 #define GPIO_EXPANDER_ELEC_SHDN 0x20  // Port 5: ELEC_SHDN output
 
 // Mask for all input pins
 #define GPIO_EXPANDER_INPUTS_MASK (GPIO_EXPANDER_BTN0 | GPIO_EXPANDER_BTN1 | \
                                   GPIO_EXPANDER_BTN2 | GPIO_EXPANDER_BTN3 | \
                                   GPIO_EXPANDER_BATT_ALRT)
 
 // Mask for all output pins
 #define GPIO_EXPANDER_OUTPUTS_MASK (GPIO_EXPANDER_ELEC_SHDN)
 
 // Button event types
 typedef enum {
     BUTTON_PRESSED,
     BUTTON_RELEASED,
     BATTERY_ALERT_ACTIVE,
     BATTERY_ALERT_INACTIVE
 } GpioExpanderEventType_t;
 
 // Button event data structure
 typedef struct {
     GpioExpanderEventType_t eventType;
     uint8_t buttonMask;       // Which button(s) triggered the event
     unsigned long timestamp;  // Timestamp of the event
 } GpioExpanderEvent_t;
 
 // GPIO Expander status data structure
 typedef struct {
     uint8_t inputState;       // Current state of all inputs
     uint8_t outputState;      // Current state of all outputs
     bool success;             // Whether reading was successful
 } GpioExpanderStatus_t;
 
 /**
  * Initialize the GPIO expander module
  * @param wire Reference to I2C instance
  * @return true if initialization was successful
  */
 bool initGpioExpanderModule(TwoWire &wire);
 
 /**
  * Create a GPIO expander monitoring task
  * This task will monitor the interrupt pin and read button states
  * Unlike other tasks, this is a persistent task that doesn't self-delete
  * @return true if task creation was successful
  */
 bool createGpioExpanderTask();
 
 /**
  * Receive the latest GPIO expander status
  * @param status Pointer to store the GPIO expander status
  * @param timeout Maximum time to wait for results
  * @return true if results were received
  */
 bool receiveGpioExpanderStatus(GpioExpanderStatus_t *status, TickType_t timeout);
 
 /**
  * Wait for a button event (with optional filtering)
  * @param event Pointer to store the button event
  * @param buttonMask Optional mask to filter specific buttons (0 for any button)
  * @param timeout Maximum time to wait for an event
  * @return true if an event was received
  */
 bool waitForButtonEvent(GpioExpanderEvent_t *event, uint8_t buttonMask, TickType_t timeout);
 
 /**
  * Set the state of an output pin
  * @param pin The pin mask (e.g., GPIO_EXPANDER_ELEC_SHDN)
  * @param state The desired state (true=HIGH, false=LOW)
  * @return true if successful
  */
 bool setGpioExpanderOutput(uint8_t pin, bool state);
 
 /**
  * Get the current state of the ELEC_SHDN pin
  * @return true if ELEC_SHDN is HIGH, false if LOW
  */
 bool getElecShutdownState();
 
 /**
  * Set the ELEC_SHDN output
  * @param shutdown true to enable shutdown, false to disable
  * @return true if successful
  */
 bool setElecShutdown(bool shutdown);
 
 #endif // GPIO_EXPANDER_TASKS_H