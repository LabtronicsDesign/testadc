#include "AD7495.h"

AD7495::AD7495(SPIClass &spi, uint8_t csPin)
  : _spi(spi), _csPin(csPin), _initialized(false), _spiFreq(1000000) {
  // Create mutex for SPI access
  _spiMutex = xSemaphoreCreateMutex();
}

bool AD7495::begin(uint32_t spiFreq) {
  // Check if mutex was created successfully
  if (_spiMutex == NULL) {
    return false;
  }
  
  // Configure CS pin
  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);  // Deselect the chip initially
  
  // Store SPI frequency for later use
  _spiFreq = spiFreq;
  
  _initialized = true;
  return true;
}

uint16_t AD7495::readSample() {
  if (!_initialized) {
    return 0xFFFF;  // Error code
  }
  
  uint16_t value = 0xFFFF;  // Default to error value
  
  // Take the SPI mutex with timeout
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Select the chip
    digitalWrite(_csPin, LOW);
    
    // Small delay for stability - using FreeRTOS compatible approach
    ets_delay_us(1);
    
    // Read 16 bits (the AD7495 sends 16 bits per conversion)
    // Note: We configure SPI settings just for this transaction
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    value = _spi.transfer16(0);
    _spi.endTransaction();
    
    // Deselect the chip
    digitalWrite(_csPin, HIGH);
    
    // Release the mutex
    xSemaphoreGive(_spiMutex);
    
    // The AD7495 data is 12-bit, right-aligned in a 16-bit word
    // Mask the lower 12 bits
    return value & 0x0FFF;
  }
  
  // If we couldn't get the mutex, return error
  return 0xFFFF;
}

unsigned long AD7495::readSamples(uint16_t* buffer, uint16_t count, uint16_t delayBetweenSamples) {
  if (!_initialized || buffer == NULL) {
    return 0;
  }
  
  unsigned long startTime = millis();
  
  for (uint16_t i = 0; i < count; i++) {
    uint16_t sample = readSample();
    
    // Check for error
    if (sample == 0xFFFF) {
      return 0;  // Error occurred
    }
    
    buffer[i] = sample;
    
    // Delay between samples if specified - using FreeRTOS compatible approach
    if (delayBetweenSamples > 0) {
      if (delayBetweenSamples < 1000) {
        // For short delays, use ets_delay_us
        ets_delay_us(delayBetweenSamples);
      } else {
        // For longer delays, use vTaskDelay
        vTaskDelay(pdMS_TO_TICKS(delayBetweenSamples / 1000));
      }
    }
  }
  
  return millis() - startTime;
}