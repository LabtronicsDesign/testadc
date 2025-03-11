/*
 * Combined Example with AD7495 ADC and MAX17048 Fuel Gauge
 * Main application file with ESP32-S3 support and simplified debug utilities
 */

 #include <Arduino.h>
 #include <SPI.h>
 #include <Wire.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "adc_tasks.h"
 #include "battery_tasks.h"
 #include "simplified_debug.h"
 
 // Pin definitions
 // SPI pins
 #define MISO_PIN MISO
 #define MOSI_PIN MOSI  // Not used for ADC
 #define SCLK_PIN SCK
 #define CS_PIN_ADC 5
 
 // I2C pins
 #define SDA_PIN SDA
 #define SCL_PIN SCL
 
 // Global peripheral instances - use HSPI for ESP32-S3
 SPIClass sharedSPI = SPIClass(HSPI);
 TwoWire& sharedI2C = Wire;
 
 // Flags for battery status - volatile since accessed from multiple tasks
 volatile bool lowBatteryFlag = false;     // Can be read by other tasks
 volatile bool isChargingFlag = false;     // True when battery is charging
 volatile bool chargeCompleteFlag = false; // True when battery is fully charged
 volatile bool batteryConnectedFlag = false; // True when slide switch connects battery to output
 
 // Task handle for control task
 TaskHandle_t controlTaskHandle = NULL;
 
 // Debug monitor task - periodically outputs system status
 void debugMonitorTask(void *pvParameters) {
   const TickType_t monitorDelay = pdMS_TO_TICKS(30000); // Every 30 seconds
   
   while(1) {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "--- PERIODIC SYSTEM STATS ---");
     
     // Print system-wide memory stats
     DEBUG_HEAP_INFO();
     
     // Print stack info for monitor task
     DEBUG_STACK_INFO("Debug Monitor");
     
     vTaskDelay(monitorDelay);
   }
 }
 
 // Battery switch change event handler - called from main task context
 void handleSwitchChange() {
     static bool lastSwitchState = false;
     
     // Only process if state has changed since last check
     if (batteryConnectedFlag != lastSwitchState) {
         lastSwitchState = batteryConnectedFlag;
         
         // Log the change
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "Switch state changed to: %s", 
                     batteryConnectedFlag ? "CONNECTED" : "DISCONNECTED");
         
         Serial.print("\n*** ON/OFF Switch changed: Battery ");
         Serial.print(batteryConnectedFlag ? "CONNECTED" : "DISCONNECTED");
         Serial.println(" ***");
         
         // Add any immediate actions needed when switch changes
         // For example, you might want to put the device into low power mode when disconnected
         if (!batteryConnectedFlag) {
             DEBUG_PRINT(DEBUG_LEVEL_INFO, "Taking actions for disconnected state...");
             Serial.println("Taking actions for disconnected state...");
             // e.g., disable high-power peripherals
         } else {
             DEBUG_PRINT(DEBUG_LEVEL_INFO, "Resuming normal operation...");
             Serial.println("Resuming normal operation...");
             // e.g., resume normal functionality
         }
     }
 }
 
 // Main control task that triggers other tasks
 void controlTask(void *pvParameters) {
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Control Task Started");
   Serial.println("Control Task Started");
   
   while (1) {
     // Check for switch changes (battery connection state)
     handleSwitchChange();
     
     // Log memory stats at the start of each cycle
     DEBUG_STACK_INFO("Control Task");
     
     Serial.println("\n--- Control task cycle ---");
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "--- Starting control task cycle ---");
     
     // ----- Trigger ADC sampling -----
     Serial.println("Creating ADC task...");
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating ADC task");
     
     if (!createAdcTask()) {
       Serial.println("Failed to create ADC task!");
       DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create ADC task");
     }
     
     // ----- Trigger battery monitoring -----
     Serial.println("Creating Battery task...");
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating Battery task");
     
     if (!createBatteryTask()) {
       Serial.println("Failed to create Battery task!");
       DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Battery task");
     }
     
     // ----- Wait for ADC results -----
     AdcResult_t adcResult;
     if (receiveAdcResults(&adcResult, pdMS_TO_TICKS(5000))) {
       if (adcResult.success) {
         // Print the first 5 samples
         Serial.println("First 5 ADC samples:");
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC samples received successfully");
         
         for (int i = 0; i < 5; i++) {
           Serial.print(i);
           Serial.print(": ");
           Serial.println(adcResult.samples[i]);
         }
         
         Serial.print("Batch captured in ");
         Serial.print(adcResult.captureTimeMs);
         Serial.println(" ms");
         
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC batch captured in %lu ms", adcResult.captureTimeMs);
       } else {
         Serial.println("ADC task reported failure");
         DEBUG_PRINT(DEBUG_LEVEL_ERROR, "ADC task reported failure");
       }
     } else {
       Serial.println("Timeout waiting for ADC results!");
       DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for ADC results");
     }
     
     // ----- Wait for battery results -----
     BatteryStatus_t battStatus;
     if (receiveBatteryResults(&battStatus, pdMS_TO_TICKS(5000))) {
       if (battStatus.success) {
         // Print the battery status
         Serial.println("Battery status:");
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery status received successfully");
         
         Serial.print("Voltage: ");
         Serial.print(battStatus.voltage);
         Serial.println(" mV");
         
         Serial.print("State of Charge: ");
         Serial.print(battStatus.soc);
         Serial.println("%");
         
         // Print TP4056 charging status
         Serial.print("Charging Status: ");
         Serial.println(getChargingStatusString(battStatus.chrgStatus));
         
         // Print slide switch status
         Serial.print("Battery Output: ");
         Serial.println(battStatus.switchState ? "Connected" : "Disconnected");
         
         // Update charging flags
         isChargingFlag = (battStatus.chrgStatus == CHARGING);
         chargeCompleteFlag = (battStatus.chrgStatus == CHARGE_COMPLETE);
         
         // Take action based on battery capacity and update flag
         if (battStatus.soc <= BATT_ALERT_THRESHOLD) {
           // Low battery condition
           if (!lowBatteryFlag) {
             // First time detecting low battery
             Serial.println("WARNING: BATTERY LOW - CRITICAL LEVEL!");
             DEBUG_PRINT(DEBUG_LEVEL_WARN, "BATTERY LOW - CRITICAL LEVEL! SOC: %u%%", battStatus.soc);
             lowBatteryFlag = true;
           }
         } else if (battStatus.soc >= BATT_ALERT_THRESHOLD + 5) {
           // Battery recovered (with 5% hysteresis)
           if (lowBatteryFlag) {
             Serial.println("Battery level recovered above threshold");
             DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery level recovered to %u%%", battStatus.soc);
             lowBatteryFlag = false;
           }
         }
         
         // Print the alert status separately
         Serial.print("Alert Status: ");
         if (battStatus.isAlert) {
           Serial.println("ALERT - Low Battery");
           DEBUG_PRINT(DEBUG_LEVEL_WARN, "Battery ALERT active");
         } else {
           Serial.println("Normal");
         }
         
         // Print a summary of the global flags that other tasks can access
         Serial.println("\nGlobal Battery Flags Status:");
         Serial.print("- Low Battery: ");
         Serial.println(lowBatteryFlag ? "YES" : "NO");
         Serial.print("- Charging: ");
         Serial.println(isChargingFlag ? "YES" : "NO");
         Serial.print("- Charge Complete: ");
         Serial.println(chargeCompleteFlag ? "YES" : "NO");
         Serial.print("- Battery Connected: ");
         Serial.println(batteryConnectedFlag ? "YES" : "NO");
         
         DEBUG_PRINT(DEBUG_LEVEL_INFO, 
                    "Battery Flags - Low: %s, Charging: %s, Complete: %s, Connected: %s",
                    lowBatteryFlag ? "YES" : "NO",
                    isChargingFlag ? "YES" : "NO",
                    chargeCompleteFlag ? "YES" : "NO",
                    batteryConnectedFlag ? "YES" : "NO");
       } else {
         Serial.println("Battery task reported failure");
         DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Battery task reported failure");
       }
     } else {
       Serial.println("Timeout waiting for battery results!");
       DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for battery results");
     }
     
     // Wait before next cycle - using a shorter delay to be more responsive to switch changes
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Control task cycle completed, waiting for next cycle");
     vTaskDelay(pdMS_TO_TICKS(1000));  // Run every second
   }
 }
 
 void setup() {
   // Initialize serial communication
   Serial.begin(115200);
   vTaskDelay(pdMS_TO_TICKS(1000));
   
   Serial.println("\nESP32-S3 Combined ADC and Battery Monitor Example");
   
   // Initialize debug utilities
   DEBUG_INIT();
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "ESP32-S3 Combined ADC and Battery Monitor Example");
   DEBUG_HEAP_INFO();
   
   // Initialize the shared SPI bus
   sharedSPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN);
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "SPI initialized (HSPI) - SCLK: %d, MISO: %d, MOSI: %d", 
               SCLK_PIN, MISO_PIN, MOSI_PIN);
   
   // Initialize the shared I2C bus
   sharedI2C.begin(SDA_PIN, SCL_PIN);
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "I2C initialized - SDA: %d, SCL: %d", SDA_PIN, SCL_PIN);
   
   // Initialize modules
   Serial.println("Initializing ADC module...");
   if (!initAdcModule(sharedSPI, CS_PIN_ADC)) {
     Serial.println("Failed to initialize ADC module! Halting.");
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "ADC module initialization failed!");
     while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
   }
   
   Serial.println("Initializing Battery module...");
   if (!initBatteryModule(sharedI2C)) {
     Serial.println("Failed to initialize Battery module! Halting.");
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Battery module initialization failed!");
     while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
   }
   
   // Create control task
   Serial.println("Creating control task...");
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating control task");
   
   BaseType_t result = xTaskCreate(
     controlTask,
     "Control Task",
     4096,
     NULL,
     3,  // Medium priority
     &controlTaskHandle
   );
   
   if (result != pdPASS) {
     Serial.println("Failed to create control task! Halting.");
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create control task!");
     while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
   }
   
   #ifdef DEBUG_ENABLED
   // Create debug monitor task if debugging is enabled
   xTaskCreate(
     debugMonitorTask,
     "Debug Monitor",
     4096,
     NULL,
     1,  // Low priority
     NULL
   );
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Debug monitor task created");
   #endif
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Setup complete");
   Serial.println("Setup complete");
 }
 
 void loop() {
   // Empty loop - all work is done in tasks
   vTaskDelay(pdMS_TO_TICKS(1000));
 }