/*
 * ADC Tasks Module Header
 * Provides functions for ADC sampling in a separate high-priority task
 */

 #ifndef ADC_TASKS_H
 #define ADC_TASKS_H
 
 #include <Arduino.h>
 #include <SPI.h>
 #include "freertos/FreeRTOS.h"
 #include "AD7495.h"
 
 // Configuration
 #define SAMPLES_PER_BATCH 100
 
 // MISO pin may be needed for constructor
 #define MISO_PIN MISO
 
 // Data structure for ADC results
 typedef struct {
   uint16_t samples[SAMPLES_PER_BATCH];
   unsigned long captureTimeMs;
   bool success;
 } AdcResult_t;
 
 /**
  * Initialize the ADC module
  * @param spi Reference to SPI instance to use with the AD7495
  * @param csPin Chip select pin for the ADC
  * @return true if initialization was successful
  */
 bool initAdcModule(SPIClass &spi, uint8_t csPin);
 
 /**
  * Create a high-priority ADC sampling task
  * The task will self-delete after completion
  * @return true if task creation was successful
  */
 bool createAdcTask();
 
 /**
  * Receive ADC results from the task
  * @param result Pointer to store the ADC results
  * @param timeout Maximum time to wait for results
  * @return true if results were received
  */
 bool receiveAdcResults(AdcResult_t *result, TickType_t timeout);
 
 #endif // ADC_TASKS_H