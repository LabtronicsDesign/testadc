#ifndef MAX17048_H
#define MAX17048_H

#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// MAX17048 I2C Address
#define MAX17048_ADDR 0x36

// MAX17048 Register Addresses
#define MAX17048_VCELL     0x02 // Voltage register
#define MAX17048_SOC       0x04 // State of Charge register
#define MAX17048_MODE      0x06 // Mode register
#define MAX17048_VERSION   0x08 // Version register
#define MAX17048_HIBRT     0x0A // Hibernate register
#define MAX17048_CONFIG    0x0C // Configuration register
#define MAX17048_VALRT     0x14 // Voltage alert register
#define MAX17048_CRATE     0x16 // Current rate register
#define MAX17048_VRESET    0x18 // Voltage reset register
#define MAX17048_STATUS    0x1A // Status register
#define MAX17048_CMD       0xFE // Command register

class MAX17048 {
  public:
    /**
     * Constructor with external Wire instance
     * @param wire Reference to an existing TwoWire (I2C) instance
     */
    MAX17048(TwoWire &wire);
    
    /**
     * Initialize the fuel gauge
     * @param alertThreshold Low battery alert threshold (0-32% in 1% steps)
     */
    void begin(uint8_t alertThreshold = 20);
    
    /**
     * Read the battery voltage
     * @return Battery voltage in millivolts or 0 if error
     */
    uint16_t readVoltage();
    
    /**
     * Read the battery state of charge
     * @return Battery state of charge percentage (0-100) or 255 if error
     */
    uint8_t readSOC();
    
    /**
     * Read the version of the chip
     * @return Version number or 0 if error
     */
    uint16_t readVersion();
    
    /**
     * Set the low battery alert threshold
     * @param threshold Percentage threshold (0-32%)
     * @return true if successful
     */
    bool setAlertThreshold(uint8_t threshold);
    
    /**
     * Check if an alert is active
     * @return true if alert is active
     */
    bool isAlertActive();
    
    /**
     * Clear any active alerts
     * @return true if successful
     */
    bool clearAlert();

  private:
    TwoWire &_wire;
    bool _initialized;
    SemaphoreHandle_t _i2cMutex;
    
    // Helper functions for I2C operations
    bool writeRegister(uint8_t reg, uint16_t value);
    bool readRegister(uint8_t reg, uint16_t *value);
};

#endif // MAX17048_H