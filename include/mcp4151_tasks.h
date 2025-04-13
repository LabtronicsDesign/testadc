/*
 * MCP4151 Tasks Module Header
 * Provides functions for MCP4151 digital potentiometer control in a separate high-priority task
 */

 #ifndef MCP4151_TASKS_H
 #define MCP4151_TASKS_H
 
 #include <Arduino.h>
 #include <SPI.h>
 #include "freertos/FreeRTOS.h"
 #include "MCP4151.h"
 
 // Data structure for digipot results
 typedef struct {
   uint8_t position;     // Current wiper position
   bool success;         // Whether the operation was successful
 } DigipotResult_t;
 
 // Digipot operation types
 typedef enum {
   DIGIPOT_OP_SET,       // Set wiper to absolute position
   DIGIPOT_OP_INCREMENT, // Increment wiper
   DIGIPOT_OP_DECREMENT, // Decrement wiper
   DIGIPOT_OP_READ       // Read wiper position only
 } DigipotOp_t;
 
 /**
  * Initialize the digipot module
  * @param spi Reference to SPI instance to use with the MCP4151
  * @param csPin Chip select pin for the digipot
  * @return true if initialization was successful
  */
 bool initDigipotModule(SPIClass &spi, uint8_t csPin);
 
 /**
  * Create a high-priority digipot control task
  * The task will self-delete after completion
  * @param operation The operation to perform
  * @param position The position to set (0-255) - only used for DIGIPOT_OP_SET
  * @return true if task creation was successful
  */
 bool createDigipotTask(DigipotOp_t operation, uint8_t position = 0);
 
 /**
  * Receive digipot results from the task
  * @param result Pointer to store the digipot results
  * @param timeout Maximum time to wait for results
  * @return true if results were received
  */
 bool receiveDigipotResults(DigipotResult_t *result, TickType_t timeout);
 
 /**
  * Get the last known wiper position
  * This doesn't communicate with the device, just returns cached value
  * @return Last known position or 255 if unknown
  */
 uint8_t getLastWiperPosition();
 
 #endif // MCP4151_TASKS_H