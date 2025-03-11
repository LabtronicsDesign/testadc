/*
 * MAX17048 Fuel Gauge with Task Triggering and Self-Deletion
 * 
 * This example demonstrates:
 * 1. A high-priority battery monitoring task that's triggered by another task
 * 2. The battery monitoring task deletes itself after completion
 * 3. Thread-safe I2C access in a FreeRTOS environment
 */

 #include <Arduino.h>
 #include <Wire.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "MAX17048.h"
 
 // I2C pin definitions
 #define SDA_PIN 21
 #define SCL_PIN 22
 
 // Battery alert threshold (%)
 #define BATT_ALERT_THRESHOLD 15
 
 // Structure for passing battery data between tasks
 typedef struct {
   uint16_t voltage;      // Battery voltage in millivolts
   uint8_t soc;           // State of charge percentage (0-100)
   bool isAlert;          // Alert flag
   bool success;          // Whether reading was successful
 } BatteryStatus_t;
 
 // Global I2C instance shared among devices
 TwoWire& sharedI2C = Wire;
 
 // Create MAX17048 instance using the shared I2C bus
 MAX17048 fuelGauge(sharedI2C);
 
 // Queue to store battery results
 QueueHandle_t batteryQueue = NULL;
 
 // Battery monitoring task - runs once and deletes itself
 void batteryTask(void *pvParameters) {
   Serial.println("Battery Task Started (One-shot)");
   
   // Create a local structure for battery status
   BatteryStatus_t battStatus;
   battStatus.success = false;
   
   // Perform battery readings
   Serial.println("Reading battery status...");
   
   // Read all battery parameters in one go (minimizes I2C transactions)
   battStatus.success = fuelGauge.readBatteryStatus(
     &battStatus.voltage, 
     &battStatus.soc, 
     &battStatus.isAlert
   );
   
   // Check if reading was successful
   if (battStatus.success) {
     Serial.println("Battery status read successful!");
     
     // Send results to the queue
     if (xQueueSend(batteryQueue, &battStatus, pdMS_TO_TICKS(100)) != pdPASS) {
       Serial.println("Failed to send battery status to queue!");
     }
   } else {
     Serial.println("Error reading battery status!");
   }
   
   Serial.println("Battery Task completed, deleting itself");
   
   // Delete ourselves - this task is complete
   vTaskDelete(NULL);
 }
 
 // Control task that triggers battery monitoring when needed
 void controlTask(void *pvParameters) {
   Serial.println("Control Task Started");
   TaskHandle_t batteryTaskHandle = NULL;
   
   while (1) {
     // Perform some control operation
     Serial.println("\n--- Control task running ---");
     
     // Trigger battery monitoring
     Serial.println("Creating Battery task...");
     
     // Create the battery task with high priority
     BaseType_t result = xTaskCreate(
       batteryTask,               // Task function
       "Battery Task",            // Name
       4096,                      // Stack size (bytes)
       NULL,                      // Parameters
       configMAX_PRIORITIES-1,    // Highest priority
       &batteryTaskHandle         // Task handle
     );
     
     if (result != pdPASS) {
       Serial.println("Failed to create Battery task!");
     }
     
     // Wait for battery results to arrive in queue
     BatteryStatus_t battStatus;
     if (xQueueReceive(batteryQueue, &battStatus, pdMS_TO_TICKS(5000)) == pdPASS) {
       if (battStatus.success) {
         // Print the battery status
         Serial.println("Battery status from completed task:");
         Serial.print("Voltage: ");
         Serial.print(battStatus.voltage);
         Serial.println(" mV");
         
         Serial.print("State of Charge: ");
         Serial.print(battStatus.soc);
         Serial.println("%");
         
         Serial.print("Alert Status: ");
         Serial.println(battStatus.isAlert ? "ALERT!" : "Normal");
         
         // Take action if battery is low
         if (battStatus.soc <= BATT_ALERT_THRESHOLD) {
           Serial.println("WARNING: Battery charge is low!");
           // Additional low-battery handling could go here
         }
       } else {
         Serial.println("Battery task reported failure");
       }
     } else {
       Serial.println("Timeout waiting for battery results!");
     }
     
     // Wait a bit before next check
     vTaskDelay(pdMS_TO_TICKS(10000));  // Check battery every 10 seconds
   }
 }
 
 void setup() {
   // Initialize serial communication
   Serial.begin(115200);
   
   // Small delay to ensure serial is up
   vTaskDelay(pdMS_TO_TICKS(1000));
   
   Serial.println("\nESP32 MAX17048 Battery Monitor Example");
   
   // Initialize the shared I2C bus
   sharedI2C.begin(SDA_PIN, SCL_PIN);
   
   // Initialize the fuel gauge
   if (!fuelGauge.begin(BATT_ALERT_THRESHOLD)) {
     Serial.println("Failed to initialize fuel gauge! Check connections and restart.");
     while (1) {
       vTaskDelay(pdMS_TO_TICKS(1000));
     }
   }
   
   // Create queue for passing results
   batteryQueue = xQueueCreate(1, sizeof(BatteryStatus_t));
   if (batteryQueue == NULL) {
     Serial.println("Failed to create queue! Halting.");
     while (1) {
       vTaskDelay(pdMS_TO_TICKS(1000));
     }
   }
   
   // Create control task only
   xTaskCreate(
     controlTask,        // Task function
     "Control Task",     // Name
     4096,               // Stack size (bytes)
     NULL,               // Parameters
     3,                  // Medium priority
     NULL                // Task handle not needed
   );
   
   Serial.println("Setup complete");
 }
 
 void loop() {
   // Empty loop - all work is done in tasks
   vTaskDelay(pdMS_TO_TICKS(1000));
 }