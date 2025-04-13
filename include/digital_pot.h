/*
 * Digital Potentiometer Module Header
 * Provides functions for controlling an MCP4151 digital potentiometer
 */

 #ifndef DIGITAL_POT_H
 #define DIGITAL_POT_H
 
 #include <Arduino.h>
 #include <SPI.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/semphr.h"
 #include "freertos/task.h"
 
 // MCP4151 SPI settings
 #define DIGITAL_POT_CS_PIN 12
 
 // MCP4151 commands
 #define DIGITAL_POT_CMD_WRITE  0x00
 #define DIGITAL_POT_CMD_READ   0x0C
 #define DIGITAL_POT_CMD_INCR   0x04
 #define DIGITAL_POT_CMD_DECR   0x08
 
 // Value limits for the potentiometer (8-bit: 0-255)
 #define DIGITAL_POT_MIN_VALUE  100
 #define DIGITAL_POT_MAX_VALUE  128
 #define DIGITAL_POT_DEFAULT_VALUE 128
 
 // Strength mapping range (application specific)
 #define STRENGTH_MIN_VALUE 10
 #define STRENGTH_MAX_VALUE 250
 
 // External global variable (defined in main.cpp)
 extern volatile uint8_t strength;
 
 /**
  * Initialize the digital potentiometer module
  * @param spi Reference to SPI instance
  * @return true if initialization was successful
  */
 bool initDigitalPot(SPIClass &spi);
 
 /**
  * Set the digital potentiometer value directly (0-255)
  * @param value Wiper position value (0-255)
  * @return true if successful
  */
 bool setDigitalPotValue(uint8_t value);
 
 /**
  * Read the current digital potentiometer value
  * @return Current wiper position (0-255) or 0xFF if error
  */
 uint8_t readDigitalPotValue();
 
 /**
  * Update the digital potentiometer based on the global strength variable
  * Maps the strength value (STRENGTH_MIN_VALUE to STRENGTH_MAX_VALUE) to potentiometer range
  * @return true if successful
  */
 bool updateDigitalPotFromStrength();
 
 /**
  * Create a digital potentiometer monitoring task
  * This task will monitor the global strength variable and update the potentiometer
  * @return true if task creation was successful
  */
 bool createDigitalPotTask();
 
 #endif // DIGITAL_POT_H