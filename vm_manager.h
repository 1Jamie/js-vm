// vm_manager.h
#ifndef VM_MANAGER_H
#define VM_MANAGER_H

#include <Arduino.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "duktape.h"

// Configuration
#define MAX_VMS 5
#define MAX_FILENAME_LEN 32
#define DEFAULT_VM_MEMORY (16 * 1024) 
#define SPIFFS_PARTITION "spiffs"
#define MAX_MESSAGE_LENGTH 256
#define SPIFFS_BLOCK_SIZE 512
#define MAX_FILE_SIZE (4 * 1024)
#define SPIFFS_CHECK_INTERVAL 5000
#define VM_STACK_SIZE 8192

// VM Structure
typedef struct {
  String filename;
  String fullPath;
  duk_context *ctx;
  bool running;
  unsigned long lastRunTime;
  TaskHandle_t taskHandle;
  SemaphoreHandle_t pinMutex;
  size_t memoryAllocated;
  int core;
  bool needsRestart;
  bool needsTermination;
  QueueHandle_t messageQueue;
  unsigned long lastFileCheckTime;
  unsigned long fileSize;
  bool fileExists;
  bool fileChanged;
} VM;

// Declare these as extern so other modules can use them
extern VM vms[MAX_VMS];
extern int vmCount;
extern SemaphoreHandle_t pinMutexes[40]; 
extern bool pinInUse[40];

// Function Declarations
unsigned long get_ms();
bool isEnoughMemoryAvailable(size_t memoryNeeded);
int createVM(const String& filename);
void startVM(int vmIndex);
void stopVM(int vmIndex);
void registerDuktapeBindings(duk_context *ctx, int vmIndex);
void monitorAndRescheduleVMs();
duk_ret_t duk_digitalWrite(duk_context *ctx);
duk_ret_t duk_digitalRead(duk_context *ctx);
duk_ret_t duk_analogRead(duk_context *ctx);
duk_ret_t duk_analogWrite(duk_context *ctx);
duk_ret_t duk_pinMode(duk_context *ctx);
duk_ret_t duk_delay(duk_context *ctx);
duk_ret_t duk_udpSend(duk_context *ctx);
duk_ret_t duk_udpReceive(duk_context *ctx);
duk_ret_t duk_sendMessage(duk_context *ctx);
duk_ret_t duk_receiveMessage(duk_context *ctx);
duk_ret_t duk_print(duk_context *ctx);
duk_ret_t duk_wait(duk_context *ctx);

#endif 