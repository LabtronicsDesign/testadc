/*
 * Pulse Burst Monitoring Tasks Module Implementation
 */

 #include "pulse_tasks.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "simplified_debug.h"
 
 // Static variables
 static uint8_t pulsePin = PULSE_MONITOR_PIN;
 static QueueHandle_t pulseResultsQueue = NULL;
 static TaskHandle_t pulseTaskHandle = NULL;
 
 // ISR-safe critical section for ESP32
 static portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
 
 // Pulse burst tracking variables
 static volatile uint32_t lastEdgeTimeUs = 0;
 static volatile uint32_t firstPulseTimeUs = 0;
 static volatile uint32_t burstStartTimeUs = 0;
 static volatile uint32_t lastBurstEndTimeUs = 0;
 static volatile uint16_t edgeCount = 0;
 static volatile bool burstActive = false;
 static volatile bool notifyTask = false;
 
 // ISR for handling edge detection - optimized for high-frequency pulse bursts
 static void IRAM_ATTR pulseBurstISR() {
   uint32_t currentTimeUs = micros();
   
   portENTER_CRITICAL_ISR(&pulseMux);
   
   // If this is the first edge after a long gap, it's the start of a new burst
   if (!burstActive && (currentTimeUs - lastEdgeTimeUs > PULSE_BURST_TIMEOUT_US)) {
     burstStartTimeUs = currentTimeUs;
     edgeCount = 1;
     burstActive = true;
     firstPulseTimeUs = 0;  // Will be set on the next edge
     notifyTask = true;     // Notify task to start monitoring
   } 
   // If we're in an active burst
   else if (burstActive) {
     edgeCount++;
     
     // If this is the second edge, measure the first pulse period
     if (edgeCount == 3 && firstPulseTimeUs == 0) {
       firstPulseTimeUs = currentTimeUs - lastEdgeTimeUs;
     }
   }
   
   // Update last edge time regardless
   lastEdgeTimeUs = currentTimeUs;
   
   portEXIT_CRITICAL_ISR(&pulseMux);
 }
 
 // Pulse burst monitoring task - runs continuously
 static void pulseBurstTask(void *pvParameters) {
   DEBUG_START_TASK("Pulse Burst Monitor");
   Serial.println("Pulse Burst Monitoring Task Started");
   
   PulseBurstResult_t result;
   result.burstActive = false;
   result.success = false;
   result.burstDurationUs = 0;
   result.offPeriodUs = 0;
   result.pulseCount = 0;
   result.frequencyKHz = 0;
   result.firstPulsePeriodUs = 0;
   result.timestamp = 0;
   
   // Local variables for tracking burst state
   uint32_t previousBurstEnd = 0;
   uint32_t lastCheckTimeUs = 0;
   bool wasActive = false;
   
   // Variables for rolling average
   const int avgWindow = 10;  // Average over last 10 bursts
   uint16_t burstCounts[avgWindow] = {0};
   float frequencies[avgWindow] = {0};
   uint32_t firstPulsePeriods[avgWindow] = {0};
   uint32_t burstDurations[avgWindow] = {0};
   uint32_t offPeriods[avgWindow] = {0};
   int avgIndex = 0;
   int validBursts = 0;
   
   // Store first valid reading for comparison
   bool haveFirstReading = false;
   uint16_t firstPulseCount = 0;
   float firstFrequency = 0;
   uint32_t firstPulsePeriod = 0;
   uint32_t firstBurstDuration = 0;
   uint32_t firstOffPeriod = 0;
   uint32_t firstReadingTimestamp = 0;  // Timestamp when first reading was stored
   const uint32_t FIRST_READING_MIN_DURATION_MS = 3000;  // Keep first reading for at least 3 seconds
   
   // Maximum pulses threshold
   const uint16_t MAX_PULSE_COUNT = 40;
   
   // Variables for periodic reporting
   TickType_t lastReportTime = xTaskGetTickCount();
   const TickType_t reportInterval = pdMS_TO_TICKS(PULSE_REPORT_INTERVAL_MS);
   
   while (1) {
     // Short delay to check status frequently but avoid hogging CPU
     vTaskDelay(pdMS_TO_TICKS(10));
     
     uint32_t currentTimeUs = micros();
     bool localBurstActive;
     uint32_t localLastEdgeTime;
     uint32_t localBurstStartTime;
     uint16_t localEdgeCount;
     uint32_t localFirstPulseTime;
     bool localNotifyTask;
     
     // Critical section to safely read volatile variables
     portENTER_CRITICAL(&pulseMux);
     localBurstActive = burstActive;
     localLastEdgeTime = lastEdgeTimeUs;
     localBurstStartTime = burstStartTimeUs;
     localEdgeCount = edgeCount;
     localFirstPulseTime = firstPulseTimeUs;
     localNotifyTask = notifyTask;
     
     // Reset notification flag
     if (localNotifyTask) {
       notifyTask = false;
     }
     portEXIT_CRITICAL(&pulseMux);
     
     // Check if burst ended (timeout since last edge)
     if (localBurstActive && (currentTimeUs - localLastEdgeTime > PULSE_BURST_TIMEOUT_US)) {
       // Burst just ended
       portENTER_CRITICAL(&pulseMux);
       burstActive = false;
       lastBurstEndTimeUs = currentTimeUs;
       portEXIT_CRITICAL(&pulseMux);
       
       // Calculate burst duration
       uint32_t burstDuration = 0;
       if (currentTimeUs > localBurstStartTime) {
         burstDuration = currentTimeUs - localBurstStartTime;
       }
       
       // Calculate off period since previous burst
       uint32_t offPeriod = 0;
       if (previousBurstEnd > 0 && localBurstStartTime > previousBurstEnd) {
         offPeriod = localBurstStartTime - previousBurstEnd;
       }
       
       // Calculate frequency in kHz if we have enough edges
       float freqKHz = 0;
       if (localEdgeCount >= 4 && burstDuration > 0) {
         // We divide by 2 because we count both rising and falling edges
         freqKHz = ((float)(localEdgeCount / 2) * 1000.0f) / (burstDuration / 1000.0f);
       }
       
       // Store results
       result.success = true;
       result.burstActive = false;
       result.burstDurationUs = burstDuration;
       result.offPeriodUs = offPeriod;
       result.pulseCount = localEdgeCount / 2;  // Each pulse has 2 edges
       result.frequencyKHz = freqKHz;
       result.firstPulsePeriodUs = localFirstPulseTime;
       result.timestamp = millis();
       
       // Check if pulses per burst is below our max threshold
       uint16_t pulseCount = result.pulseCount;
       
       if (pulseCount > MAX_PULSE_COUNT) {
         // Too many pulses - log but don't include in average
         DEBUG_PRINT(DEBUG_LEVEL_WARN, "Burst with %u pulses exceeds limit, ignoring", pulseCount);
         
         // Clear the rolling average buffer
         memset(burstCounts, 0, sizeof(burstCounts));
         memset(frequencies, 0, sizeof(frequencies));
         memset(firstPulsePeriods, 0, sizeof(firstPulsePeriods));
         memset(burstDurations, 0, sizeof(burstDurations));
         memset(offPeriods, 0, sizeof(offPeriods));
         validBursts = 0;
         avgIndex = 0;
         
         // Only reset first reading if it's been at least 3 seconds since it was stored
         uint32_t currentTime = millis();
         if (haveFirstReading && (currentTime - firstReadingTimestamp > FIRST_READING_MIN_DURATION_MS)) {
           DEBUG_PRINT(DEBUG_LEVEL_INFO, "Resetting first reading after 3+ seconds");
           haveFirstReading = false;
         } else if (haveFirstReading) {
           DEBUG_PRINT(DEBUG_LEVEL_INFO, "Preserving first reading (within 3 sec window)");
         }
       } else {
         // Update rolling average window with valid data
         burstCounts[avgIndex] = result.pulseCount;
         frequencies[avgIndex] = result.frequencyKHz;
         firstPulsePeriods[avgIndex] = result.firstPulsePeriodUs;
         burstDurations[avgIndex] = result.burstDurationUs;
         offPeriods[avgIndex] = result.offPeriodUs;
         
         // Store first valid reading if we don't have one yet
         if (!haveFirstReading) {
           firstPulseCount = result.pulseCount;
           firstFrequency = result.frequencyKHz;
           firstPulsePeriod = result.firstPulsePeriodUs;
           firstBurstDuration = result.burstDurationUs;
           firstOffPeriod = result.offPeriodUs;
           firstReadingTimestamp = millis();  // Store timestamp
           haveFirstReading = true;
           
           DEBUG_PRINT(DEBUG_LEVEL_INFO, "Stored first valid reading: %u pulses at %.2f kHz", 
                      firstPulseCount, firstFrequency);
         }
         
         
         // Increment index and valid burst count
         avgIndex = (avgIndex + 1) % avgWindow;
         if (validBursts < avgWindow) {
           validBursts++;
         }
         
         DEBUG_PRINT(DEBUG_LEVEL_INFO, "Valid burst recorded: %u pulses", result.pulseCount);
       }
       
       // Update for next cycle
       previousBurstEnd = currentTimeUs;
       
       // Send results to the queue
       xQueueOverwrite(pulseResultsQueue, &result);
       
       wasActive = false;
     }
     // Check if a new burst started
     else if (localNotifyTask && !wasActive) {
       // New burst detected - just log it without printing
       DEBUG_PRINT(DEBUG_LEVEL_INFO, "New Pulse Burst started");
       
       // Update result with active status
       result.burstActive = true;
       result.success = true;
       
       // Send updated status to queue
       xQueueOverwrite(pulseResultsQueue, &result);
       
       wasActive = true;
     }
     
     // Check if it's time to report the rolling average
     TickType_t currentTicks = xTaskGetTickCount();
     if ((currentTicks - lastReportTime) >= reportInterval && validBursts > 0) {
       lastReportTime = currentTicks;
       
       // Calculate rolling averages
       float avgPulseCount = 0;
       float avgFrequency = 0;
       float avgFirstPulsePeriod = 0;
       float avgBurstDuration = 0;
       float avgOffPeriod = 0;
       
       for (int i = 0; i < validBursts; i++) {
         avgPulseCount += burstCounts[i];
         avgFrequency += frequencies[i];
         avgFirstPulsePeriod += firstPulsePeriods[i];
         avgBurstDuration += burstDurations[i];
         avgOffPeriod += offPeriods[i];
       }
       
       avgPulseCount /= validBursts;
       avgFrequency /= validBursts;
       avgFirstPulsePeriod /= validBursts;
       avgBurstDuration /= validBursts;
       avgOffPeriod /= validBursts;
       
       // Print rolling average
       Serial.println("\n--- Pulse Burst 1-Second Rolling Average ---");
       Serial.print("Current - Pulses: ");
       Serial.print(avgPulseCount, 1);
       Serial.print(", Freq: ");
       Serial.print(avgFrequency, 2);
       Serial.println(" kHz");
       
       Serial.print("Current - Pulse period: ");
       Serial.print(avgFirstPulsePeriod, 1);
       Serial.print(" us, Burst: ");
       Serial.print(avgBurstDuration, 1);
       Serial.print(" us, Off: ");
       Serial.print(avgOffPeriod / 1000, 2);  // Convert to ms for readability
       Serial.println(" ms");
       
       // Print first reading for comparison if available
       if (haveFirstReading) {
         Serial.print("First   - Pulses: ");
         Serial.print(firstPulseCount);
         Serial.print(", Freq: ");
         Serial.print(firstFrequency, 2);
         Serial.println(" kHz");
         
         Serial.print("First   - Pulse period: ");
         Serial.print(firstPulsePeriod);
         Serial.print(" us, Burst: ");
         Serial.print(firstBurstDuration);
         Serial.print(" us, Off: ");
         Serial.print(firstOffPeriod / 1000, 2);  // Convert to ms for readability
         Serial.println(" ms");
         
         // Print change percentage for key metrics
         float pulseCountChange = ((avgPulseCount - firstPulseCount) / firstPulseCount) * 100.0f;
         float frequencyChange = ((avgFrequency - firstFrequency) / firstFrequency) * 100.0f;
         
         Serial.print("Change  - Pulses: ");
         Serial.print(pulseCountChange, 1);
         Serial.print("%, Freq: ");
         Serial.print(frequencyChange, 1);
         Serial.println("%");
       }
       
       Serial.print("Bursts in average: ");
       Serial.println(validBursts);
       
       DEBUG_PRINT(DEBUG_LEVEL_INFO, 
                  "Pulse Avg: %.1f pulses, %.2f kHz, First: %.1f us, Burst: %.1f us, Off: %.2f ms", 
                  avgPulseCount, avgFrequency, avgFirstPulsePeriod, 
                  avgBurstDuration, avgOffPeriod / 1000);
     }
     
     // Update time for next cycle
     lastCheckTimeUs = currentTimeUs;
   }
   
   // Should never reach here, but just in case
   DEBUG_END_TASK("Pulse Burst Monitor");
   vTaskDelete(NULL);
 }
 
 bool initPulseBurstModule(uint8_t monitorPin) {
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Initializing Pulse Burst module on pin %d", monitorPin);
   
   // Store the pin number
   pulsePin = monitorPin;
   
   // Configure the pin as input
   pinMode(pulsePin, INPUT);
   
   // Create queue for passing results (size 1, we only care about latest result)
   pulseResultsQueue = xQueueCreate(1, sizeof(PulseBurstResult_t));
   if (pulseResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create pulse results queue - out of memory");
     return false;
   }
   
   // Check if pin supports interrupts
   if (digitalPinToInterrupt(pulsePin) < 0) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Pin %d does not support interrupts", pulsePin);
     vQueueDelete(pulseResultsQueue);
     pulseResultsQueue = NULL;
     return false;
   }
   
   // Initialize tracking variables
   lastEdgeTimeUs = 0;
   firstPulseTimeUs = 0;
   burstStartTimeUs = 0;
   lastBurstEndTimeUs = 0;
   edgeCount = 0;
   burstActive = false;
   notifyTask = false;
   
   // Attach interrupt (no return value to check)
   attachInterrupt(digitalPinToInterrupt(pulsePin), pulseBurstISR, CHANGE);
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Burst module initialized successfully");
   return true;
 }
 
 bool createPulseBurstTask() {
   if (pulseResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Cannot create Pulse Burst task - module not initialized");
     return false;
   }
   
   // Don't create if already running
   if (pulseTaskHandle != NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_WARN, "Pulse Burst task already running");
     return true;
   }
   
   // Create the pulse burst monitoring task with medium priority
   BaseType_t result = xTaskCreate(
     pulseBurstTask,
     "Pulse Burst Task",
     4096,
     NULL,
     3,  // Medium priority
     &pulseTaskHandle
   );
   
   if (result != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Pulse Burst task - error code: %d", result);
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Burst task created successfully");
   return true;
 }
 
 bool receivePulseBurstResults(PulseBurstResult_t *result, TickType_t timeout) {
   if (result == NULL || pulseResultsQueue == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Invalid Pulse Burst results receive request");
     return false;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Reading Pulse Burst results");
   
   // Receive results from the queue (peek doesn't remove the item)
   BaseType_t received = xQueuePeek(pulseResultsQueue, result, timeout);
   
   if (received != pdPASS) {
     DEBUG_PRINT(DEBUG_LEVEL_WARN, "No Pulse Burst results available");
   } else {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Burst results received successfully");
   }
   
   return (received == pdPASS);
 }
 
 bool stopPulseBurstTask() {
   if (pulseTaskHandle == NULL) {
     DEBUG_PRINT(DEBUG_LEVEL_WARN, "Pulse Burst task not running");
     return true;
   }
   
   // Detach the interrupt
   detachInterrupt(digitalPinToInterrupt(pulsePin));
   
   // Delete the task
   vTaskDelete(pulseTaskHandle);
   pulseTaskHandle = NULL;
   
   // Clean up queue
   if (pulseResultsQueue != NULL) {
     vQueueDelete(pulseResultsQueue);
     pulseResultsQueue = NULL;
   }
   
   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Burst task stopped");
   return true;
 }