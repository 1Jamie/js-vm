#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include "duktape.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include "ESP-FTP-Server-Lib.h"
#include "FTPFilesystem.h"

// Configuration (Adjust as needed)
#define MAX_VMS 5
#define MAX_FILENAME_LEN 32
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define UDP_PORT 4210
#define DEFAULT_VM_MEMORY (16 * 1024) // 16KB
#define SPIFFS_PARTITION "spiffs"
#define MAX_MESSAGE_LENGTH 256
#define SPIFFS_BLOCK_SIZE 512
#define MAX_FILE_SIZE (4 * 1024)
#define SPIFFS_CHECK_INTERVAL 5000
#define VM_STACK_SIZE 8192
#define SERIAL_RX_BUFFER_SIZE 64
#define FTP_USER "esp32"
#define FTP_PASSWORD "esp32"

// VM Structure
typedef struct
{
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

VM vms[MAX_VMS];
int vmCount = 0;

// Pin Management
SemaphoreHandle_t pinMutexes[40];
bool pinInUse[40] = {false}; // Initialize pinInUse array

// Networking
WiFiUDP udp;
FTPServer ftp; // Create an instance of the FTPServer

// Use Serial for USB CDC communication
char serial_rx_buffer[SERIAL_RX_BUFFER_SIZE];
uint32_t serial_rx_buffer_head = 0;
uint32_t serial_rx_buffer_tail = 0;

// Helper function to get current time in milliseconds.
unsigned long get_ms()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// === Memory Management ===
bool isEnoughMemoryAvailable(size_t memoryNeeded)
{
  return ESP.getFreeHeap() > memoryNeeded + 16384;
}

// === VM Management ===
duk_ret_t loadAndRunJS(duk_context *ctx, const String &filename)
{
  // Open the file with the "/SPIFFS/" prefix
  File file = SPIFFS.open("/SPIFFS/" + filename, "r");
  if (!file)
  {
    Serial.printf("Error opening file in /SPIFFS: %s\n", filename.c_str());
    return -1;
  }

  size_t fileSize = file.size();
  if (fileSize > MAX_FILE_SIZE)
  {
    Serial.printf("File %s exceeds maximum size (%d bytes)\n", filename.c_str(), MAX_FILE_SIZE);
    file.close();
    return -1;
  }
  char *buffer = new char[fileSize + 1];
  if (buffer == nullptr)
  {
    Serial.println("Memory allocation failed!");
    file.close();
    return -1;
  }
  file.readBytes(buffer, fileSize);
  buffer[fileSize] = '\0';
  file.close();

  duk_push_string(ctx, buffer);
  if (duk_pcompile_string(ctx, DUK_COMPILE_EVAL, buffer) != 0)
  { // Compile the code
    Serial.printf("%s: %s\n", filename.c_str(), duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    delete[] buffer;
    return -1;
  }
  if (duk_pcall(ctx, 0) != 0)
  { // Execute the compiled code
    Serial.printf("%s: %s\n", filename.c_str(), duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    delete[] buffer;
    return -1;
  }
  // get the output if any
  if (duk_is_string(ctx, -1))
  {
    Serial.printf("%s: %s\n", filename.c_str(), duk_get_string(ctx, -1));
  }
  duk_pop(ctx);
  delete[] buffer;
  return 0;
}

int createVM(const String &filename)
{
  if (vmCount >= MAX_VMS)
  {
    Serial.println("Maximum VM count reached.");
    return -1;
  }
  if (!isEnoughMemoryAvailable(DEFAULT_VM_MEMORY))
  {
    Serial.println("Not enough memory to start VM");
    return -1;
  }
  // get the size of the file
  String fullPath = "/SPIFFS/" + filename;
  File file = SPIFFS.open(fullPath, "r");
  if (!file)
  {
    Serial.printf("Failed to open file: %s\n", fullPath.c_str());
    return -1;
  }
  unsigned long fileSize = file.size();
  file.close();

  int vmIndex = vmCount++;
  vms[vmIndex].filename = filename;
  vms[vmIndex].fullPath = fullPath;
  vms[vmIndex].running = false;
  vms[vmIndex].lastRunTime = 0;
  vms[vmIndex].memoryAllocated = 16384;
  vms[vmIndex].pinMutex = xSemaphoreCreateMutex();
  vms[vmIndex].needsRestart = false;
  vms[vmIndex].needsTermination = false;
  vms[vmIndex].core = vmIndex % 2;
  vms[vmIndex].messageQueue = xQueueCreate(10, sizeof(String));
  vms[vmIndex].lastFileCheckTime = 0;
  vms[vmIndex].fileSize = fileSize;
  vms[vmIndex].fileExists = true;
  vms[vmIndex].fileChanged = false;
  if (vms[vmIndex].messageQueue == NULL)
  {
    Serial.printf("Failed to create message queue for VM %d\n", vmIndex);
    vmCount--;
    return -1;
  }
  Serial.printf("VM created: %s (index: %d)\n", filename.c_str(), vmIndex);
  return vmIndex;
}
void startVM(int vmIndex)
{
  if (vmIndex < 0 || vmIndex >= vmCount || vms[vmIndex].running)
    return;

  int *taskVmIndex = new int;
  *taskVmIndex = vmIndex;

  xTaskCreatePinnedToCore(
      [](void *pvParameters)
      {
        int index = *(int *)pvParameters;
        VM &vm = vms[index];

        Serial.printf("VM %d: Memory available before allocation: %d bytes\n", index, ESP.getFreeHeap());
        Serial.printf("VM %d: Attempting to allocate %d bytes for Duktape heap\n", index, vm.memoryAllocated);

        void *heapMemory = malloc(vm.memoryAllocated);
        if (heapMemory == nullptr)
        {
          Serial.printf("VM %d: Memory allocation failed (needed %d bytes)\n", index, vm.memoryAllocated);
          goto cleanup;
        }

        Serial.printf("VM %d: Memory available after allocation: %d bytes\n", index, ESP.getFreeHeap());

        vm.ctx = duk_create_heap(
            [](void *udata, size_t size) -> void *
            {
              void *ptr = malloc(size);
              if (ptr == NULL)
              {
                Serial.printf("Duktape malloc failed for size %d\n", size);
              }
              return ptr;
            },
            [](void *udata, void *ptr, size_t size) -> void *
            {
              if (size == 0)
              {
                free(ptr);
                return NULL;
              }
              void *newPtr = realloc(ptr, size);
              if (newPtr == NULL)
              {
                Serial.printf("Duktape realloc failed for size %d\n", size);
              }
              return newPtr;
            },
            [](void *udata, void *ptr)
            { free(ptr); },
            NULL,
            [](void *udata, const char *msg)
            {
              Serial.printf("Duktape Fatal Error: %s\n", msg);
            });

        if (vm.ctx == NULL)
        {
          Serial.printf("VM %d: Error creating Duktape context for %s\n", index, vm.filename.c_str());
          free(heapMemory);
          goto cleanup;
        }

        registerDuktapeBindings(vm.ctx, index);
        Serial.printf("VM %d: Duktape bindings registered for %s\n", index, vm.filename.c_str());

        if (loadAndRunJS(vm.ctx, vm.filename) != 0)
        {
          Serial.printf("VM %d: Error loading and running file %s\n", index, vm.filename.c_str());
          goto cleanup;
        }

        vm.running = true;
        Serial.printf("VM %d started: %s (Core %d)\n", index, vm.filename.c_str(), vm.core);

        // Duktape event loop with a termination check within the loop
        while (vm.running)
        {
          if (vm.needsTermination)
          {
            Serial.printf("VM %d: Termination requested, breaking loop.\n", index);
            break; // Exit the loop if termination is requested
          }

          duk_peval_string(vm.ctx, ""); // Process Duktape events

          vm.lastRunTime = get_ms();
          String msg;
          while (xQueueReceive(vm.messageQueue, &msg, 0) == pdTRUE)
          {
            duk_push_string(vm.ctx, msg.c_str());
            if (duk_peval(vm.ctx) != 0)
            {
              Serial.printf("VM %d: Error processing message: %s\n", index, duk_safe_to_string(vm.ctx, -1));
              duk_pop(vm.ctx);
            }
            duk_pop(vm.ctx);
            vTaskDelay(1); // Allow other tasks to run
          }
          if (vm.fileChanged)
          {
            vm.fileChanged = false;
            if (loadAndRunJS(vm.ctx, vm.filename) != 0)
            {
              Serial.printf("VM %d: Error reloading file %s\n", index, vm.filename.c_str());
            }
          }

          vTaskDelay(1); // Small delay for responsiveness
        }

      cleanup:
        Serial.println("Cleanup started");
        if (vm.ctx)
        {
          Serial.println("Destroying Duktape heap");
          duk_destroy_heap(vm.ctx);
        }
        if (heapMemory)
        {
          Serial.println("Freeing heap memory");
          free(heapMemory);
        }
        vm.running = false;
        vTaskDelete(NULL);
        vQueueDelete(vm.messageQueue);
        delete (int *)pvParameters;
      },
      vms[vmIndex].filename.c_str(),
      VM_STACK_SIZE,
      taskVmIndex,
      1,
      &vms[vmIndex].taskHandle,
      vms[vmIndex].core);

  if (vms[vmIndex].taskHandle == NULL)
  {
    Serial.printf("Failed to start task for VM %d\n", vmIndex);
    vms[vmIndex].running = false;
    vms[vmIndex].needsTermination = false;
  }
}

// stop a VM
void stopVM(int vmIndex)
{
  if (vmIndex < 0 || vmIndex >= vmCount || !vms[vmIndex].running)
    return;

  Serial.printf("Stopping VM %d: %s\n", vmIndex, vms[vmIndex].filename.c_str());

  vms[vmIndex].needsTermination = true;

  // Destroy the Duktape context directly
  if (vms[vmIndex].ctx)
  {
    //kill any running tasks
    duk_destroy_heap(vms[vmIndex].ctx);
    vms[vmIndex].ctx = NULL; // Make sure to set the context to NULL
  }

  // Efficiently wait for the task to finish without blocking (still needed)
  int timeout = 5000; // 5 seconds timeout
  while (vms[vmIndex].running && timeout > 0)
  {
    vTaskDelay(10); // Yield while waiting
    timeout -= 10;
  }

  if (vms[vmIndex].running)
  {
    Serial.printf("VM %d did not stop gracefully, forcing termination\n", vmIndex);
    if (vms[vmIndex].taskHandle != NULL)
    {
      vTaskDelete(vms[vmIndex].taskHandle);
    }
  }

  // Reset VM state
  vms[vmIndex].running = false;
  vms[vmIndex].needsTermination = false;
  vms[vmIndex].taskHandle = NULL;

  if (vms[vmIndex].messageQueue != NULL)
  {
    vQueueDelete(vms[vmIndex].messageQueue);
    vms[vmIndex].messageQueue = NULL;
  }

  if (vms[vmIndex].pinMutex != NULL)
  {
    vSemaphoreDelete(vms[vmIndex].pinMutex);
    vms[vmIndex].pinMutex = NULL;
  }

  Serial.printf("VM %d stopped: %s\n", vmIndex, vms[vmIndex].filename.c_str());
}

// === File System Handling (SPIFFS) ===
bool isJSFile(const String &filename)
{
  return filename.length() > 4 && filename.substring(filename.length() - 3) == ".js";
}

void checkFileChanges(int vmIndex)
{
  VM &vm = vms[vmIndex];
  if (get_ms() - vm.lastFileCheckTime >= SPIFFS_CHECK_INTERVAL)
  {
    vm.lastFileCheckTime = get_ms();

    File file = SPIFFS.open(vm.fullPath, "r"); // Open with full path
    bool fileExistsNow;
    if (file)
    {
      file.close();
      fileExistsNow = true;
    }
    else
    {
      fileExistsNow = false;
    }

    if (!fileExistsNow && vm.fileExists)
    {
      Serial.printf("File removed: %s\n", vm.filename.c_str());
      // Only terminate if the VM is currently running
      if (vm.running)
      {
        vm.needsTermination = true;
      }
      vm.fileExists = false;
      return;
    }
    else if (fileExistsNow && !vm.fileExists)
    {
      Serial.printf("File added: %s\n", vm.filename.c_str());
      vm.needsRestart = true;
      vm.fileExists = true;
      return;
    }

    if (fileExistsNow)
    {
      File file = SPIFFS.open(vm.fullPath, "r"); // Open with full path
      if (!file)
      {
        Serial.printf("Error opening file: %s\n", vm.filename.c_str());
        return;
      }
      unsigned long fileSize = file.size();
      file.close();

      if (fileSize != vm.fileSize)
      {
        vm.fileChanged = true;
        Serial.printf("File changed: %s\n", vm.filename.c_str());
      }
      else
      {
        vm.fileChanged = false;
      }
      vm.fileSize = fileSize;
    }
  }
}

void scanSPIFFS()
{
  File root = SPIFFS.open("/"); // Open the SPIFFS root
  if (!root)
  {
    Serial.println("Failed to open SPIFFS root");
    return;
  }

  listFiles(root, 0);

  root.close();
}

void listFiles(File dir, int indent)
{
  File file = dir.openNextFile();
  while (file)
  {
    for (int i = 0; i < indent; i++)
      Serial.print("  ");
    if (file.isDirectory())
    {
      Serial.print(file.name());
      Serial.println("/");
      listFiles(file, indent + 1);
    }
    else
    {
      Serial.println(file.name());
      if (isJSFile(file.name()) && strncmp(file.name(), "/SPIFFS/", 8) == 0)
      {
        String filename = file.name() + 8; // Remove "/SPIFFS/" prefix by adding 8 to the pointer
        int vmIndex = createVM(filename);
        if (vmIndex != -1)
        {
          vms[vmIndex].lastFileCheckTime = get_ms();
          vms[vmIndex].fileExists = true;
          startVM(vmIndex);
        }
      }
    }
    file = dir.openNextFile();
  }
}

// === Duktape Bindings ===
duk_ret_t duk_digitalWrite(duk_context *ctx)
{
  int pin = duk_require_int(ctx, 0);
  int value = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);

  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS)
    return -1;

  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE)
  {
    if (xSemaphoreTake(pinMutexes[pin], portMAX_DELAY) == pdTRUE)
    {
      digitalWrite(pin, value);
      xSemaphoreGive(pinMutexes[pin]);
    }
    else
    {
      Serial.printf("VM %d: Error: Pin %d is already in use\n", vmIndex, pin);
    }
    xSemaphoreGive(vms[vmIndex].pinMutex);
  }
  else
  {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
  return 0;
}

duk_ret_t duk_digitalRead(duk_context *ctx)
{
  int pin = duk_require_int(ctx, 0);
  int vmIndex = duk_require_int(ctx, 1);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS)
    return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE)
  {
    duk_push_int(ctx, digitalRead(pin));
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 1;
  }
  else
  {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_analogRead(duk_context *ctx)
{
  int pin = duk_require_int(ctx, 0);
  int vmIndex = duk_require_int(ctx, 1);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS)
    return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE)
  {
    duk_push_int(ctx, analogRead(pin));
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 1;
  }
  else
  {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_analogWrite(duk_context *ctx)
{
  int pin = duk_require_int(ctx, 0);
  int value = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS)
    return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE)
  {
    analogWrite(pin, value);
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 0;
  }
  else
  {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_pinMode(duk_context *ctx)
{
  int pin = duk_require_int(ctx, 0);
  int mode = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS)
    return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE)
  {
    pinMode(pin, mode);
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 0;
  }
  else
  {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_delay(duk_context *ctx)
{
  int ms = duk_require_int(ctx, 0);
  delay(ms);
  return 0;
}

duk_ret_t duk_udpSend(duk_context *ctx)
{
  const char *ipStr = duk_require_string(ctx, 0);
  uint16_t port = duk_require_int(ctx, 1);
  const char *msg = duk_require_string(ctx, 2);
  int vmIndex = duk_require_int(ctx, 3);

  IPAddress ip;
  if (!ip.fromString(ipStr))
  {
    Serial.printf("VM %d: Invalid IP address\n", vmIndex);
    return -1;
  }

  udp.beginPacket(ip, port);
  udp.write((const uint8_t *)msg, strlen(msg));
  udp.endPacket();
  return 0;
}

duk_ret_t duk_udpReceive(duk_context *ctx)
{
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    char packetBuffer[MAX_MESSAGE_LENGTH + 1];
    int bytesRead = udp.read(packetBuffer, MAX_MESSAGE_LENGTH);
    packetBuffer[bytesRead] = '\0';
    duk_push_string(ctx, packetBuffer);
    return 1;
  }
  else
  {
    return 0;
  }
}

