/*
 * MCP4151 Tasks Module Implementation
 */

 #include "mcp4151_tasks.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "simplified_debug.h"
 
 // Static variables
 static MCP4151 *digipotInstance = NULL;
 static QueueHandle_t digipotResultsQueue = NULL;
 static uint8_t lastPosition = 0;
 
 // Task parameters structure
 typedef struct {
   DigipotOp_t operation;
   uint8_t position;
 } DigipotTaskParams_t;
 
 // Digipot control task - runs once and deletes itself
 static void digipotTask(void *pvParameters) {
  //  DEBUG_START_TASK("Digipot");
   //DEBUG_PRINT(DEBUG_LEVEL_INFO,"Digipot Task Started (One-shot)");
   
   // Get parameters
   DigipotTaskParams_t *params = (DigipotTaskParams_t*)pvParameters;
   
   // Create a local structure for results
   DigipotResult_t result;
   result.success = false;
   
   // Perform the requested operation
   switch (params->operation) {
     case DIGIPOT_OP_SET:
       //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Setting digipot wiper to position %d...\n", params->position);
       result.success = digipotInstance->setWiper(params->position);
       break;
       
     case DIGIPOT_OP_INCREMENT:
       //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Incrementing digipot wiper...");
       result.position = digipotInstance->incrementWiper();
       result.success = (result.position != 255);  // Check for error code
       break;
       
     case DIGIPOT_OP_DECREMENT:
       //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Decrementing digipot wiper...");
       result.position = digipotInstance->decrementWiper();
       result.success = (result.position != 255);  // Check for error code
       break;
       
     case DIGIPOT_OP_READ:
       //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Reading digipot wiper position...");
       result.position = digipotInstance->getWiper();
       result.success = (result.position != 255);  // Check for error code
       break;
       
     default:
       //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Unknown digipot operation: %d", params->operation);
       break;
   }
   
   // If we performed a set operation, read back to verify
   if (params->operation == DIGIPOT_OP_SET && result.success) {
     result.position = digipotInstance->getWiper();
     result.success = (result.position != 255);  // Check for error code
   }
   
   // Check if operation was successful
   if (result.success) {
     lastPosition = result.position;
     
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digipot wiper position: %d", result.position);
     
     // Send results to the queue
     if (xQueueSend(digipotResultsQueue, &result, pdMS_TO_TICKS(100)) != pdPASS) {
       //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to send digipot results to queue!");
     }
   } else {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Error with digipot operation!");
   }
   
   // Free the parameters
   delete params;
   
  //  DEBUG_END_TASK("Digipot");
   vTaskDelete(NULL);
 }
 
 bool initDigipotModule(SPIClass &spi, uint8_t csPin) {
   //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing Digipot module on CS pin %d", csPin);
   
   // Create digipot instance
   digipotInstance = new MCP4151(spi, csPin);
   if (digipotInstance == NULL) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create digipot instance - out of memory");
     return false;
   }
   
   // Initialize the digipot
   if (!digipotInstance->begin(1000000)) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to initialize digipot");
     delete digipotInstance;
     digipotInstance = NULL;
     return false;
   }
   
   // Read initial position
   lastPosition = digipotInstance->getWiper();
   
   // Create queue for passing results
   digipotResultsQueue = xQueueCreate(1, sizeof(DigipotResult_t));
   if (digipotResultsQueue == NULL) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create digipot results queue - out of memory");
     delete digipotInstance;
     digipotInstance = NULL;
     return false;
   }
   
   //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digipot module initialized successfully (initial position: %d)", lastPosition);
   return true;
 }
 
 bool createDigipotTask(DigipotOp_t operation, uint8_t position) {
   if (digipotInstance == NULL || digipotResultsQueue == NULL) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create Digipot task - module not initialized");
     return false;
   }
   
   // Create parameters for the task
   DigipotTaskParams_t *params = new DigipotTaskParams_t;
   if (params == NULL) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to allocate memory for task parameters");
     return false;
   }
   
   params->operation = operation;
   params->position = position;
   
   // Create the digipot task with high priority
   TaskHandle_t digipotTaskHandle = NULL;
   BaseType_t result = xTaskCreate(
     digipotTask,
     "Digipot Task",
     4096,
     params,  // Pass the parameters structure
     configMAX_PRIORITIES-2,  // High priority but lower than ADC and battery
     &digipotTaskHandle
   );
   
   if (result != pdPASS) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Digipot task - error code: %d", result);
     delete params;  // Clean up on failure
     return false;
   }
   
   //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digipot task created successfully for operation %d", operation);
   return true;
 }
 
 bool receiveDigipotResults(DigipotResult_t *result, TickType_t timeout) {
   if (result == NULL || digipotResultsQueue == NULL) {
     //DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Invalid Digipot results receive request");
     return false;
   }
   
   //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Waiting for Digipot results (timeout: %u ms)", timeout);
   
   // Receive results from the queue
   BaseType_t received = xQueueReceive(digipotResultsQueue, result, timeout);
   
   if (received != pdPASS) {
     //DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for Digipot results");
   } else {
     //DEBUG_PRINT(DEBUG_LEVEL_INFO, "Digipot results received successfully");
     lastPosition = result->position;
   }
   
   return (received == pdPASS);
 }
 
 uint8_t getLastWiperPosition() {
   return lastPosition;
 }