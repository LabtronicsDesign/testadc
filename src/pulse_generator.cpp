/*
 * Pulse Generator Module Implementation
 */

 #include "pulse_generator.h"
 #include "simplified_debug.h"
 
 // Static variables
 static TwoWire *i2cWire = NULL;
 static SemaphoreHandle_t i2cMutex = NULL;
 static TaskHandle_t pulseGeneratorTaskHandle = NULL;
 
 // Current state tracking
 static uint16_t currentFrequency = 0;
 static bool currentlyEnabled = false;
 static bool pca9685Initialized = false;
 
 // Helper function to read a PCA9685 register with mutex protection
 static bool readPCA9685Register(uint8_t reg, uint8_t *value) {
     if (i2cWire == NULL || value == NULL) {
         return false;
     }
     
     bool success = false;
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Set register pointer
         i2cWire->beginTransmission(PCA9685_ADDR);
         i2cWire->write(reg);
         if (i2cWire->endTransmission() == 0) {
             // Read 1 byte
             if (i2cWire->requestFrom(PCA9685_ADDR, 1) == 1) {
                 *value = i2cWire->read();
                 success = true;
             }
         }
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     return success;
 }
 
 // Helper function to write a PCA9685 register with mutex protection
 static bool writePCA9685Register(uint8_t reg, uint8_t value) {
     if (i2cWire == NULL) {
         return false;
     }
     
     bool success = false;
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Write register
         i2cWire->beginTransmission(PCA9685_ADDR);
         i2cWire->write(reg);
         i2cWire->write(value);
         success = (i2cWire->endTransmission() == 0);
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     return success;
 }
 
 // Calculate prescale value for desired frequency
 static uint8_t calculatePrescale(uint16_t freq) {
     // Constrain frequency to valid range
     if (freq < PULSE_MIN_FREQ) freq = PULSE_MIN_FREQ;
     if (freq > PULSE_MAX_FREQ) freq = PULSE_MAX_FREQ;
     
     // Formula from datasheet: prescale = round(25MHz / (4096 * freq)) - 1
     float prescaleval = 25000000.0;  // 25MHz
     prescaleval /= 4096.0;           // 12-bit resolution
     prescaleval /= freq;
     prescaleval -= 1.0;
     
     return (uint8_t)(prescaleval + 0.5f); // Round to nearest
 }
 
 // Set PWM on a specific channel with a 50% duty cycle
 static bool setPWM(uint8_t channel, uint16_t on, uint16_t off) {
     if (i2cWire == NULL) {
         return false;
     }
     
     bool success = false;
     
     // Calculate register addresses for this channel
     uint8_t channelBase = PCA9685_LED0_ON_L + (channel * 4);
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Write all four registers
         i2cWire->beginTransmission(PCA9685_ADDR);
         i2cWire->write(channelBase);
         i2cWire->write(on & 0xFF);         // ON_L
         i2cWire->write(on >> 8);           // ON_H
         i2cWire->write(off & 0xFF);        // OFF_L
         i2cWire->write(off >> 8);          // OFF_H
         success = (i2cWire->endTransmission() == 0);
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     return success;
 }
 
 // Set 50% duty cycle on a channel (2048 ticks, half of 4096)
 static bool set50PercentDutyCycle(uint8_t channel) {
     // For 50% duty cycle, we set ON at 0 and OFF at 2048 (half of 4096)
     return setPWM(channel, 0, 2048);
 }
 
 // Helper function to check if PCA9685 is present on the I2C bus
 static bool isPCA9685Present() {
     if (i2cWire == NULL) {
         return false;
     }
     
     bool deviceFound = false;
     
     // Take the I2C mutex with timeout
     if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
         // Try to communicate with the device
         i2cWire->beginTransmission(PCA9685_ADDR);
         uint8_t error = i2cWire->endTransmission();
         
         deviceFound = (error == 0); // 0 = success
         
         // Release the mutex
         xSemaphoreGive(i2cMutex);
     }
     
     if (!deviceFound) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "PCA9685 not found at address 0x%02X", PCA9685_ADDR);
     } else {
         //DEBUG_PRINT(DEBUG_LEVEL_INFO, "PCA9685 found at address 0x%02X", PCA9685_ADDR);
     }
     
     return deviceFound;
 }
 
 // Reset the PCA9685
 static bool resetPCA9685() {
     bool success = true;
     
     // Put to sleep
     if (!writePCA9685Register(PCA9685_MODE1, PCA9685_SLEEP)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to put PCA9685 to sleep - device may not be connected");
         return false;
     }
     
     // Wait a bit for the oscillator to stabilize
     vTaskDelay(pdMS_TO_TICKS(5));
     
     // Set Mode1 with AI (auto-increment) enabled and sleep bit cleared
     if (!writePCA9685Register(PCA9685_MODE1, PCA9685_AI)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set PCA9685 MODE1 register");
         return false;
     }
     
     // Set Mode2 with OUTDRV (totem pole output) enabled
     if (!writePCA9685Register(PCA9685_MODE2, PCA9685_OUTDRV)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set PCA9685 MODE2 register");
         return false;
     }
     
     // Wait for restart
     vTaskDelay(pdMS_TO_TICKS(5));
     
     // Verify that we can read from the device
     uint8_t mode1Value;
     if (!readPCA9685Register(PCA9685_MODE1, &mode1Value)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot read from PCA9685 after reset");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "PCA9685 reset successful, MODE1=%02X", mode1Value);
     return true;
 }
 
 bool initPulseGenerator(TwoWire &wire) {
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing Pulse Generator module");
     
     // Store the I2C reference
     i2cWire = &wire;
     
     // Create a mutex for I2C access (or use an existing one)
     i2cMutex = xSemaphoreCreateMutex();
     if (i2cMutex == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Pulse Generator I2C mutex");
         return false;
     }
     
     // Configure the enable pin
     pinMode(PULSE_ENABLE_PIN, OUTPUT);
     digitalWrite(PULSE_ENABLE_PIN, LOW);  // Start disabled
     
     // Check if the PCA9685 is present on the I2C bus
     if (!isPCA9685Present()) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "PCA9685 not detected on the I2C bus");
         return false;
     }
     
     // Try to reset the PCA9685
     if (!resetPCA9685()) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to reset PCA9685, check connections");
         return false;
     }
     
     // Mark as initialized BEFORE setting frequency and duty cycle
     pca9685Initialized = true;
     
     // Set the default frequency
     if (!setPulseFrequency(PULSE_DEFAULT_FREQ)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set default frequency");
         pca9685Initialized = false;  // Revert initialization state
         return false;
     }
     
     // Set 50% duty cycle on both channels but keep disabled
     if (!set50PercentDutyCycle(PULSE_CHANNEL_1) || !set50PercentDutyCycle(PULSE_CHANNEL_2)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set duty cycle");
         pca9685Initialized = false;  // Revert initialization state
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Generator initialized successfully");
     return true;
 }
 
 bool setPulseFrequency(uint16_t freq) {
     // Check if I2C wire is available (even if not fully initialized)
     if (i2cWire == NULL) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "I2C interface not available");
         return false;
     }
     
     // Only check initialization flag in normal operation, not during init
     if (!pca9685Initialized) {
         //DEBUG_PRINT(DEBUG_LEVEL_WARN, "Setting frequency before full initialization");
         // We'll continue anyway, as this might be called from initPulseGenerator
     }
     
     // Constrain frequency to valid range
     if (freq < PULSE_MIN_FREQ) freq = PULSE_MIN_FREQ;
     if (freq > PULSE_MAX_FREQ) freq = PULSE_MAX_FREQ;
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Setting pulse frequency to %u Hz", freq);
     
     // Calculate the prescale value
     uint8_t prescale = calculatePrescale(freq);
     
     // Read current mode
     uint8_t oldmode;
     if (!readPCA9685Register(PCA9685_MODE1, &oldmode)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to read MODE1 register");
         return false;
     }
     
     // To change the frequency, we need to put the device to sleep
     uint8_t newmode = (oldmode & ~PCA9685_RESTART) | PCA9685_SLEEP;
     
     // Go to sleep
     if (!writePCA9685Register(PCA9685_MODE1, newmode)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set MODE1 register (sleep)");
         return false;
     }
     
     // Set the prescaler
     if (!writePCA9685Register(PCA9685_PRESCALE, prescale)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set prescale value");
         return false;
     }
     
     // Restore the original mode value without sleep bit
     if (!writePCA9685Register(PCA9685_MODE1, oldmode)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to restore MODE1 register");
         return false;
     }
     
     // Wait for oscillator
     vTaskDelay(pdMS_TO_TICKS(5));
     
     // Set the RESTART bit to apply changes
     if (!writePCA9685Register(PCA9685_MODE1, oldmode | PCA9685_RESTART)) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to set RESTART bit");
         return false;
     }
     
     // Store the current frequency
     currentFrequency = freq;
     
     // Reapply 50% duty cycle as frequency change affects the timing
     bool success = set50PercentDutyCycle(PULSE_CHANNEL_1) && set50PercentDutyCycle(PULSE_CHANNEL_2);
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse frequency set to %u Hz with prescale %u", freq, prescale);
     return success;
 }
 
 bool enablePulseGenerator(bool enable) {
     // Check if initialized
     if (!pca9685Initialized) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Pulse Generator not initialized");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "%s pulse generator", enable ? "Enabling" : "Disabling");
     
     // Set the enable pin
     digitalWrite(PULSE_ENABLE_PIN, enable ? HIGH : LOW);
     
     // Update state tracking
     currentlyEnabled = enable;
     
     return true;
 }
 
 bool updatePulseGenerator() {
     bool success = true;
     
     // Check if pulseEn has changed
     if (pulseEn != currentlyEnabled) {
         success &= enablePulseGenerator(pulseEn);
     }
     
     // Check if pFrequency has changed
     if (pFrequency != currentFrequency && pFrequency >= PULSE_MIN_FREQ && pFrequency <= PULSE_MAX_FREQ) {
         success &= setPulseFrequency(pFrequency);
     }
     
     return success;
 }
 
 // Task function to monitor and update pulse generator
 static void pulseGeneratorTask(void *pvParameters) {
    //  DEBUG_START_TASK("Pulse Generator");
     Serial.println("Pulse Generator Task Started");
     
     // Main task loop
     while (1) {
         // Update pulse generator based on global variables
         updatePulseGenerator();
         
         // Sleep for a while before checking again
         vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms
     }
     
     // Should never reach here
    //  DEBUG_END_TASK("Pulse Generator");
     vTaskDelete(NULL);
 }
 
 bool createPulseGeneratorTask() {
     // Check if initialized
     if (!pca9685Initialized) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create Pulse Generator task - module not initialized");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating Pulse Generator task");
     
     // Create the task
     BaseType_t result = xTaskCreate(
         pulseGeneratorTask,
         "Pulse Generator",
         4096,
         NULL,
         2,  // Lower priority
         &pulseGeneratorTaskHandle
     );
     
     if (result != pdPASS) {
         //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Pulse Generator task");
         return false;
     }
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Generator task created successfully");
     return true;
 }