duk_ret_t duk_sendMessage(duk_context *ctx)
{
  int receiverID = duk_require_int(ctx, 0);
  const char *message = duk_require_string(ctx, 1);
  int senderID = duk_require_int(ctx, 2);

  if (receiverID < 0 || receiverID >= MAX_VMS || senderID < 0 || senderID >= MAX_VMS)
  {
    Serial.printf("Invalid receiver or sender ID\n", receiverID);
    return -1;
  }

  String msg = String(message);
  if (xQueueSend(vms[receiverID].messageQueue, &msg, portMAX_DELAY) != pdTRUE)
  {
    Serial.printf("VM %d: Error sending message to VM %d\n", senderID, receiverID);
    return -1;
  }
  return 0;
}

duk_ret_t duk_receiveMessage(duk_context *ctx)
{
  int vmIndex = duk_require_int(ctx, 0);

  if (vmIndex < 0 || vmIndex >= MAX_VMS)
  {
    Serial.printf("Invalid VM index\n");
    return -1;
  }

  String msg;
  if (xQueueReceive(vms[vmIndex].messageQueue, &msg, 0) == pdTRUE)
  {
    duk_push_string(ctx, msg.c_str());
    return 1;
  }
  else
  {
    return 0;
  }
}

duk_ret_t duk_print(duk_context *ctx)
{
  const char *str = duk_require_string(ctx, 0);
  Serial.println(str);
  return 0;
}

