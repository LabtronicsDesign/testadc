#include "MCP4151.h"

MCP4151::MCP4151(SPIClass &spi, uint8_t csPin)
  : _spi(spi), _csPin(csPin), _initialized(false), _spiFreq(1000000), _lastPosition(0) {
  // Create mutex for SPI access
  _spiMutex = xSemaphoreCreateMutex();
}

bool MCP4151::begin(uint32_t spiFreq) {
  // Check if mutex was created successfully
  if (_spiMutex == NULL) {
    return false;
  }
  
  // Configure CS pin
  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);  // Deselect the chip initially
  
  // Store SPI frequency for later use
  _spiFreq = spiFreq;
  
  // Try to read the current wiper position to verify communication
  uint8_t position = getWiper();
  if (position != 255) {  // 255 is error code
    _lastPosition = position;
    _initialized = true;
    return true;
  }
  
  return false;
}

bool MCP4151::setWiper(uint8_t position) {
  if (!_initialized) {
    return false;
  }
  
  bool success = writeRegister(MCP4151_REG_WIPER, position);
  if (success) {
    _lastPosition = position;
  }
  
  return success;
}

uint8_t MCP4151::getWiper() {
  if (!_initialized) {
    return 255;  // Error code
  }
  
  uint8_t position = readRegister(MCP4151_REG_WIPER);
  if (position != 255) {  // 255 is error code
    _lastPosition = position;
  }
  
  return position;
}

uint8_t MCP4151::incrementWiper() {
  if (!_initialized) {
    return 255;  // Error code
  }
  
  // Take the SPI mutex with timeout
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Select the chip
    digitalWrite(_csPin, LOW);
    
    // Small delay for stability
    ets_delay_us(1);
    
    // Send increment command (00h for special increment command)
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    _spi.transfer(0x04);  // Increment command
    _spi.endTransaction();
    
    // Deselect the chip
    digitalWrite(_csPin, HIGH);
    
    // Release the mutex
    xSemaphoreGive(_spiMutex);
    
    // Update last position if not at max
    if (_lastPosition < 255) {
      _lastPosition++;
    }
    
    return _lastPosition;
  }
  
  return 255;  // Error code
}

uint8_t MCP4151::decrementWiper() {
  if (!_initialized) {
    return 255;  // Error code
  }
  
  // Take the SPI mutex with timeout
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Select the chip
    digitalWrite(_csPin, LOW);
    
    // Small delay for stability
    ets_delay_us(1);
    
    // Send decrement command
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    _spi.transfer(0x08);  // Decrement command
    _spi.endTransaction();
    
    // Deselect the chip
    digitalWrite(_csPin, HIGH);
    
    // Release the mutex
    xSemaphoreGive(_spiMutex);
    
    // Update last position if not at min
    if (_lastPosition > 0) {
      _lastPosition--;
    }
    
    return _lastPosition;
  }
  
  return 255;  // Error code
}

bool MCP4151::writeRegister(uint8_t reg, uint8_t value) {
  if (_spiMutex == NULL) {
    return false;
  }
  
  bool success = false;
  
  // Take the SPI mutex with timeout
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Select the chip
    digitalWrite(_csPin, LOW);
    
    // Small delay for stability
    ets_delay_us(1);
    
    // Format command byte: command (00 for write) + register address
    uint8_t cmdByte = MCP4151_CMD_WRITE | (reg & 0x0F);
    
    // Send command and data
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    _spi.transfer(cmdByte);
    _spi.transfer(value);
    _spi.endTransaction();
    
    // Deselect the chip
    digitalWrite(_csPin, HIGH);
    
    // Release the mutex
    xSemaphoreGive(_spiMutex);
    
    success = true;
  }
  
  return success;
}

uint8_t MCP4151::readRegister(uint8_t reg) {
  if (_spiMutex == NULL) {
    return 255;  // Error code
  }
  
  uint8_t value = 255;  // Default to error value
  
  // Take the SPI mutex with timeout
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Select the chip
    digitalWrite(_csPin, LOW);
    
    // Small delay for stability
    ets_delay_us(1);
    
    // Format command byte: command (11 for read) + register address
    uint8_t cmdByte = MCP4151_CMD_READ | (reg & 0x0F);
    
    // Send command and read data
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    _spi.transfer(cmdByte);
    value = _spi.transfer(0);  // Send dummy byte to read
    _spi.endTransaction();
    
    // Deselect the chip
    digitalWrite(_csPin, HIGH);
    
    // Release the mutex
    xSemaphoreGive(_spiMutex);
  }
  
  return value;
}