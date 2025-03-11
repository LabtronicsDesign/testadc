#include "MAX17048.h"

MAX17048::MAX17048(TwoWire &wire)
  : _wire(wire), _initialized(false) {
  // Create mutex for I2C access
  _i2cMutex = xSemaphoreCreateMutex();
}

void MAX17048::begin(uint8_t alertThreshold) {
  // Check if mutex was created successfully
  if (_i2cMutex == NULL) {
    return;
  }
  
  // Validate the alert threshold (valid range is 1-32%)
  if (alertThreshold > 32) {
    alertThreshold = 32;
  }
  
  // Reset all alerts first
  uint16_t configReg;
  if (readRegister(MAX17048_CONFIG, &configReg)) {
    // Clear ALL alert bits - bits 5,6,7 in the CONFIG register
    configReg &= ~(0x00E0);  // Clear bits 5,6,7 
    
    // Write the updated config back
    writeRegister(MAX17048_CONFIG, configReg);
  }
  
  // Wait a bit for the chip to process
  delay(10);
  
  // Read STATUS register and clear it (write 0 to clear)
  uint16_t statusReg;
  if (readRegister(MAX17048_STATUS, &statusReg)) {
    writeRegister(MAX17048_STATUS, 0x0000);
  }
  
  // Configure alert threshold
  setAlertThreshold(alertThreshold);
  
  // Check if we can read the version register to verify communication
  uint16_t version;
  if (readRegister(MAX17048_VERSION, &version)) {
    _initialized = true;
  }
}

uint16_t MAX17048::readVoltage() {
  if (!_initialized) {
    return 0;
  }
  
  uint16_t voltage;
  if (!readRegister(MAX17048_VCELL, &voltage)) {
    return 0;
  }
  
  // Convert raw value to millivolts (78.125uV/cell per bit)
  return (uint16_t)((voltage * 78125UL) / 1000000UL);
}

uint8_t MAX17048::readSOC() {
  if (!_initialized) {
    return 255;  // Error code
  }
  
  uint16_t soc;
  if (!readRegister(MAX17048_SOC, &soc)) {
    return 255;
  }
  
  // Convert raw value to percentage (1% = 256)
  return (uint8_t)(soc >> 8);
}

uint16_t MAX17048::readVersion() {
  if (!_initialized) {
    return 0;
  }
  
  uint16_t version;
  if (!readRegister(MAX17048_VERSION, &version)) {
    return 0;
  }
  
  return version;
}

bool MAX17048::setAlertThreshold(uint8_t threshold) {
  if (!_initialized && _i2cMutex == NULL) {
    return false;
  }
  
  // Cap the threshold to valid range (0-32%)
  if (threshold > 32) {
    threshold = 32;
  }
  
  // Read current CONFIG register
  uint16_t config;
  if (!readRegister(MAX17048_CONFIG, &config)) {
    return false;
  }
  
  // First, clear any existing alert flags (bits 5,6,7)
  config &= ~(0x00E0);
  
  // Then clear the alert threshold bits (bits 0-4)
  config &= 0xFFE0;
  
  // Set the new threshold (ALRT_Threshold = 32 - threshold)
  config |= (32 - threshold);
  
  // Write back updated config
  return writeRegister(MAX17048_CONFIG, config);
}

bool MAX17048::isAlertActive() {
  if (!_initialized) {
    return false;
  }
  
  // First check CONFIG register (bit 5 is ALRT bit)
  uint16_t config;
  if (!readRegister(MAX17048_CONFIG, &config)) {
    return false;
  }
  
  // If the ALRT bit is set, there's an active alert
  if (config & 0x0020) {
    return true;
  }
  
  // Also check STATUS register 
  uint16_t status;
  if (!readRegister(MAX17048_STATUS, &status)) {
    return false;
  }
  
  // Check various alert bits in STATUS register
  // Bits 10 (RI), 9 (VL), 8 (VH), etc. can indicate alerts
  if (status & 0x0700) {  // Mask for bits 8, 9, and 10
    return true;
  }
  
  return false;
}

bool MAX17048::clearAlert() {
  if (!_initialized) {
    return false;
  }
  
  bool success = true;
  
  // 1. Clear the CONFIG register alert bit
  uint16_t config;
  if (readRegister(MAX17048_CONFIG, &config)) {
    // Clear ALRT bit (bit 5) and any other alert bits (6,7)
    config &= ~(0x00E0);  // Clear bits 5,6,7
    
    if (!writeRegister(MAX17048_CONFIG, config)) {
      success = false;
    }
  } else {
    success = false;
  }
  
  // 2. Clear the STATUS register by writing zeros
  if (!writeRegister(MAX17048_STATUS, 0x0000)) {
    success = false;
  }
  
  // 3. Re-set the alert threshold to ensure it's properly configured
  uint8_t threshold = config & 0x001F;  // Get current threshold from bits 0-4
  threshold = 32 - threshold;  // Convert to percentage (0-32)
  
  if (!setAlertThreshold(threshold)) {
    success = false;
  }
  
  return success;
}

// Private methods for I2C communication
bool MAX17048::writeRegister(uint8_t reg, uint16_t value) {
  if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  
  bool success = true;
  
  _wire.beginTransmission(MAX17048_ADDR);
  _wire.write(reg);
  _wire.write((value >> 8) & 0xFF);  // High byte first
  _wire.write(value & 0xFF);         // Low byte
  
  if (_wire.endTransmission() != 0) {
    success = false;
  }
  
  xSemaphoreGive(_i2cMutex);
  return success;
}

bool MAX17048::readRegister(uint8_t reg, uint16_t *value) {
  if (value == NULL) {
    return false;
  }
  
  if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  
  bool success = true;
  
  // Set register pointer
  _wire.beginTransmission(MAX17048_ADDR);
  _wire.write(reg);
  if (_wire.endTransmission() != 0) {
    success = false;
  }
  
  // Read 2 bytes
  if (success) {
    if (_wire.requestFrom(MAX17048_ADDR, 2) != 2) {
      success = false;
    } else {
      uint8_t highByte = _wire.read();
      uint8_t lowByte = _wire.read();
      *value = ((uint16_t)highByte << 8) | lowByte;
    }
  }
  
  xSemaphoreGive(_i2cMutex);
  return success;
}