duk_ret_t duk_wait(duk_context *ctx)
{
  int ms = duk_require_int(ctx, 0);
  // Convert milliseconds to ticks
  TickType_t ticks = ms / portTICK_PERIOD_MS;
  vTaskDelay(ticks);
  return 0;
}

void registerDuktapeBindings(duk_context *ctx, int vmIndex)
{

  duk_push_c_function(ctx, duk_print, 1);
  duk_put_global_string(ctx, "print");
  duk_push_c_function(ctx, duk_digitalWrite, 3);
  duk_put_global_string(ctx, "digitalWrite");
  duk_push_c_function(ctx, duk_digitalRead, 2);
  duk_put_global_string(ctx, "digitalRead");
  duk_push_c_function(ctx, duk_analogRead, 2);
  duk_put_global_string(ctx, "analogRead");
  duk_push_c_function(ctx, duk_analogWrite, 3);
  duk_put_global_string(ctx, "analogWrite");
  duk_push_c_function(ctx, duk_pinMode, 3);
  duk_put_global_string(ctx, "pinMode");
  duk_push_c_function(ctx, duk_delay, 1);
  duk_put_global_string(ctx, "delay");
  duk_push_c_function(ctx, duk_udpSend, 4);
  duk_put_global_string(ctx, "udpSend");
  duk_push_c_function(ctx, duk_udpReceive, 0);
  duk_put_global_string(ctx, "udpReceive");
  duk_push_c_function(ctx, duk_sendMessage, 3);
  duk_put_global_string(ctx, "sendMessage");
  duk_push_c_function(ctx, duk_receiveMessage, 1);
  duk_put_global_string(ctx, "receiveMessage");
  duk_push_int(ctx, vmIndex);
  duk_put_global_string(ctx, "vmIndex");
  // Set __filename to the full path of the currently running script
  duk_push_string(ctx, ("/SPIFFS/" + vms[vmIndex].filename).c_str());
  duk_put_global_string(ctx, "__filename");
  // add the wait function
  duk_push_c_function(ctx, duk_wait, 1);
  duk_put_global_string(ctx, "wait");
}

