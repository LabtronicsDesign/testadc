/*
 * Pulse Generator Module Header
 * Provides functions for controlling a PCA9685 PWM controller
 * for generating pulses at a configurable frequency with 50% duty cycle
 */

 #ifndef PULSE_GENERATOR_H
 #define PULSE_GENERATOR_H
 
 #include <Arduino.h>
 #include <Wire.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/semphr.h"
 #include "freertos/task.h"
 
 // PCA9685 I2C address (default is 0x40)
 #define PCA9685_ADDR 0x40
 
 // PCA9685 register addresses
 #define PCA9685_MODE1        0x00
 #define PCA9685_MODE2        0x01
 #define PCA9685_SUBADR1      0x02
 #define PCA9685_SUBADR2      0x03
 #define PCA9685_SUBADR3      0x04
 #define PCA9685_PRESCALE     0xFE
 #define PCA9685_LED0_ON_L    0x06
 #define PCA9685_LED0_ON_H    0x07
 #define PCA9685_LED0_OFF_L   0x08
 #define PCA9685_LED0_OFF_H   0x09
 #define PCA9685_ALL_LED_ON_L 0xFA
 #define PCA9685_ALL_LED_ON_H 0xFB
 #define PCA9685_ALL_LED_OFF_L 0xFC
 #define PCA9685_ALL_LED_OFF_H 0xFD
 
 // Mode1 register bits
 #define PCA9685_RESTART      0x80
 #define PCA9685_EXTCLK       0x40
 #define PCA9685_AI           0x20
 #define PCA9685_SLEEP        0x10
 #define PCA9685_SUB1         0x08
 #define PCA9685_SUB2         0x04
 #define PCA9685_SUB3         0x02
 #define PCA9685_ALLCALL      0x01
 
 // Mode2 register bits
 #define PCA9685_INVRT        0x10
 #define PCA9685_OCH          0x08
 #define PCA9685_OUTDRV       0x04
 #define PCA9685_OUTNE1       0x02
 #define PCA9685_OUTNE0       0x01
 
 // ESP32 GPIO pin for PCA9685 enable
 #define PULSE_ENABLE_PIN 7
 
 // PCA9685 output channels
 #define PULSE_CHANNEL_1 6  // Port 6
 #define PULSE_CHANNEL_2 7  // Port 7
 
 // Frequency limits (in Hz)
 #define PULSE_MIN_FREQ 24
 #define PULSE_MAX_FREQ 1526
 #define PULSE_DEFAULT_FREQ 100
 
 // External global variables (defined in main.cpp)
 extern volatile uint16_t pFrequency;
 extern volatile bool pulseEn;
 
 /**
  * Initialize the pulse generator module
  * @param wire Reference to I2C instance
  * @return true if initialization was successful
  */
 bool initPulseGenerator(TwoWire &wire);
 
 /**
  * Set the pulse frequency
  * @param freq Frequency in Hz (24-1526 Hz)
  * @return true if successful
  */
 bool setPulseFrequency(uint16_t freq);
 
 /**
  * Enable or disable the pulse generator
  * @param enable true to enable, false to disable
  * @return true if successful
  */
 bool enablePulseGenerator(bool enable);
 
 /**
  * Update the pulse generator based on global settings
  * This should be called periodically to check if settings have changed
  * @return true if successful
  */
 bool updatePulseGenerator();
 
 /**
  * Create a pulse generator monitoring task
  * This task will monitor the global variables and update the pulse generator
  * @return true if task creation was successful
  */
 bool createPulseGeneratorTask();
 
 #endif // PULSE_GENERATOR_H