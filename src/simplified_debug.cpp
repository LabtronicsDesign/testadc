/*
 * Simplified Debug Utilities Implementation
 * Compatible with ESP32-S3 Arduino core
 */

 #include "simplified_debug.h"
 #include <stdio.h>
 #include <stdarg.h>
 
 // Start time for uptime calculation
 static unsigned long startTimeMs = 0;
 
 void debugInit() {
     startTimeMs = millis();
     Serial.println("\n--- DEBUG INITIALIZED ---");
     debugHeapInfo();
 }
 
 void debugPrint(int level, const char* fmt, ...) {
     // Get current uptime
     unsigned long uptimeMs = millis() - startTimeMs;
     unsigned long seconds = uptimeMs / 1000;
     unsigned long minutes = seconds / 60;
     unsigned long hours = minutes / 60;
     
     // Format timestamp
     char timestamp[20];
     sprintf(timestamp, "[%02lu:%02lu:%02lu.%03lu] ", 
             hours, minutes % 60, seconds % 60, uptimeMs % 1000);
     
     // Level prefix
     const char* levelPrefix;
     switch(level) {
         case DEBUG_LEVEL_WARN:
             levelPrefix = "[WARN] ";
             break;
         case DEBUG_LEVEL_ERROR:
             levelPrefix = "[ERROR] ";
             break;
         case DEBUG_LEVEL_INFO:
         default:
             levelPrefix = "[INFO] ";
             break;
     }
     
     // Print timestamp and level
     Serial.print(timestamp);
     Serial.print(levelPrefix);
     
     // Format and print the rest of the message
     char buffer[256];
     va_list args;
     va_start(args, fmt);
     vsnprintf(buffer, sizeof(buffer), fmt, args);
     va_end(args);
     
     Serial.println(buffer);
 }
 
 void debugHeapInfo() {
     // General heap info
     uint32_t freeHeap = ESP.getFreeHeap();
     uint32_t totalHeap = ESP.getHeapSize();
     uint32_t minFreeHeap = ESP.getMinFreeHeap();
     
     // IRAM heap info (instruction memory)
     uint32_t freeIRAM = heap_caps_get_free_size(MALLOC_CAP_32BIT);
     
     // Print heap info
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Heap - Free: %lu bytes, Total: %lu bytes, Used: %lu bytes (%.1f%%)",
                freeHeap, totalHeap, totalHeap - freeHeap, 
                100.0 * (totalHeap - freeHeap) / totalHeap);
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Heap - Min Free Ever: %lu bytes", minFreeHeap);
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "IRAM Free: %lu bytes", freeIRAM);
 }
 
 void debugStackInfo(const char* taskName) {
     // Get stack high water mark for current task
     UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
     
     // Print task stack info
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Task '%s' - Stack Free: %lu bytes", 
                taskName, 
                stackHighWaterMark * sizeof(StackType_t));
 }
 
 void debugStartTask(const char* taskName) {
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Task '%s' STARTED", taskName);
     
     // Print current task stack info
     debugStackInfo(taskName);
     
     // Print heap info on task start
     debugHeapInfo();
 }
 
 void debugEndTask(const char* taskName) {
     // Print current task stack info before ending
     debugStackInfo(taskName);
     
     DEBUG_PRINT(DEBUG_LEVEL_INFO, "Task '%s' COMPLETED", taskName);
 }