// === Serial Communication and Debugging ===
void printVMStatus()
{
  Serial.println("=== VM Status ===");
  for (int i = 0; i < vmCount; i++)
  {
    Serial.printf("VM %d: %s, Running: %s, Core: %d, File Exists: %s\n",
                  i, vms[i].filename.c_str(), vms[i].running ? "true" : "false",
                  vms[i].core, vms[i].fileExists ? "true" : "false");
  }
  Serial.println("===============");
}

void handleSerialInput()
{
  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "status")
    {
      printVMStatus();
    }
    else if (command.startsWith("stop "))
    {
      int vmIndex = command.substring(5).toInt();
      stopVM(vmIndex);
    }
    else if (command.startsWith("start "))
    {
      int vmIndex = command.substring(6).toInt();
      startVM(vmIndex);
    }
    else if (command.startsWith("restart "))
    {
      int vmIndex = command.substring(8).toInt();
      // stop the VM if it's running
      if (vms[vmIndex].running)
      {
        stopVM(vmIndex);
      }
      // get the full path of the file
      startVM(vmIndex);
    }
    else if (command == "scan")
    {
      Serial.println("Starting scan!");
      scanSPIFFS();
    }
    else if (command.startsWith("create "))
    {
      String filename = command.substring(7);
      int vmIndex = createVM(filename);
      if (vmIndex != -1)
      {
        vms[vmIndex].lastFileCheckTime = get_ms();
        vms[vmIndex].fileExists = true;
        startVM(vmIndex);
      }
    }
    else if (command == "reboot")
    {
      ESP.restart();
    }
    else if (command.startsWith("print "))
    { // Print the file contents to the serial monitor
      String filename = command.substring(6);
      String fullPath = "/SPIFFS/" + filename;

      // Unmount and remount SPIFFS to ensure it's properly initialized
      SPIFFS.end();
      if (!SPIFFS.begin(true))
      {
        Serial.println("Failed to remount SPIFFS");
        return;
      }

      File file = SPIFFS.open(fullPath, "r");

      if (!file)
      {
        Serial.println("File not found");
      }
      else
      {
        Serial.printf("Printing file contents for: %s\n", fullPath.c_str());
        Serial.printf("File size: %d bytes\n", file.size());

        if (file.seek(0))
        { // Ensure the file pointer is at the beginning
          if (file.size() == 0)
          {
            Serial.println("File is empty");
          }
          else
          {
            Serial.println("File contents:");
            while (file.available())
            {
              Serial.write(file.read());
            }
          }
        }
        else
        {
          Serial.println("Failed to seek to the beginning of the file.");
        }
      }

      file.close();
    }
    else if (command == "help")
    {
      Serial.println("Commands:");
      Serial.println("  status: Print VM status");
      Serial.println("  stop <vmIndex>: Stop VM");
      Serial.println("  start <vmIndex>: Start VM");
      Serial.println("  restart <vmIndex>: Restart VM");
      Serial.println("  scan: Scan SPIFFS for JS files");
      Serial.println("  create <filename>: Create VM");
      Serial.println("  reboot: Reboot ESP32");
      Serial.println("  help: Print this help text");
      Serial.println("  print <filename>: Print file contents");
    }

    // ... (add more serial commands as needed) ...
  }
}

