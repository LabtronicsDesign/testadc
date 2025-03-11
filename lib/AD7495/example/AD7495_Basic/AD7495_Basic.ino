/*
 * AD7495 ADC Example
 * 
 * This example demonstrates how to read 100 samples every 2 seconds
 * from an AD7495 ADC using the AD7495 library.
 * 
 * Connections:
 * - MISO: GPIO 34
 * - SCLK: GPIO 35 (Note: This is an input-only pin on ESP32, consider using GPIO 18 instead)
 * - CS: GPIO 5
 */

#include <AD7495.h>

// Pin definitions
#define MISO_PIN 34
#define SCLK_PIN 35  // Note: This is input-only on ESP32, consider using GPIO 18 instead
#define CS_PIN 5

// Sample rate configuration
#define SAMPLES_PER_BATCH 100
#define BATCH_INTERVAL_MS 2000  // 2 seconds

// Create AD7495 instance
AD7495 adc(MISO_PIN, SCLK_PIN, CS_PIN);

// Buffer for storing samples
uint16_t samples[SAMPLES_PER_BATCH];

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while(!Serial) {
    ; // Wait for serial port to connect
  }
  
  Serial.println("ESP32 AD7495 Library Example");
  
  // Initialize the ADC
  adc.begin();
}

void loop() {
  Serial.println("Starting to capture 100 samples...");
  
  // Read samples and get elapsed time
  unsigned long elapsedTime = adc.readSamples(samples, SAMPLES_PER_BATCH);
  
  // Print the samples
  Serial.println("Samples:");
  for (int i = 0; i < SAMPLES_PER_BATCH; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(samples[i]);
  }
  
  // Print timing information
  Serial.print("Batch captured in ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
  
  // Wait for the remainder of the 2-second interval
  if (elapsedTime < BATCH_INTERVAL_MS) {
    delay(BATCH_INTERVAL_MS - elapsedTime);
  }
}