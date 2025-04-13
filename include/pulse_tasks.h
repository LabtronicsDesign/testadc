/*
 * Pulse Burst Monitoring Tasks Module Header
 * Provides functions for monitoring digital pulse bursts on a GPIO pin
 */

 #ifndef PULSE_TASKS_H
 #define PULSE_TASKS_H
 
 #include <Arduino.h>
 #include "freertos/FreeRTOS.h"
 
 // Configuration
 #define PULSE_MONITOR_PIN 6  // GPIO pin to monitor
 #define PULSE_BURST_TIMEOUT_US 2000  // Time in microseconds to consider a burst ended (2ms)
 #define PULSE_REPORT_INTERVAL_MS 1000  // Report rolling average every 1 second
 
 // Data structure for pulse burst results
 typedef struct {
   uint32_t burstDurationUs;    // Total duration of the pulse burst in microseconds
   uint32_t offPeriodUs;        // Duration of the off period after the burst in microseconds
   uint16_t pulseCount;         // Number of pulses in the burst
   float frequencyKHz;          // Frequency in kHz within the burst
   uint32_t firstPulsePeriodUs; // Period of the first pulse in microseconds  
   uint32_t timestamp;          // Timestamp when the burst was detected
   bool burstActive;            // Whether a burst is currently active
   bool success;                // Whether reading was successful
 } PulseBurstResult_t;
 
 /**
  * Initialize the pulse burst monitoring module
  * @param monitorPin GPIO pin to monitor (default is 6)
  * @return true if initialization was successful
  */
 bool initPulseBurstModule(uint8_t monitorPin = PULSE_MONITOR_PIN);
 
 /**
  * Create a pulse burst monitoring task
  * The task will continue running in the background, updating results
  * @return true if task creation was successful
  */
 bool createPulseBurstTask();
 
 /**
  * Receive the latest pulse burst results
  * @param result Pointer to store the pulse burst results
  * @param timeout Maximum time to wait for results
  * @return true if results were received
  */
 bool receivePulseBurstResults(PulseBurstResult_t *result, TickType_t timeout);
 
 /**
  * Stop the pulse burst monitoring task
  * @return true if task was successfully stopped
  */
 bool stopPulseBurstTask();
 
 #endif // PULSE_TASKS_H