/*
 * Beeper Module Implementation
 */

 #include "beeper.h"
 #include "simplified_debug.h"
 
 // Use a mutex to protect concurrent access to the beeper
 static SemaphoreHandle_t beeperMutex = NULL;
 
 // Beeper task handle
 static TaskHandle_t beeperTaskHandle = NULL;
 
 // Structure to pass beep parameters to the task
 typedef struct {
     uint16_t frequency;
     uint16_t duration;
 } BeepParams_t;
 
 // Beeper task function - generates the beep in its own task
 // This ensures beeping doesn't block other operations
 static void beeperTask(void *pvParameters) {
     BeepParams_t *params = (BeepParams_t *)pvParameters;
     
     // Calculate delay for the frequency (half period)
     uint32_t delayPeriod = 500000 / params->frequency; // in microseconds
     
     // Calculate number of cycles based on duration
     uint32_t cycles = (params->frequency * params->duration) / 1000;
     
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Beep: %u Hz for %u ms (%u cycles)", 
                 params->frequency, params->duration, cycles);
     
     // Generate the square wave
     for (uint32_t i = 0; i < cycles; i++) {
         digitalWrite(BEEPER_PIN, HIGH);
         ets_delay_us(delayPeriod);
         digitalWrite(BEEPER_PIN, LOW);
         ets_delay_us(delayPeriod);
     }
     
     // Ensure the pin is LOW when done
     digitalWrite(BEEPER_PIN, LOW);
     
     // Free the parameters structure
     free(params);
     
     // Delete ourselves
     vTaskDelete(NULL);
 }
 
 void initBeeper() {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing beeper on GPIO %d", BEEPER_PIN);
     
     // Initialize the beeper pin
     pinMode(BEEPER_PIN, OUTPUT);
     digitalWrite(BEEPER_PIN, LOW);
     
     // Create the mutex for beeper access
     beeperMutex = xSemaphoreCreateMutex();
     
     if (beeperMutex == NULL) {
         DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create beeper mutex");
     } else {
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "Beeper initialized successfully");
     }
 }
 
 void beep(uint16_t frequency, uint16_t duration) {
     // Check if initialization has been done
     if (beeperMutex == NULL) {
         return;
     }
     
     // Try to take the mutex with timeout
     if (xSemaphoreTake(beeperMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
         // Allocate parameters structure (freed by the task)
         BeepParams_t *params = (BeepParams_t *)malloc(sizeof(BeepParams_t));
         
         if (params != NULL) {
             params->frequency = frequency;
             params->duration = duration;
             
             // Create a task to generate the beep
             BaseType_t result = xTaskCreate(
                 beeperTask,
                 "Beeper",
                 2048,
                 params,
                 2,  // Lower priority
                 &beeperTaskHandle
             );
             
             if (result != pdPASS) {
                 DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create beeper task");
                 free(params);
             }
         }
         
         // Release the mutex
         xSemaphoreGive(beeperMutex);
     }
 }
 
 void shortBeep() {
     beep(BEEP_FREQUENCY_HZ, BEEP_DURATION_MS);
 }
 
 void buttonBeep() {
     beep(BEEP_FREQUENCY_HZ, BEEP_BUTTON_PRESS_MS);
 }