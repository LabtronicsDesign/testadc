/*
 * GPIO Expander Tasks Module Implementation
 */

 #include "gpio_expander_tasks.h"
 #include "simplified_debug.h"
 #include "beeper.h"
 
 // Static variables
 static TwoWire *i2cWire = NULL;
 static SemaphoreHandle_t i2cMutex = NULL;
 static QueueHandle_t gpioExpanderStatusQueue = NULL;
 static QueueHandle_t buttonEventQueue = NULL;
 static TaskHandle_t gpioExpanderTaskHandle = NULL;
 
 // Keep track of pin states
 static volatile uint8_t lastInputState = 0;
 static volatile uint8_t currentOutputState = 0;
 static volatile bool interruptOccurred = false;
 
 // IRAM_ATTR needed for ESP32 interrupt handlers
 void IRAM_ATTR gpioExpanderISR() {
     interruptOccurred = true;
     // Wake up the GPIO expander task if it exists
     if (gpioExpanderTaskHandle != NULL) {
         BaseType_t xHigherPriorityTaskWoken = pdFALSE;
         vTaskNotifyGiveFromISR(gpioExpanderTaskHandle, &xHigherPriorityTaskWoken);
         if (xHigherPriorityTaskWoken) {
             portYIELD_FROM_ISR();
         }
     }
 }
 
 // Helper function to read a TCA9534A register with mutex protection
 static bool readTCA9534ARegister(uint8_t reg, uint8_t *value) {
     if (i2cWire == NULL || value == NULL) {
         return false;
     }
     
     bool success = false;
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Set register pointer
         i2cWire->beginTransmission(TCA9534A_ADDR);
         i2cWire->write(reg);
         if (i2cWire->endTransmission() == 0) {
             // Read 1 byte
             if (i2cWire->requestFrom(TCA9534A_ADDR, 1) == 1) {
                 *value = i2cWire->read();
                 success = true;
             }
         }
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     return success;
 }
 
 // Helper function to write a TCA9534A register with mutex protection
 static bool writeTCA9534ARegister(uint8_t reg, uint8_t value) {
     if (i2cWire == NULL) {
         return false;
     }
     
     bool success = false;
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Write register
         i2cWire->beginTransmission(TCA9534A_ADDR);
         i2cWire->write(reg);
         i2cWire->write(value);
         success = (i2cWire->endTransmission() == 0);
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     return success;
 }
 
 // Task function to monitor GPIO expander
 static void gpioExpanderTask(void *pvParameters) {
     DEBUG_START_TASK("GPIO Expander");
     Serial.println("GPIO Expander Task Started");
     
     // Store task handle for interrupt notification
     gpioExpanderTaskHandle = xTaskGetCurrentTaskHandle();
     
     // Local variables for task state
     GpioExpanderStatus_t status;
     GpioExpanderEvent_t event;
     
     // Initial read of the input state to establish baseline
     uint8_t tempInputState = 0;
     if (!readTCA9534ARegister(TCA9534A_REG_INPUT, &tempInputState)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Initial GPIO expander input read failed");
     } else {
         lastInputState = tempInputState;
     }
     
     // Initialize status and send it to the queue
     status.inputState = lastInputState;
     status.outputState = currentOutputState;
     status.success = true;
     xQueueOverwrite(gpioExpanderStatusQueue, &status);
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO Expander initial state - Inputs: 0x%02X, Outputs: 0x%02X", lastInputState, currentOutputState);
     
     while (1) {
         // Wait for interrupt or timeout (poll every 100ms as a backup)
         if (interruptOccurred || ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
             interruptOccurred = false;
             
             // Read input register
             uint8_t inputState = 0;
             bool readSuccess = readTCA9534ARegister(TCA9534A_REG_INPUT, &inputState);
             
             if (readSuccess) {
                 // Update status
                 status.inputState = inputState;
                 status.outputState = currentOutputState;
                 status.success = true;
                 
                 // Detect changes and generate events
                 uint8_t changedInputs = inputState ^ lastInputState;
                 
                 if (changedInputs != 0) {
                     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO inputs changed: 0x%02X -> 0x%02X", lastInputState, inputState);
                     
                     // Check each button and the battery alert
                     for (int i = 0; i < 5; i++) {
                         uint8_t mask = (1 << i);
                         
                         // Check if this pin changed
                         if (changedInputs & mask) {
                             // Create an event
                             event.timestamp = millis();
                             event.buttonMask = mask;
                             
                             // Determine event type (assuming active low buttons)
                             bool pinState = (inputState & mask) == 0;
                             
                             if (i < 4) {
                                 // Buttons
                                 event.eventType = pinState ? BUTTON_PRESSED : BUTTON_RELEASED;
                                 //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button %d %s", i, pinState ? "PRESSED" : "RELEASED");
                                 
                                 // Trigger a beep when a button is pressed
                                 if (pinState) {
                                     buttonBeep();
                                 }
                             } else {
                                 // Battery alert
                                 event.eventType = pinState ? BATTERY_ALERT_ACTIVE : BATTERY_ALERT_INACTIVE;
                                 //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery Alert %s", pinState ? "ACTIVE" : "INACTIVE");
                             }
                             
                             // Send to event queue, don't block if queue is full
                             xQueueSend(buttonEventQueue, &event, 0);
                         }
                     }
                     
                     // Update last state
                     lastInputState = inputState;
                 }
                 
                 // Send current status to status queue, replacing any old status
                 xQueueOverwrite(gpioExpanderStatusQueue, &status);
             } else {
                 //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to read GPIO expander input register");
             }
         }
     }
     
     // Should never reach here under normal operation
     gpioExpanderTaskHandle = NULL;
     DEBUG_END_TASK("GPIO Expander");
     vTaskDelete(NULL);
 }
 
 bool initGpioExpanderModule(TwoWire &wire) {
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing GPIO Expander module");
     
     // Store the I2C reference
     i2cWire = &wire;
     
     // Create a mutex for I2C access
     i2cMutex = xSemaphoreCreateMutex();
     if (i2cMutex == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create GPIO Expander I2C mutex");
         return false;
     }
     
     // Create the status queue (only keeps the latest status)
     gpioExpanderStatusQueue = xQueueCreate(1, sizeof(GpioExpanderStatus_t));
     if (gpioExpanderStatusQueue == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create GPIO Expander status queue");
         vSemaphoreDelete(i2cMutex);
         return false;
     }
     
     // Create the event queue (can hold multiple events)
     buttonEventQueue = xQueueCreate(10, sizeof(GpioExpanderEvent_t));
     if (buttonEventQueue == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create button event queue");
         vQueueDelete(gpioExpanderStatusQueue);
         vSemaphoreDelete(i2cMutex);
         return false;
     }
     
     // Configure the interrupt pin
     pinMode(GPIO_EXPANDER_INT_PIN, INPUT_PULLUP);
     
     // Set up the TCA9534A
     // 1. Configure pins (inputs and outputs)
     // The TCA9534A configuration register: 1=input, 0=output
     if (!writeTCA9534ARegister(TCA9534A_REG_CONFIG, GPIO_EXPANDER_INPUTS_MASK)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to configure GPIO Expander pins");
         return false;
     }
     
     // 2. Set initial output values (all off)
     currentOutputState = 0;
     if (!writeTCA9534ARegister(TCA9534A_REG_OUTPUT, currentOutputState)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set initial GPIO Expander outputs");
         return false;
     }
     
     // 3. Read initial input values
     uint8_t tempInputState = 0;
     if (!readTCA9534ARegister(TCA9534A_REG_INPUT, &tempInputState)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to read initial GPIO Expander inputs");
         return false;
     }
     lastInputState = tempInputState;
     
     // Attach interrupt for the INT pin (active low, so trigger on falling edge)
     attachInterrupt(digitalPinToInterrupt(GPIO_EXPANDER_INT_PIN), gpioExpanderISR, FALLING);
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO Expander module initialized successfully");
     return true;
 }
 
 bool createGpioExpanderTask() {
     if (i2cWire == NULL || i2cMutex == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create GPIO Expander task - module not initialized");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating GPIO Expander task");
     
     // Create the task
     BaseType_t result = xTaskCreate(
         gpioExpanderTask,
         "GPIO Expander",
         4096,
         NULL,
         4,  // Medium-high priority (higher than control task, lower than ADC/battery)
         &gpioExpanderTaskHandle
     );
     
     if (result != pdPASS) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create GPIO Expander task");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO Expander task created successfully");
     return true;
 }
 
 bool receiveGpioExpanderStatus(GpioExpanderStatus_t *status, TickType_t timeout) {
     if (status == NULL || gpioExpanderStatusQueue == NULL) {
         return false;
     }
     
     return (xQueuePeek(gpioExpanderStatusQueue, status, timeout) == pdPASS);
 }
 
 bool waitForButtonEvent(GpioExpanderEvent_t *event, uint8_t buttonMask, TickType_t timeout) {
     if (event == NULL || buttonEventQueue == NULL) {
         return false;
     }
     
     // Wait for an event
     if (xQueueReceive(buttonEventQueue, event, timeout) != pdPASS) {
         return false;
     }
     
     // If no specific button mask is provided, accept any button
     if (buttonMask == 0) {
         return true;
     }
     
     // Check if the received event matches the requested button mask
     return ((event->buttonMask & buttonMask) != 0);
 }
 
 bool setGpioExpanderOutput(uint8_t pin, bool state) {
     // Ensure pin is in the outputs mask
     if ((pin & GPIO_EXPANDER_OUTPUTS_MASK) == 0) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Invalid GPIO Expander output pin: 0x%02X", pin);
         return false;
     }
     
     // Update the output state
     uint8_t newState = currentOutputState;
     if (state) {
         newState |= pin;  // Set bit
     } else {
         newState &= ~pin; // Clear bit
     }
     
     // Only write if changed
     if (newState != currentOutputState) {
         if (writeTCA9534ARegister(TCA9534A_REG_OUTPUT, newState)) {
             currentOutputState = newState;
             //DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO Expander output set - Pin: 0x%02X, State: %d", pin, state);
             return true;
         } else {
             //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set GPIO Expander output");
             return false;
         }
     }
     
     // No change needed
     return true;
 }
 
 bool getElecShutdownState() {
     return (currentOutputState & GPIO_EXPANDER_ELEC_SHDN) != 0;
 }
 
 bool setElecShutdown(bool shutdown) {
     return setGpioExpanderOutput(GPIO_EXPANDER_ELEC_SHDN, shutdown);
 }