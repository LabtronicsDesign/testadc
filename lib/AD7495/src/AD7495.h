#ifndef AD7495_H
#define AD7495_H

#include <Arduino.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class AD7495 {
  public:
    /**
     * Constructor with external SPI instance
     * @param spi Reference to an existing SPI instance
     * @param csPin Chip Select pin number
     */
    AD7495(SPIClass &spi, uint8_t csPin);
    
    /**
     * Initialize the ADC (does not initialize SPI)
     * @param spiFreq SPI frequency in Hz (default: 1MHz)
     */
    bool begin(uint32_t spiFreq = 1000000);
    
    /**
     * Read a single sample from the ADC
     * @return 12-bit ADC value or 0xFFFF if error
     */
    uint16_t readSample();
    
    /**
     * Read multiple samples from the ADC
     * @param buffer Array to store samples
     * @param count Number of samples to read
     * @param delayBetweenSamples Microseconds to delay between samples (default: 100)
     * @return Elapsed time in milliseconds or 0 if error
     */
    unsigned long readSamples(uint16_t* buffer, uint16_t count, uint16_t delayBetweenSamples = 100);

  private:
    SPIClass &_spi;      // Reference to external SPI instance
    uint8_t _csPin;
    bool _initialized;
    SemaphoreHandle_t _spiMutex;  // Mutex for SPI bus access
    uint32_t _spiFreq;   // Store frequency for later use
};

#endif // AD7495_H