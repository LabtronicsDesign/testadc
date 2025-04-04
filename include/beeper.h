/*
 * Beeper Module Header
 * Provides functions for controlling a piezo beeper
 */

 #ifndef BEEPER_H
 #define BEEPER_H
 
 #include <Arduino.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 // Beeper GPIO pin
 #define BEEPER_PIN 21
 
 // Beeper tone frequency and duration constants
 #define BEEP_FREQUENCY_HZ 4000    // 2kHz tone
 #define BEEP_DURATION_MS 50       // 50ms duration for a short beep
 #define BEEP_BUTTON_PRESS_MS 30   // Shorter beep for button press
 
 /**
  * Initialize the beeper
  */
 void initBeeper();
 
 /**
  * Generate a beep with specified frequency and duration
  * 
  * @param frequency Frequency in Hz
  * @param duration Duration in milliseconds
  */
 void beep(uint16_t frequency, uint16_t duration);
 
 /**
  * Generate a short beep using default frequency and duration
  */
 void shortBeep();
 
 /**
  * Generate a button press beep (even shorter)
  */
 void buttonBeep();
 
 #endif // BEEPER_H