// === Setup and Loop ===
void setup()
{
  Serial.begin(115200);
  Serial.println("Starting ESP32 JavaScript VM Platform (Duktape)...");

  // Initialize pin mutexes
  for (int i = 0; i < 40; i++)
  {
    pinMutexes[i] = xSemaphoreCreateMutex();
    if (pinMutexes[i] == NULL)
    {
      Serial.printf("Failed to create mutex for pin %d\n", i);
    }
  }

  // Initialize Networking (WiFi)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    Serial.println("SPIFFS opened!");
    // add FTP user
    ftp.addUser(FTP_USER, FTP_PASSWORD);
    // Add filesystem to FTP
    ftp.addFilesystem("SPIFFS", &SPIFFS);
    // Start the FTP server
    ftp.begin();
    Serial.println("FTP Server started");

    // Get all information of your SPIFFS and send it to the Serial Monitor
    Serial.println("=== SPIFFS Information ===");
    Serial.print("Total space: ");
    Serial.print(SPIFFS.totalBytes());
    Serial.print(" bytes, ");
    Serial.print(SPIFFS.totalBytes() / 1024);
    Serial.println(" KB");
    Serial.print("Used space: ");
    Serial.print(SPIFFS.usedBytes());
    Serial.print(" bytes, ");
    Serial.print(SPIFFS.usedBytes() / 1024);
    Serial.println(" KB");
    Serial.println();
    // Print all files in SPIFFS
    Serial.println("=== Files in SPIFFS ===");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
      if (file.isDirectory())
      {
        Serial.print("  DIR : ");
      }
      else
      {
        Serial.print("  FILE: ");
      }
      Serial.println(file.name());
      file = root.openNextFile();
    }
    root.close();
    Serial.println("===============");
    // create a simple print function for the Duktape VMs and write it to the SPIFFS directory if it doesn't exist /SPIFFS/print.js
    if (!SPIFFS.exists("/SPIFFS/print.js"))
    {
      File printFile = SPIFFS.open("/SPIFFS/print.js", "w");
      printFile.print("print('hello from javascript!');");
      printFile.close();
    }
    if (!SPIFFS.exists("/SPIFFS/loop.js"))
    {
      File loopFile = SPIFFS.open("/SPIFFS/loop.js", "w");
      loopFile.print("while (true) {print('hello again!'); wait(5000);}");
      loopFile.close();
    }
  }
  udp.begin(UDP_PORT);

  // Scan SPIFFS for .js files and start VMs
  scanSPIFFS();
}

void loop()
{
  handleSerialInput();
  ftp.handle(); // Handle FTP client requests (If you still have FTP, remove it for TinyUSB)

  // Monitor and reschedule VMs
  for (int i = 0; i < vmCount; i++)
  {
    if (vms[i].running)
    {
      checkFileChanges(i);
      if (vms[i].needsTermination)
      {
        stopVM(i);
      }
      else if (vms[i].needsRestart || (get_ms() - vms[i].lastRunTime) > 1000)
      {
        vms[i].needsRestart = true;
      }
    }
  }

  for (int i = 0; i < vmCount; i++)
  {
    if (vms[i].running && vms[i].needsRestart)
    {
      vms[i].needsRestart = false;
      startVM(i);
    }
  }

  vTaskDelay(10); // Use vTaskDelay in the main loop for responsiveness
}