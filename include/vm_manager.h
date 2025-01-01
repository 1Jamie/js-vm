#ifndef VM_MANAGER_H
#define VM_MANAGER_H

#include <Arduino.h>
#include <duktape.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#define MAX_VMS 4
#define MAX_MESSAGE_LENGTH 256
#define FS_CHECK_INTERVAL 5000
#define VM_STACK_SIZE 8192

// VM structure
struct VM {
  duk_context* ctx = nullptr;
  TaskHandle_t taskHandle = nullptr;
  SemaphoreHandle_t pinMutex = nullptr;
  QueueHandle_t messageQueue = nullptr;
  String filename;
  String fullPath;
  bool running = false;
  bool needsTermination = false;
  bool forceTerminate = false;
  unsigned long lastFileCheckTime = 0;
  unsigned long lastRunTime = 0;
  size_t memoryAllocated = 0;
  size_t fileSize = 0;
  time_t lastModified = 0;
};

// Extern declarations
extern VM vms[MAX_VMS];
extern int vmCount;
extern SemaphoreHandle_t pinMutexes[40];
extern bool pinInUse[40];

// Function declarations
bool isEnoughMemoryAvailable(size_t memoryNeeded);
int findFreeVMSlot();
void destroyVM(int vmIndex);
int createVM(const String& filename, const char* content, const String& fullPath);
void executeVM(int vmIndex);
int startVM(int vmIndex);
void stopVM(int vmIndex);
void monitorAndRescheduleVMs();

#endif