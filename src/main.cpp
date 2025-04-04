/*
 * Combined Example with AD7495 ADC and MAX17048 Fuel Gauge
 * Main application file with ESP32-S3 support and simplified debug utilities
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc_tasks.h"
#include "battery_tasks.h"
#include "gpio_expander_tasks.h"
#include "beeper.h"
#include "pulse_generator.h"
#include "simplified_debug.h"

// Pin definitions
// SPI pins
#define MISO_PIN MISO
#define MOSI_PIN MOSI // Not used for ADC
#define SCLK_PIN SCK
#define CS_PIN_ADC 5

// I2C pins
#define SDA_PIN SDA
#define SCL_PIN SCL

// Global peripheral instances - use HSPI for ESP32-S3
SPIClass sharedSPI = SPIClass(HSPI);
TwoWire &sharedI2C = Wire;

// Flags for battery status - volatile since accessed from multiple tasks
volatile bool lowBatteryFlag = false;       // Can be read by other tasks
volatile bool isChargingFlag = false;       // True when battery is charging
volatile bool chargeCompleteFlag = false;   // True when battery is fully charged
volatile bool batteryConnectedFlag = false; // True when slide switch connects battery to output

// Flags for button states
volatile bool button0Pressed = false;
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile bool button3Pressed = false;
volatile bool gpioExpanderBattAlertActive = false;

// Global variables
volatile uint16_t pFrequency = PULSE_DEFAULT_FREQ; // Default 100Hz
volatile bool pulseEn = false;                     // Initially disabled

// Task handle for control task
TaskHandle_t controlTaskHandle = NULL;

// Debug monitor task - periodically outputs system status
void debugMonitorTask(void *pvParameters)
{
  const TickType_t monitorDelay = pdMS_TO_TICKS(30000); // Every 30 seconds

  while (1)
  {
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "--- PERIODIC SYSTEM STATS ---");

    // Print system-wide memory stats
    DEBUG_HEAP_INFO();

    // Print stack info for monitor task
    DEBUG_STACK_INFO("Debug Monitor");

    vTaskDelay(monitorDelay);
  }
}

// Battery switch change event handler - called from main task context
void handleSwitchChange()
{
  static bool lastSwitchState = false;

  // Only process if state has changed since last check
  if (batteryConnectedFlag != lastSwitchState)
  {
    lastSwitchState = batteryConnectedFlag;

    // Log the change
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Switch state changed to: %s",
                batteryConnectedFlag ? "CONNECTED" : "DISCONNECTED");

    Serial.print("\n*** ON/OFF Switch changed: Battery ");
    Serial.print(batteryConnectedFlag ? "CONNECTED" : "DISCONNECTED");
    Serial.println(" ***");

    // Add any immediate actions needed when switch changes
    // For example, you might want to put the device into low power mode when disconnected
    if (!batteryConnectedFlag)
    {
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Taking actions for disconnected state...");
      Serial.println("Taking actions for disconnected state...");
      // e.g., disable high-power peripherals
    }
    else
    {
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Resuming normal operation...");
      Serial.println("Resuming normal operation...");
      // e.g., resume normal functionality
    }
  }
}

// Main control task that triggers other tasks
void controlTask(void *pvParameters)
{
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Control Task Started");

  while (1)
  {
    // Check for switch changes (battery connection state)
    handleSwitchChange();

    // Log memory stats at the start of each cycle
    DEBUG_STACK_INFO("Control Task");
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "--- Starting control task cycle ---");

    // ----- Trigger ADC sampling -----
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating ADC task");

    if (!createAdcTask())
    {
      DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create ADC task");
    }

    // ----- Trigger battery monitoring -----
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating Battery task");

    if (!createBatteryTask())
    {
      DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create Battery task");
    }

    // ----- Wait for ADC results -----
    AdcResult_t adcResult;
    if (receiveAdcResults(&adcResult, pdMS_TO_TICKS(5000)))
    {
      if (adcResult.success)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC samples received successfully");

        for (int i = 0; i < SAMPLES_PER_BATCH; i++)
        {
          Serial.println(adcResult.samples[i]);
        }
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "ADC batch captured in %lu ms", adcResult.captureTimeMs);
      }
      else
      {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "ADC task reported failure");
      }
    }
    else
    {
      DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for ADC results");
    }

    // ----- Wait for battery results -----
    BatteryStatus_t battStatus;
    if (receiveBatteryResults(&battStatus, pdMS_TO_TICKS(5000)))
    {
      if (battStatus.success)
      {
        // Print the battery status
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery status: %dmW, %d%%", battStatus.voltage, battStatus.soc);

        // Print TP4056 charging status
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Charging Status: %s", getChargingStatusString(battStatus.chrgStatus));

        // Print slide switch status
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery Output: %s", battStatus.switchState ? "Connected" : "Disconnected");

        // Update charging flags
        isChargingFlag = (battStatus.chrgStatus == CHARGING);
        chargeCompleteFlag = (battStatus.chrgStatus == CHARGE_COMPLETE);

        // Take action based on battery capacity and update flag
        if (battStatus.soc <= BATT_ALERT_THRESHOLD)
        {
          // Low battery condition
          if (!lowBatteryFlag)
          {
            // First time detecting low battery
            DEBUG_PRINT(DEBUG_LEVEL_WARN, "BATTERY LOW - CRITICAL LEVEL! SOC: %u%%", battStatus.soc);
            lowBatteryFlag = true;
          }
        }
        else if (battStatus.soc >= BATT_ALERT_THRESHOLD + 5)
        {
          // Battery recovered (with 5% hysteresis)
          if (lowBatteryFlag)
          {
            DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery level recovered to %u%%", battStatus.soc);
            lowBatteryFlag = false;
          }
        }

        // Print the alert status separately
        if (battStatus.isAlert)
        {
          DEBUG_PRINT(DEBUG_LEVEL_WARN, "Battery ALERT active");
        }
        else
        {
          DEBUG_PRINT(DEBUG_LEVEL_INFO, "Normal");
        }

        // Print a summary of the global flags that other tasks can access
        DEBUG_PRINT(DEBUG_LEVEL_INFO,
                    "Battery Flags - Low: %s, Charging: %s, Complete: %s, Connected: %s",
                    lowBatteryFlag ? "YES" : "NO",
                    isChargingFlag ? "YES" : "NO",
                    chargeCompleteFlag ? "YES" : "NO",
                    batteryConnectedFlag ? "YES" : "NO");
      }
      else
      {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Battery task reported failure");
      }
    }
    else
    {
      DEBUG_PRINT(DEBUG_LEVEL_WARN, "Timeout waiting for battery results");
    }

    // ----- Check GPIO Expander Status -----
    GpioExpanderStatus_t gpioStatus;
    if (receiveGpioExpanderStatus(&gpioStatus, pdMS_TO_TICKS(10)))
    { // Short timeout, non-blocking
      if (gpioStatus.success)
      {
        // Print battery alert state
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "GPIO Battery Alert: %s", (gpioStatus.inputState & GPIO_EXPANDER_BATT_ALRT) ? "Inactive" : "Active");

        // Print ELEC_SHDN state
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "ELEC_SHDN: %s", (gpioStatus.outputState & GPIO_EXPANDER_ELEC_SHDN) ? "Active" : "Inactive");

        // Update global button states
        button0Pressed = !(gpioStatus.inputState & GPIO_EXPANDER_BTN0);
        button1Pressed = !(gpioStatus.inputState & GPIO_EXPANDER_BTN1);
        button2Pressed = !(gpioStatus.inputState & GPIO_EXPANDER_BTN2);
        button3Pressed = !(gpioStatus.inputState & GPIO_EXPANDER_BTN3);
        gpioExpanderBattAlertActive = !(gpioStatus.inputState & GPIO_EXPANDER_BATT_ALRT);
      }
    }

    // Check for any button events (non-blocking)
    GpioExpanderEvent_t buttonEvent;
    if (waitForButtonEvent(&buttonEvent, 0, 0))
    { // Zero timeout, just check for any pending events
      // Log button events
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button event detected - ");

      // Identify which button/input
      if (buttonEvent.buttonMask == GPIO_EXPANDER_BTN0)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button 0 ");
        button0Pressed = (buttonEvent.eventType == BUTTON_PRESSED);
      }
      else if (buttonEvent.buttonMask == GPIO_EXPANDER_BTN1)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button 1 ");
        button1Pressed = (buttonEvent.eventType == BUTTON_PRESSED);
      }
      else if (buttonEvent.buttonMask == GPIO_EXPANDER_BTN2)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button 2 ");
        button2Pressed = (buttonEvent.eventType == BUTTON_PRESSED);
      }
      else if (buttonEvent.buttonMask == GPIO_EXPANDER_BTN3)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button 3 ");
        button3Pressed = (buttonEvent.eventType == BUTTON_PRESSED);
      }
      else if (buttonEvent.buttonMask == GPIO_EXPANDER_BATT_ALRT)
      {
        gpioExpanderBattAlertActive = (buttonEvent.eventType == BATTERY_ALERT_ACTIVE);
      }

      // Print the event type
      if (buttonEvent.eventType == BUTTON_PRESSED)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "PRESSED");
      }
      else if (buttonEvent.eventType == BUTTON_RELEASED)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "RELEASED");
      }
      else if (buttonEvent.eventType == BATTERY_ALERT_ACTIVE)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery Alert ACTIVE");
      }
      else if (buttonEvent.eventType == BATTERY_ALERT_INACTIVE)
      {
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Battery Alert INACTIVE");
      }

      // Example: Toggle ELEC_SHDN when Button 0 is pressed
      if (buttonEvent.buttonMask == GPIO_EXPANDER_BTN0 && buttonEvent.eventType == BUTTON_PRESSED)
      {
        bool currentState = getElecShutdownState();
        setElecShutdown(!currentState);
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "ELEC_SHDN toggled to: %s", !currentState ? "ON" : "OFF");
      }
    }

    // ----- Check Pulse Generator Status -----
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Generator status:, Frequency: %d Hz, Enabled %s", pFrequency, pulseEn ? "YES" : "NO");

    // Example: Toggle pulse generator with Button 1 and adjust frequency with Buttons 2 and 3
    if (button1Pressed)
    {
      // Toggle pulse generator enable
      pulseEn = !pulseEn;
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse Generator %s", pulseEn ? "ENABLED" : "DISABLED");
    }

    if (button2Pressed)
    {
      // Decrease frequency by 10Hz (with lower limit)
      if (pFrequency >= PULSE_MIN_FREQ + 10)
      {
        pFrequency -= 10;
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse frequency decreased to %d Hz", pFrequency);
      }
    }

    if (button3Pressed)
    {
      // Increase frequency by 10Hz (with upper limit)
      if (pFrequency <= PULSE_MAX_FREQ - 10)
      {
        pFrequency += 10;
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Pulse frequency increased to %d Hz", pFrequency);
      }
    }

    // Wait before next cycle - using a shorter delay to be more responsive to switch changes
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Control task cycle completed, waiting for next cycle");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Run every second
  }
}

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  Serial.println("\nESP32-S3 Combined ADC and Battery Monitor Example");

  // Initialize debug utilities
  DEBUG_INIT();
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "ESP32-S3 Combined ADC and Battery Monitor Example");
  DEBUG_HEAP_INFO();

  // Initialize the shared SPI bus
  sharedSPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN);
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "SPI initialized (HSPI) - SCLK: %d, MISO: %d, MOSI: %d",
              SCLK_PIN, MISO_PIN, MOSI_PIN);

  // Initialize the shared I2C bus
  sharedI2C.begin(SDA_PIN, SCL_PIN);
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "I2C initialized - SDA: %d, SCL: %d", SDA_PIN, SCL_PIN);

  // Initialize modules
  Serial.println("Initializing ADC module...");
  if (!initAdcModule(sharedSPI, CS_PIN_ADC))
  {
    Serial.println("Failed to initialize ADC module! Halting.");
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "ADC module initialization failed!");
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  Serial.println("Initializing Battery module...");
  if (!initBatteryModule(sharedI2C))
  {
    Serial.println("Failed to initialize Battery module! Halting.");
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Battery module initialization failed!");
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  Serial.println("Initializing GPIO Expander module...");
  if (!initGpioExpanderModule(sharedI2C))
  {
    Serial.println("Failed to initialize GPIO Expander module! Halting.");
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "GPIO Expander module initialization failed!");
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // Create GPIO expander task (unlike ADC and battery tasks, this one is persistent)
  Serial.println("Creating GPIO Expander task...");
  if (!createGpioExpanderTask())
  {
    Serial.println("Failed to create GPIO Expander task! Halting.");
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create GPIO Expander task!");
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  Serial.println("Initializing beeper...");
  initBeeper();
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Beeper initialized on GPIO %d", BEEPER_PIN);

  Serial.println("Initializing Pulse Generator...");
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Attempting to initialize Pulse Generator module");

  // We'll try to initialize but won't treat failure as fatal
  if (!initPulseGenerator(sharedI2C))
  {
    Serial.println("Warning: Failed to initialize Pulse Generator module!");
    Serial.println("System will continue without pulse generator functionality.");
    DEBUG_PRINT(DEBUG_LEVEL_WARN, "Pulse Generator module initialization failed - continuing without it");
  }
  else
  {
    // Only create the task if initialization succeeded
    Serial.println("Creating Pulse Generator task...");
    if (!createPulseGeneratorTask())
    {
      Serial.println("Warning: Failed to create Pulse Generator task!");
      DEBUG_PRINT(DEBUG_LEVEL_WARN, "Failed to create Pulse Generator task");
    }
  }

  // Create control task
  Serial.println("Creating control task...");
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Creating control task");

  BaseType_t result = xTaskCreate(
      controlTask,
      "Control Task",
      4096,
      NULL,
      3, // Medium priority
      &controlTaskHandle);

  if (result != pdPASS)
  {
    Serial.println("Failed to create control task! Halting.");
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to create control task!");
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

#ifdef DEBUG_ENABLED
  // Create debug monitor task if debugging is enabled
  xTaskCreate(
      debugMonitorTask,
      "Debug Monitor",
      4096,
      NULL,
      1, // Low priority
      NULL);
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Debug monitor task created");
#endif

  // startup beep sequence to indicate successful initialization
  shortBeep();
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait 100ms
  shortBeep();
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Setup complete");
  Serial.println("Setup complete");
}

void loop()
{
  // Empty loop - all work is done in tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}