/*
 * Digital Potentiometer Module Implementation
 */

 #include "digital_pot.h"
 #include "simplified_debug.h"
 
 // Static variables
 static SPIClass *spiInstance = NULL;
 static SemaphoreHandle_t spiMutex = NULL;
 static TaskHandle_t digitalPotTaskHandle = NULL;
 
 // Current state tracking
 static uint8_t currentValue = DIGITAL_POT_DEFAULT_VALUE;
 static bool potInitialized = false;
 
 // Helper function to transfer data to/from MCP4151 with mutex protection
 static uint8_t spiTransfer(uint8_t command, uint8_t data = 0) {
     if (spiInstance == NULL) {
         return 0xFF;  // Error value
     }
     
     uint8_t result = 0xFF;
     
     // Take the SPI mutex with timeout
     if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Select the chip
         digitalWrite(DIGITAL_POT_CS_PIN, LOW);
         
         // Small delay for stability
         ets_delay_us(1);
         
         // Send the command and data and receive response
         spiInstance->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
         
         // First byte: command
         spiInstance->transfer(command);
         
         // Second byte: data or dummy byte for read operations
         result = spiInstance->transfer(data);
         
         spiInstance->endTransaction();
         
         // Deselect the chip
         digitalWrite(DIGITAL_POT_CS_PIN, HIGH);
         
         // Release the mutex
         xSemaphoreGive(spiMutex);
     }
     
     return result;
 }
 
 // Helper function to check if the MCP4151 is responding
 static bool isDigitalPotResponding() {
     // Try to read the current value
     uint8_t testValue = readDigitalPotValue();
     
     // Set a test value
     if (!setDigitalPotValue(127)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to write test value to MCP4151");
         return false;
     }
     
     // Read back the value
     uint8_t readback = readDigitalPotValue();
     
     // Should be 127 if device is working properly
     if (readback == 0xFF) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to read back test value from MCP4151");
         return false;
     }
     
     // Restore original value if we were able to read it
     if (testValue != 0xFF) {
         setDigitalPotValue(testValue);
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "MCP4151 is responding at CS pin %d", DIGITAL_POT_CS_PIN);
     return true;
 }
 
 bool initDigitalPot(SPIClass &spi) {
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing Digital Potentiometer module");
     
     // Store the SPI reference
     spiInstance = &spi;
     
     // Create a mutex for SPI access (or use an existing one)
     spiMutex = xSemaphoreCreateMutex();
     if (spiMutex == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Digital Pot SPI mutex");
         return false;
     }
     
     // Configure the chip select pin
     pinMode(DIGITAL_POT_CS_PIN, OUTPUT);
     digitalWrite(DIGITAL_POT_CS_PIN, HIGH);  // Deselect initially
     
     // Check if the device is responding
     if (!isDigitalPotResponding()) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "MCP4151 not detected or not responding");
         return false;
     }
     
     // Set initial value
     if (!setDigitalPotValue(DIGITAL_POT_DEFAULT_VALUE)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set initial Digital Pot value");
         return false;
     }
     
     potInitialized = true;
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digital Pot initialized with value %d", DIGITAL_POT_DEFAULT_VALUE);
     return true;
 }
 
 bool setDigitalPotValue(uint8_t value) {
     // Check if initialized (but allow setting during initialization)
     if (spiInstance == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Digital Pot module not initialized");
         return false;
     }
     
     // Constrain value to the valid range
     if (value > DIGITAL_POT_MAX_VALUE) {
         value = DIGITAL_POT_MAX_VALUE;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Setting Digital Pot to value %d", value);
     
     // Send the command to write the value
     spiTransfer(DIGITAL_POT_CMD_WRITE, value);
     
     // Update the current value
     currentValue = value;
     
     return true;
 }
 
 uint8_t readDigitalPotValue() {
     if (spiInstance == NULL) {
         return 0xFF;  // Error value
     }
     
     // Send read command and receive value
     uint8_t value = spiTransfer(DIGITAL_POT_CMD_READ);
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Read Digital Pot value: %d", value);
     
     return value;
 }
 
 bool updateDigitalPotFromStrength() {
     // Map from strength range to potentiometer range
     uint8_t potValue = map(strength, STRENGTH_MIN_VALUE, STRENGTH_MAX_VALUE, 
                           DIGITAL_POT_MIN_VALUE, DIGITAL_POT_MAX_VALUE);
     
     // Only update if the value has changed
     if (potValue != currentValue) {
         //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Strength %d mapped to Digital Pot value %d", strength, potValue);
         return setDigitalPotValue(potValue);
     }
     
     return true;
 }
 
 // Task function to monitor and update digital potentiometer
 static void digitalPotTask(void *pvParameters) {
    //  DEBUG_START_TASK("Digital Pot");
    //  Serial.println("Digital Pot Task Started");
     
     // Keep track of last strength to detect changes
     uint8_t lastStrength = strength;
     
     // Main task loop
     while (1) {
         // Check if strength has changed
         if (strength != lastStrength) {
             lastStrength = strength;
             updateDigitalPotFromStrength();
         }
         
         // Sleep for a while before checking again
         vTaskDelay(pdMS_TO_TICKS(50));  // Check every 50ms
     }
     
     // Should never reach here
    //  DEBUG_END_TASK("Digital Pot");
     vTaskDelete(NULL);
 }
 
 bool createDigitalPotTask() {
     // Check if initialized
     if (!potInitialized) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create Digital Pot task - module not initialized");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating Digital Pot task");
     
     // Create the task
     BaseType_t result = xTaskCreate(
         digitalPotTask,
         "Digital Pot",
         4096,
         NULL,
         2,  // Lower priority
         &digitalPotTaskHandle
     );
     
     if (result != pdPASS) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Digital Pot task");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digital Pot task created successfully");
     return true;
 }