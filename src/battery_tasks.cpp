/*
 * Battery Tasks Module Implementation
 */

 #include "battery_tasks.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "simplified_debug.h"
 
 // Static variables
 static MAX17048 *fuelGaugeInstance = NULL;
 static QueueHandle_t batteryQueue = NULL;
 
 // Debug flag - set to true to see detailed alert handling logs
 static const bool DEBUG_ALERTS = true;
 
 // External reference to the global battery connected flag
 extern volatile bool batteryConnectedFlag;
 
 // Switch state change ISR
 void IRAM_ATTR switchChangeISR() {
     // Read and update the switch state immediately (with inverted logic)
     batteryConnectedFlag = !digitalRead(BATT_SWITCH_PIN);
 }
 
 // Helper function to determine charging status from TP4056 pins
 static ChargingStatus_t getChargingStatus() {
     bool chrg = digitalRead(TP4056_CHRG_PIN);  // LOW when charging
     bool stdby = digitalRead(TP4056_STDBY_PIN); // LOW when charge complete
     
     if (!chrg && stdby) {
         return CHARGING;         // Charging in progress
     } else if (chrg && !stdby) {
         return CHARGE_COMPLETE;  // Charge complete / standby
     } else if (chrg && stdby) {
         return NOT_CHARGING;     // Not charging (no power or battery disconnected)
     } else {
         return ERROR_STATUS;     // Both pins LOW - shouldn't happen in normal operation
     }
 }
 
 // Helper function to read slide switch state (inverted logic)
 static bool readSwitchState() {
     // Invert the reading (LOW = connected, HIGH = disconnected)
     return !digitalRead(BATT_SWITCH_PIN);
 }
 
 // Battery monitoring task - runs once and deletes itself
 static void batteryTask(void *pvParameters) {
   DEBUG_START_TASK("Battery");
   //Serial.println("Battery Task Started (One-shot)");
   
   // Create a local structure for battery status
   BatteryStatus_t battStatus;
   battStatus.success = false;
   
   // Perform battery readings
   battStatus.voltage = fuelGaugeInstance->readVoltage();
   battStatus.soc = fuelGaugeInstance->readSOC();
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery readings - Voltage: %u mV, SOC: %u%%", 
               battStatus.voltage, battStatus.soc);
   
   // Read TP4056 charging status
   battStatus.chrgStatus = getChargingStatus();
   
   // Read slide switch state
   battStatus.switchState = readSwitchState();
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery status - Charging: %s, Switch: %s", 
               getChargingStatusString(battStatus.chrgStatus),
               battStatus.switchState ? "Connected" : "Disconnected");
   
   // Check if reading was successful (voltage > 0, SOC != 255)
   if (battStatus.voltage > 0 && battStatus.soc != 255) {
     battStatus.success = true;
     
     // Handle alerts - check first if there's an alert
     battStatus.isAlert = fuelGaugeInstance->isAlertActive();
     
     // If alert is active but SOC is well above threshold, clear it as a false alert
     if (battStatus.isAlert && battStatus.soc > BATT_ALERT_THRESHOLD + 5) {
       DEBUG_PRINT(DEBUG_LEVEL_WARN, "False alert detected (SOC: %u%%). Clearing...", battStatus.soc);
       Serial.println("\nClearing false alert condition");
       
       // Try to clear the alert
       bool clearResult = fuelGaugeInstance->clearAlert();
       
       if (DEBUG_ALERTS) {
         Serial.print("Alert clear result: ");
         Serial.println(clearResult ? "Success" : "Failed");
       }
       
       DEBUG_PRINT(DEBUG_LEVEL_INFO, "Alert clear result: %s", clearResult ? "Success" : "Failed");
       
       // Re-read alert status to check if it was actually cleared
       battStatus.isAlert = fuelGaugeInstance->isAlertActive();
       
       if (DEBUG_ALERTS) {
         Serial.print("Alert status after clearing: ");
         Serial.println(battStatus.isAlert ? "Still Active" : "Cleared");
       }
       
       DEBUG_PRINT(DEBUG_LEVEL_INFO, "Alert status after clearing: %s", 
                   battStatus.isAlert ? "Still Active" : "Cleared");
       
       // If still not cleared, try one more aggressive approach
       if (battStatus.isAlert) {
         DEBUG_PRINT(DEBUG_LEVEL_WARN, "First clear attempt failed, trying again...");
         Serial.println("First clear attempt failed, trying again...");
         // Reinitialize the fuel gauge completely
         fuelGaugeInstance->begin(BATT_ALERT_THRESHOLD);
         // Re-read alert status again
         battStatus.isAlert = fuelGaugeInstance->isAlertActive();
         
         if (DEBUG_ALERTS) {
           Serial.print("Alert status after reinitialization: ");
           Serial.println(battStatus.isAlert ? "Still Active" : "Cleared");
         }
         
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "Alert status after reinitialization: %s", 
                     battStatus.isAlert ? "Still Active" : "Cleared");
       }
     }
     
     // Send results to the queue
     if (xQueueSend(batteryQueue, &battStatus, pdMS_TO_TICKS(100)) != pdPASS) {
       DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to send battery status to queue!");
       Serial.println("Failed to send battery status to queue!");
     }
   } else {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Error reading battery status! Voltage: %u mV, SOC: %u%%", 
                 battStatus.voltage, battStatus.soc);
     Serial.println("Error reading battery status!");
   }
   
   DEBUG_END_TASK("Battery");
   //Serial.println("Battery Task completed, deleting itself");
   vTaskDelete(NULL);
 }
 
 bool initBatteryModule(TwoWire &wire) {
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing Battery module");
   
   // Create fuel gauge instance
   fuelGaugeInstance = new MAX17048(wire);
   if (fuelGaugeInstance == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create fuel gauge instance - out of memory");
     return false;
   }
   
   // Setup TP4056 indicator pins
   pinMode(TP4056_CHRG_PIN, INPUT_PULLUP);  // Pull-up since active LOW
   pinMode(TP4056_STDBY_PIN, INPUT_PULLUP);  // Pull-up since active LOW
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "TP4056 pins configured - CHRG: %d, STDBY: %d", 
               TP4056_CHRG_PIN, TP4056_STDBY_PIN);
   
   // Setup slide switch pin with interrupt
   pinMode(BATT_SWITCH_PIN, INPUT_PULLUP);  // Use pull-up
   
   // Initialize switch state (inverted logic)
   batteryConnectedFlag = readSwitchState();
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery switch pin configured: %d, initial state: %s", 
               BATT_SWITCH_PIN, batteryConnectedFlag ? "Connected" : "Disconnected");
   
   // Attach interrupt for immediate notification of switch changes
   attachInterrupt(digitalPinToInterrupt(BATT_SWITCH_PIN), switchChangeISR, CHANGE);
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery switch interrupt attached");
   
   // Initialize the fuel gauge with specified alert threshold
   fuelGaugeInstance->begin(BATT_ALERT_THRESHOLD);
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Fuel gauge initialized with alert threshold: %d%%", 
               BATT_ALERT_THRESHOLD);
   
   // Wait a moment for the fuel gauge to stabilize
   vTaskDelay(pdMS_TO_TICKS(100));
   
   // Make sure to clear any existing alerts
   fuelGaugeInstance->clearAlert();
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initial alerts cleared");
   
   // Create queue for passing results
   batteryQueue = xQueueCreate(1, sizeof(BatteryStatus_t));
   if (batteryQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create battery queue - out of memory");
     delete fuelGaugeInstance;
     fuelGaugeInstance = NULL;
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery module initialized successfully");
   return true;
 }
 
 bool createBatteryTask() {
   if (fuelGaugeInstance == NULL || batteryQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create Battery task - module not initialized");
     return false;
   }
   
   // Create the battery task with high priority
   TaskHandle_t batteryTaskHandle = NULL;
   BaseType_t result = xTaskCreate(
     batteryTask,
     "Battery Task",
     4096,
     NULL,
     configMAX_PRIORITIES-1,  // Highest priority
     &batteryTaskHandle
   );
   
   if (result != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Battery task - error code: %d", result);
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery task created successfully");
   return true;
 }
 
 bool receiveBatteryResults(BatteryStatus_t *result, TickType_t timeout) {
   if (result == NULL || batteryQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Invalid Battery results receive request");
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Waiting for Battery results (timeout: %u ms)", timeout);
   
   // Receive results from the queue
   BaseType_t received = xQueueReceive(batteryQueue, result, timeout);
   
   if (received != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for Battery results");
   } else {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery results received successfully");
   }
   
   return (received == pdPASS);
 }
 
 const char* getChargingStatusString(ChargingStatus_t status) {
     switch (status) {
         case CHARGING:
             return "Charging";
         case CHARGE_COMPLETE:
             return "Charge Complete";
         case NOT_CHARGING:
             return "Not Charging";
         case ERROR_STATUS:
             return "Error";
         default:
             return "Unknown";
     }
 }