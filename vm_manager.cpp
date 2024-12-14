// vm_manager.cpp
#include "vm_manager.h"
#include "networking.h" // For UDP 
#include "file_system.h" // For SPIFFS

// Initialize these here (declared as extern in the header)
VM vms[MAX_VMS];
int vmCount = 0;
SemaphoreHandle_t pinMutexes[40]; 
bool pinInUse[40] = {false}; // Initialize pinInUse array

// Helper function to get current time in milliseconds.
unsigned long get_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// === Memory Management ===
bool isEnoughMemoryAvailable(size_t memoryNeeded) {
  return ESP.getFreeHeap() > memoryNeeded + 16384; 
}

// === VM Management ===
duk_ret_t loadAndRunJS(duk_context *ctx, const String& filename) {
  File file = SPIFFS.open("/SPIFFS/" + filename, "r");
  if (!file) {
    Serial.printf("Error opening file in /SPIFFS: %s\n", filename.c_str());
    return -1;
  }

  size_t fileSize = file.size();
  if (fileSize > MAX_FILE_SIZE) {
    Serial.printf("File %s exceeds maximum size (%d bytes)\n", filename.c_str(), MAX_FILE_SIZE);
    file.close();
    return -1;
  }
  char *buffer = new char[fileSize + 1];
  if (buffer == nullptr) {
    Serial.println("Memory allocation failed!");
    file.close();
    return -1;
  }
  file.readBytes(buffer, fileSize);
  buffer[fileSize] = '\0';
  file.close();

  duk_push_string(ctx, buffer);
  if (duk_pcompile_string(ctx, DUK_COMPILE_EVAL, buffer) != 0) { 
    Serial.printf("%s: %s\n", filename.c_str(), duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    delete[] buffer;
    return -1;
  }
  if (duk_pcall(ctx, 0) != 0) { 
    Serial.printf("%s: %s\n", filename.c_str(), duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    delete[] buffer;
    return -1;
  }
  if (duk_is_string(ctx, -1)) {
    Serial.printf("%s: %s\n", filename.c_str(), duk_get_string(ctx, -1));
  }
  duk_pop(ctx);
  delete[] buffer;
  return 0;
}


int createVM(const String& filename) {
  if (vmCount >= MAX_VMS) {
    Serial.println("Maximum VM count reached.");
    return -1;
  }
  if (!isEnoughMemoryAvailable(DEFAULT_VM_MEMORY)) {
    Serial.println("Not enough memory to start VM");
    return -1;
  }

  String fullPath = "/SPIFFS/" + filename;
  File file = SPIFFS.open(fullPath, "r");
  if (!file) {
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

  if (vms[vmIndex].messageQueue == NULL) {
    Serial.printf("Failed to create message queue for VM %d\n", vmIndex);
    vmCount--;
    return -1;
  }
  
  Serial.printf("VM created: %s (index: %d)\n", filename.c_str(), vmIndex);
  return vmIndex;
}


void startVM(int vmIndex) {
  if (vmIndex < 0 || vmIndex >= vmCount || vms[vmIndex].running) return;

  int *taskVmIndex = new int;
  *taskVmIndex = vmIndex;

  xTaskCreatePinnedToCore(
    [](void *pvParameters) {
      int index = *(int *)pvParameters;
      VM &vm = vms[index];

      Serial.printf("VM %d: Memory available before allocation: %d bytes\n", index, ESP.getFreeHeap());
      Serial.printf("VM %d: Attempting to allocate %d bytes for Duktape heap\n", index, vm.memoryAllocated);

      void *heapMemory = malloc(vm.memoryAllocated);
      if (heapMemory == nullptr) {
        Serial.printf("VM %d: Memory allocation failed (needed %d bytes)\n", index, vm.memoryAllocated);
        goto cleanup;
      }

      Serial.printf("VM %d: Memory available after allocation: %d bytes\n", index, ESP.getFreeHeap());

      vm.ctx = duk_create_heap(
          [](void *udata, size_t size) -> void * {
            void *ptr = malloc(size);
            if (ptr == NULL) {
              Serial.printf("Duktape malloc failed for size %d\n", size);
            }
            return ptr;
          },
          [](void *udata, void *ptr, size_t size) -> void * {
            if (size == 0) {
              free(ptr);
              return NULL;
            }
            void *newPtr = realloc(ptr, size);
            if (newPtr == NULL) {
              Serial.printf("Duktape realloc failed for size %d\n", size);
            }
            return newPtr;
          },
          [](void *udata, void *ptr) { free(ptr); },
          NULL,
          [](void *udata, const char *msg) {
            Serial.printf("Duktape Fatal Error: %s\n", msg);
          });

      if (vm.ctx == NULL) {
        Serial.printf("VM %d: Error creating Duktape context for %s\n", index, vm.filename.c_str());
        free(heapMemory);
        goto cleanup;
      }

      registerDuktapeBindings(vm.ctx, index);
      Serial.printf("VM %d: Duktape bindings registered for %s\n", index, vm.filename.c_str());

      if (loadAndRunJS(vm.ctx, vm.filename) != 0) {
        Serial.printf("VM %d: Error loading and running file %s\n", index, vm.filename.c_str());
        goto cleanup;
      }

      vm.running = true;
      Serial.printf("VM %d started: %s (Core %d)\n", index, vm.filename.c_str(), vm.core);

      while (vm.running) {
        if (vm.needsTermination) {
          Serial.printf("VM %d: Termination requested, breaking loop.\n", index);
          break; 
        }

        duk_peval_string(vm.ctx, ""); 

        vm.lastRunTime = get_ms();
        String msg;
        while (xQueueReceive(vm.messageQueue, &msg, 0) == pdTRUE) {
          duk_push_string(vm.ctx, msg.c_str());
          if (duk_peval(vm.ctx) != 0) {
            Serial.printf("VM %d: Error processing message: %s\n", index, duk_safe_to_string(vm.ctx, -1)); // Use vm.ctx here
            duk_pop(vm.ctx);
          }
          duk_pop(vm.ctx);
          vTaskDelay(1); 
        }

        if (vm.fileChanged) {
          vm.fileChanged = false;
          if (loadAndRunJS(vm.ctx, vm.filename) != 0) {
            Serial.printf("VM %d: Error reloading file %s\n", index, vm.filename.c_str());
          }
        }

        vTaskDelay(1); 
      }

    cleanup:
      Serial.println("Cleanup started");
      if (vm.ctx) {
        Serial.println("Destroying Duktape heap");
        duk_destroy_heap(vm.ctx);
      }
      if (heapMemory) {
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

  if (vms[vmIndex].taskHandle == NULL) {
    Serial.printf("Failed to start task for VM %d\n", vmIndex);
    vms[vmIndex].running = false;
    vms[vmIndex].needsTermination = false;
  }
}


void stopVM(int vmIndex) {
  if (vmIndex < 0 || vmIndex >= vmCount || !vms[vmIndex].running) return;

  Serial.printf("Stopping VM %d: %s\n", vmIndex, vms[vmIndex].filename.c_str());

  vms[vmIndex].needsTermination = true;

  if (vms[vmIndex].ctx) { 
    duk_destroy_heap(vms[vmIndex].ctx);
    vms[vmIndex].ctx = NULL; 
  }

  int timeout = 5000; 
  while (vms[vmIndex].running && timeout > 0) {
    vTaskDelay(10); 
    timeout -= 10;
  }

  if (vms[vmIndex].running) {
    Serial.printf("VM %d did not stop gracefully, forcing termination\n", vmIndex);
    if (vms[vmIndex].taskHandle != NULL) {
      vTaskDelete(vms[vmIndex].taskHandle);
    }
  }

  vms[vmIndex].running = false;
  vms[vmIndex].needsTermination = false;
  vms[vmIndex].taskHandle = NULL;

  if (vms[vmIndex].messageQueue != NULL) {
    vQueueDelete(vms[vmIndex].messageQueue);
    vms[vmIndex].messageQueue = NULL;
  }

  if (vms[vmIndex].pinMutex != NULL) {
    vSemaphoreDelete(vms[vmIndex].pinMutex);
    vms[vmIndex].pinMutex = NULL;
  }

  Serial.printf("VM %d stopped: %s\n", vmIndex, vms[vmIndex].filename.c_str());
}

// === Duktape Bindings === 
duk_ret_t duk_digitalWrite(duk_context *ctx) {
  int pin = duk_require_int(ctx, 0);
  int value = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);

  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS) return -1;

  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
    if (xSemaphoreTake(pinMutexes[pin], portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, value);
      xSemaphoreGive(pinMutexes[pin]);
    } else {
      Serial.printf("VM %d: Error: Pin %d is already in use\n", vmIndex, pin);
    }
    xSemaphoreGive(vms[vmIndex].pinMutex);
  } else {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
  return 0;
}


duk_ret_t duk_digitalRead(duk_context *ctx) {
  int pin = duk_require_int(ctx, 0);
  int vmIndex = duk_require_int(ctx, 1);

  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS) return -1;

  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
    duk_push_int(ctx, digitalRead(pin));
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 1; 
  } else {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_analogRead(duk_context *ctx) {
  int pin = duk_require_int(ctx, 0);
  int vmIndex = duk_require_int(ctx, 1);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS) return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
    duk_push_int(ctx, analogRead(pin));
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 1;
  } else {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_analogWrite(duk_context *ctx) {
  int pin = duk_require_int(ctx, 0);
  int value = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS) return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
    analogWrite(pin, value);
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 0;
  } else {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_pinMode(duk_context *ctx) {
  int pin = duk_require_int(ctx, 0);
  int mode = duk_require_int(ctx, 1);
  int vmIndex = duk_require_int(ctx, 2);
  if (pin < 0 || pin >= 40 || vmIndex < 0 || vmIndex >= MAX_VMS) return -1;
  if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
    pinMode(pin, mode);
    xSemaphoreGive(vms[vmIndex].pinMutex);
    return 0;
  } else {
    Serial.printf("VM %d: Error acquiring per-vm mutex\n", vmIndex);
    return -1;
  }
}

duk_ret_t duk_delay(duk_context *ctx) {
  int ms = duk_require_int(ctx, 0);
  delay(ms);
  return 0;
}

duk_ret_t duk_udpSend(duk_context *ctx) {
  const char *ipStr = duk_require_string(ctx, 0);
  uint16_t port = duk_require_int(ctx, 1);
  const char *msg = duk_require_string(ctx, 2);
  int vmIndex = duk_require_int(ctx, 3);

  IPAddress ip;
  if (!ip.fromString(ipStr)) {
    Serial.printf("VM %d: Invalid IP address\n", vmIndex);
    return -1;
  }

  udp.beginPacket(ip, port);
  udp.write((const uint8_t *)msg, strlen(msg));
  udp.endPacket();
  return 0;
}

duk_ret_t duk_udpReceive(duk_context *ctx) {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[MAX_MESSAGE_LENGTH + 1];
    int bytesRead = udp.read(packetBuffer, MAX_MESSAGE_LENGTH);
    packetBuffer[bytesRead] = '\0';
    duk_push_string(ctx, packetBuffer);
    return 1; 
  } else {
    return 0; 
  }
}

duk_ret_t duk_sendMessage(duk_context *ctx) {
  int receiverID = duk_require_int(ctx, 0);
  const char *message = duk_require_string(ctx, 1);
  int senderID = duk_require_int(ctx, 2);

  if (receiverID < 0 || receiverID >= MAX_VMS || senderID < 0 || senderID >= MAX_VMS) {
    Serial.printf("Invalid receiver or sender ID\n", receiverID);
    return -1;
  }

  String msg = String(message);
  if (xQueueSend(vms[receiverID].messageQueue, &msg, portMAX_DELAY) != pdTRUE) {
    Serial.printf("VM %d: Error sending message to VM %d\n", senderID, receiverID);
    return -1;
  }
  return 0;
}

duk_ret_t duk_receiveMessage(duk_context *ctx) {
  int vmIndex = duk_require_int(ctx, 0);

  if (vmIndex < 0 || vmIndex >= MAX_VMS) {
    Serial.printf("Invalid VM index\n");
    return -1;
  }

  String msg;
  if (xQueueReceive(vms[vmIndex].messageQueue, &msg, 0) == pdTRUE) {
    duk_push_string(ctx, msg.c_str());
    return 1; 
  } else {
    return 0;
  }
}

duk_ret_t duk_print(duk_context *ctx) {
  const char *str = duk_require_string(ctx, 0);
  Serial.println(str);
  return 0;
}

duk_ret_t duk_wait(duk_context *ctx) {
  int ms = duk_require_int(ctx, 0); 
  TickType_t ticks = ms / portTICK_PERIOD_MS;
  vTaskDelay(ticks);
  return 0;
}

void registerDuktapeBindings(duk_context *ctx, int vmIndex) {

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

  duk_push_string(ctx, ("/SPIFFS/" + vms[vmIndex].filename).c_str());
  duk_put_global_string(ctx, "__filename"); 

  duk_push_c_function(ctx, duk_wait, 1);
  duk_put_global_string(ctx, "wait");
}

void monitorAndRescheduleVMs() {
  for (int i = 0; i < vmCount; i++) {
    if (vms[i].running) {
      checkFileChanges(i); // From file_system.cpp
      if (vms[i].needsTermination) {
        stopVM(i);
      } else if (vms[i].needsRestart || (get_ms() - vms[i].lastRunTime) > 1000) {
        vms[i].needsRestart = true; // Mark for restart in the next loop iteration
      }
    }
  }

  // Restart VMs that need restarting 
  for (int i = 0; i < vmCount; i++) {
    if (vms[i].needsRestart) {
      vms[i].needsRestart = false;
      startVM(i);
    }
  }
}