/*
 * Battery Tasks Module Header
 * Provides functions for battery monitoring in a separate high-priority task
 */

 #ifndef BATTERY_TASKS_H
 #define BATTERY_TASKS_H
 
 #include <Arduino.h>
 #include <Wire.h>
 #include "freertos/FreeRTOS.h"
 #include "MAX17048.h"
 
 // Battery alert threshold (%)
 #define BATT_ALERT_THRESHOLD 10
 
 // TP4056 GPIO pins
 #define TP4056_CHRG_PIN 1  // Charging indicator pin (active LOW)
 #define TP4056_STDBY_PIN 2 // Standby/Charge Complete indicator pin (active LOW)
 
 // Slide switch pin
 #define BATT_SWITCH_PIN 4   // Slide switch pin for battery output connection
 
 // Charging status enum
 typedef enum {
     CHARGING,           // Battery is charging (CHRG=LOW, STDBY=HIGH)
     CHARGE_COMPLETE,    // Battery fully charged (CHRG=HIGH, STDBY=LOW)
     NOT_CHARGING,       // Not charging (CHRG=HIGH, STDBY=HIGH)
     ERROR_STATUS        // Unexpected state (CHRG=LOW, STDBY=LOW) - should not happen
 } ChargingStatus_t;
 
 // Data structure for battery results
 typedef struct {
   uint16_t voltage;              // Battery voltage in millivolts
   uint8_t soc;                   // State of charge percentage (0-100)
   bool isAlert;                  // Alert flag
   ChargingStatus_t chrgStatus;   // Charging status from TP4056
   bool switchState;              // Slide switch state (true = connected)
   bool success;                  // Whether reading was successful
 } BatteryStatus_t;
 
 /**
  * Initialize the battery monitoring module
  * @param wire Reference to I2C instance
  * @return true if initialization was successful
  */
 bool initBatteryModule(TwoWire &wire);
 
 /**
  * Create a high-priority battery monitoring task
  * The task will self-delete after completion
  * @return true if task creation was successful
  */
 bool createBatteryTask();
 
 /**
  * Receive battery status results from the task
  * @param result Pointer to store the battery status
  * @param timeout Maximum time to wait for results
  * @return true if results were received
  */
 bool receiveBatteryResults(BatteryStatus_t *result, TickType_t timeout);
 
 /**
  * Get charging status as a string
  * @param status The charging status enum value
  * @return String representation of the charging status
  */
 const char* getChargingStatusString(ChargingStatus_t status);
 
 #endif // BATTERY_TASKS_H