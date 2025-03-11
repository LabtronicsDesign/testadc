/*
 * ADC Tasks Module Implementation
 */

 #include "adc_tasks.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "simplified_debug.h"
 
 // Static variables
 static AD7495 *adcInstance = NULL;
 static QueueHandle_t adcResultsQueue = NULL;
 
 // ADC reading task - runs once and deletes itself
 static void adcTask(void *pvParameters) {
   DEBUG_START_TASK("ADC");
   Serial.println("ADC Task Started (One-shot)");
   
   // Create a local buffer for results
   AdcResult_t result;
   result.success = false;
   
   // Perform ADC capture
   Serial.println("Starting to capture 100 samples...");
   result.captureTimeMs = adcInstance->readSamples(result.samples, SAMPLES_PER_BATCH);
   
   // Check if sampling was successful
   if (result.captureTimeMs > 0) {
     result.success = true;
     
     // Send results to the queue
     if (xQueueSend(adcResultsQueue, &result, pdMS_TO_TICKS(100)) != pdPASS) {
       DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to send ADC results to queue!");
       Serial.println("Failed to send ADC results to queue!");
     }
   } else {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Error reading ADC samples!");
     Serial.println("Error reading ADC samples!");
   }
   
   DEBUG_END_TASK("ADC");
   Serial.println("ADC Task completed, deleting itself");
   vTaskDelete(NULL);
 }
 
 bool initAdcModule(SPIClass &spi, uint8_t csPin) {
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing ADC module on CS pin %d", csPin);
   
   // Create ADC instance
   adcInstance = new AD7495(spi, MISO_PIN, csPin);
   if (adcInstance == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create ADC instance - out of memory");
     return false;
   }
   
   // Initialize the ADC
   adcInstance->begin(1000000);
   
   // Create queue for passing results
   adcResultsQueue = xQueueCreate(1, sizeof(AdcResult_t));
   if (adcResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create ADC results queue - out of memory");
     delete adcInstance;
     adcInstance = NULL;
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC module initialized successfully");
   return true;
 }
 
 bool createAdcTask() {
   if (adcInstance == NULL || adcResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create ADC task - module not initialized");
     return false;
   }
   
   // Create the ADC task with high priority
   TaskHandle_t adcTaskHandle = NULL;
   BaseType_t result = xTaskCreate(
     adcTask,
     "ADC Task",
     4096,
     NULL,
     configMAX_PRIORITIES-1,  // Highest priority
     &adcTaskHandle
   );
   
   if (result != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create ADC task - error code: %d", result);
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC task created successfully");
   return true;
 }
 
 bool receiveAdcResults(AdcResult_t *result, TickType_t timeout) {
   if (result == NULL || adcResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Invalid ADC results receive request");
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Waiting for ADC results (timeout: %u ms)", timeout);
   
   // Receive results from the queue
   BaseType_t received = xQueueReceive(adcResultsQueue, result, timeout);
   
   if (received != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for ADC results");
   } else {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC results received successfully");
   }
   
   return (received == pdPASS);
 }