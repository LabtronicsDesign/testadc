#ifndef MCP4151_H
#define MCP4151_H

#include <Arduino.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// MCP4151 command structure
// Command byte format: C1 C0 A1 A0 X X X X
// C1:C0 - Command bits (00=write, 11=read)
// A1:A0 - Register address
#define MCP4151_CMD_WRITE    0x00
#define MCP4151_CMD_READ     0x0C

// Register addresses
#define MCP4151_REG_WIPER    0x00  // Wiper position register
#define MCP4151_REG_TCON     0x04  // Terminal control register (optional)
#define MCP4151_REG_STATUS   0x05  // Status register (optional)

class MCP4151 {
  public:
    /**
     * Constructor with external SPI instance
     * @param spi Reference to an existing SPI instance
     * @param csPin Chip Select pin number
     */
    MCP4151(SPIClass &spi, uint8_t csPin);
    
    /**
     * Initialize the digital potentiometer
     * @param spiFreq SPI frequency in Hz (default: 1MHz)
     * @return true if initialization successful
     */
    bool begin(uint32_t spiFreq = 1000000);
    
    /**
     * Set the wiper position
     * @param position Wiper position (0-255)
     * @return true if successful
     */
    bool setWiper(uint8_t position);
    
    /**
     * Get the current wiper position
     * @return Wiper position (0-255) or 255 if error
     */
    uint8_t getWiper();
    
    /**
     * Increment the wiper position by one step
     * @return New wiper position or 255 if error
     */
    uint8_t incrementWiper();
    
    /**
     * Decrement the wiper position by one step
     * @return New wiper position or 255 if error
     */
    uint8_t decrementWiper();

  private:
    SPIClass &_spi;         // Reference to external SPI instance
    uint8_t _csPin;         // Chip select pin
    bool _initialized;      // Initialization flag
    SemaphoreHandle_t _spiMutex; // Mutex for SPI bus access
    uint32_t _spiFreq;      // SPI frequency
    uint8_t _lastPosition;  // Cache of last known position
    
    /**
     * Write to a register
     * @param reg Register address
     * @param value Value to write
     * @return true if successful
     */
    bool writeRegister(uint8_t reg, uint8_t value);
    
    /**
     * Read from a register
     * @param reg Register address
     * @return Register value or 255 if error
     */
    uint8_t readRegister(uint8_t reg);
};

#endif // MCP4151_H