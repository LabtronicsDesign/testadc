/*
 * Simplified Debug Utilities
 * Compatible with ESP32-S3 Arduino core
 */

 #ifndef SIMPLIFIED_DEBUG_H
 #define SIMPLIFIED_DEBUG_H
 
 #include <Arduino.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_heap_caps.h"
 
 // Debug control - comment out to disable debug output
 #define DEBUG_ENABLED
 
 // Debug log levels
 #define DEBUG_LEVEL_INFO  1
 #define DEBUG_LEVEL_WARN  2
 #define DEBUG_LEVEL_ERROR 3
 
 // Set the current debug level
 #define DEBUG_LEVEL DEBUG_LEVEL_WARN
 
 // Debug macros
 #ifdef DEBUG_ENABLED
     #define DEBUG_PRINT(level, fmt, ...) if (level >= DEBUG_LEVEL) { debugPrint(level, fmt, ##__VA_ARGS__); }
     #define DEBUG_STACK_INFO(taskName) debugStackInfo(taskName)
     #define DEBUG_HEAP_INFO() debugHeapInfo()
     #define DEBUG_START_TASK(name) debugStartTask(name)
     #define DEBUG_END_TASK(name) debugEndTask(name)
     #define DEBUG_INIT() debugInit()
 #else
     #define DEBUG_PRINT(level, fmt, ...) 
     #define DEBUG_STACK_INFO(taskName)
     #define DEBUG_HEAP_INFO()
     #define DEBUG_START_TASK(name)
     #define DEBUG_END_TASK(name)
     #define DEBUG_INIT()
 #endif
 
 /**
  * Initialize debug utilities
  */
 void debugInit();
 
 /**
  * Print formatted debug message with timestamp and level indicator
  * @param level Debug level (1=INFO, 2=WARN, 3=ERROR)
  * @param fmt Format string
  * @param ... Additional arguments for format string
  */
 void debugPrint(int level, const char* fmt, ...);
 
 /**
  * Print current heap usage info
  */
 void debugHeapInfo();
 
 /**
  * Print stack high water mark for current task
  * @param taskName Name of the task for logging purposes
  */
 void debugStackInfo(const char* taskName);
 
 /**
  * Print message when a task starts
  * @param taskName Name of the task starting
  */
 void debugStartTask(const char* taskName);
 
 /**
  * Print message when a task ends
  * @param taskName Name of the task ending
  */
 void debugEndTask(const char* taskName);
 
 #endif // SIMPLIFIED_DEBUG_H