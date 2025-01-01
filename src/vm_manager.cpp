// vm_manager.cpp
#include "include/vm_manager.h"
#include "include/networking.h" // For UDP 
#include "include/file_system.h" // For SPIFFS
#include "include/duktape_bindings.h"
#include <FFat.h>

// Initialize these here (declared as extern in the header)
VM vms[MAX_VMS];
int vmCount = 0;
SemaphoreHandle_t pinMutexes[40]; 
bool pinInUse[40] = {false}; // Initialize pinInUse array

// Helper function to get current time in milliseconds.
unsigned long get_ms() {
  return millis();
}

// === Memory Management ===
bool isEnoughMemoryAvailable(size_t memoryNeeded) {
  return ESP.getFreeHeap() > memoryNeeded + 16384; 
}

// === VM Management ===

// Interrupt handler for VM execution
duk_ret_t vm_interrupt_handler(duk_context* ctx) {
  int vmIndex = -1;
  
  // Find which VM this context belongs to
  for (int i = 0; i < MAX_VMS; i++) {
    if (vms[i].ctx == ctx) {
      vmIndex = i;
      break;
    }
  }
  
  if (vmIndex >= 0) {
    if (vms[vmIndex].needsTermination || vms[vmIndex].forceTerminate) {
      return DUK_ERR_ERROR;
    }
  }
  return DUK_EXEC_SUCCESS;
}

int createVM(const String& filename, const char* content, const String& fullPath) {
  int vmIndex = findFreeVMSlot();
  if (vmIndex < 0) {
    Serial.println("No free VM slots");
    return -1;
  }

  // Initialize VM struct
  vms[vmIndex] = VM();
  vms[vmIndex].filename = filename;
  vms[vmIndex].fullPath = fullPath;
  vms[vmIndex].running = false;
  vms[vmIndex].needsTermination = false;
  vms[vmIndex].forceTerminate = false;
  vms[vmIndex].lastFileCheckTime = millis();
  vms[vmIndex].lastRunTime = millis();
  vms[vmIndex].memoryAllocated = 8192;

  // Create message queue
  vms[vmIndex].messageQueue = xQueueCreate(10, MAX_MESSAGE_LENGTH);
  if (!vms[vmIndex].messageQueue) {
    Serial.println("Failed to create message queue");
    return -1;
  }

  // Create Duktape context
  vms[vmIndex].ctx = duk_create_heap_default();
  if (!vms[vmIndex].ctx) {
    Serial.println("Failed to create JS context");
    vQueueDelete(vms[vmIndex].messageQueue);
    return -1;
  }

  // Initialize mutex
  vms[vmIndex].pinMutex = xSemaphoreCreateMutex();
  if (!vms[vmIndex].pinMutex) {
    Serial.println("Failed to create pin mutex");
    vQueueDelete(vms[vmIndex].messageQueue);
    duk_destroy_heap(vms[vmIndex].ctx);
    return -1;
  }

  // Register built-in functions
  registerDuktapeBindings(vms[vmIndex].ctx, vmIndex);

  // Load and compile the JavaScript code
  String wrappedCode = "(function() {\n";
  wrappedCode += "try {\n";
  wrappedCode += String(content);
  wrappedCode += "\n} catch(e) { print('Runtime error: ' + e.toString()); }\n";
  wrappedCode += "})";
  
  duk_push_string(vms[vmIndex].ctx, wrappedCode.c_str());
  if (duk_peval(vms[vmIndex].ctx) != 0) {
    Serial.printf("Failed to compile %s: %s\n", 
      filename.c_str(), 
      duk_safe_to_string(vms[vmIndex].ctx, -1)
    );
    duk_pop(vms[vmIndex].ctx);
    destroyVM(vmIndex);
    return -1;
  }

  // Store the compiled function
  duk_put_global_string(vms[vmIndex].ctx, "\xFF\xFFvm_func");

  // Start the VM task
  if (startVM(vmIndex) != 0) {
    Serial.println("Failed to start VM task");
    destroyVM(vmIndex);
    return -1;
  }

  return vmIndex;
}

void vmTask(void* parameter) {
  int vmIndex = *((int*)parameter);
  vPortFree(parameter);
  
  while (vms[vmIndex].running) {
    if (!vms[vmIndex].needsTermination) {
      executeVM(vmIndex);
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      break;
    }
  }

  vms[vmIndex].running = false;
  vTaskDelete(NULL);
}

void executeVM(int vmIndex) {
  if (vmIndex >= 0 && vmIndex < MAX_VMS && vms[vmIndex].running && vms[vmIndex].ctx) {
    vms[vmIndex].lastRunTime = millis();
    
    if (!duk_get_global_string(vms[vmIndex].ctx, "\xFF\xFFvm_func")) {
      vms[vmIndex].needsTermination = true;
      return;
    }
    
    if (duk_pcall(vms[vmIndex].ctx, 0) != 0) {
      const char* error = duk_safe_to_string(vms[vmIndex].ctx, -1);
      Serial.printf("Runtime error in %s: %s\n",
        vms[vmIndex].filename.c_str(),
        error
      );
      vms[vmIndex].needsTermination = true;
    }
    duk_pop(vms[vmIndex].ctx);
  }
}

int startVM(int vmIndex) {
  if (vmIndex < 0 || vmIndex >= MAX_VMS) {
    return -1;
  }

  if (vms[vmIndex].running) {
    return -1;
  }

  vms[vmIndex].running = true;
  vms[vmIndex].needsTermination = false;
  vms[vmIndex].forceTerminate = false;

  int* taskParam = (int*)pvPortMalloc(sizeof(int));
  if (!taskParam) {
    vms[vmIndex].running = false;
    return -1;
  }
  *taskParam = vmIndex;

  BaseType_t result = xTaskCreatePinnedToCore(
    vmTask,
    ("VM_" + String(vmIndex)).c_str(),
    8192,
    (void*)taskParam,
    1,
    &vms[vmIndex].taskHandle,
    1
  );

  if (result != pdPASS) {
    vPortFree(taskParam);
    vms[vmIndex].running = false;
    return -1;
  }

  return 0;
}

int findFreeVMSlot() {
  for (int i = 0; i < MAX_VMS; i++) {
    if (!vms[i].running) {
      return i;
    }
  }
  return -1;
}

void destroyVM(int vmIndex) {
  if (vmIndex >= 0 && vmIndex < MAX_VMS && vms[vmIndex].running) {
    stopVM(vmIndex);
    if (vms[vmIndex].ctx) {
      duk_destroy_heap(vms[vmIndex].ctx);
      vms[vmIndex].ctx = nullptr;
    }
  }
}

void stopVM(int vmIndex) {
  if (vmIndex >= 0 && vmIndex < MAX_VMS && vms[vmIndex].running) {
    // Signal the task to stop
    vms[vmIndex].needsTermination = true;
    
    // Wait for task to finish
    int timeout = 100; // 1 second timeout
    while (vms[vmIndex].running && timeout > 0) {
      delay(10);
      timeout--;
    }
    
    // Force kill if still running
    if (vms[vmIndex].running) {
      vms[vmIndex].forceTerminate = true;
      if (vms[vmIndex].taskHandle) {
        vTaskDelete(vms[vmIndex].taskHandle);
      }
    }
    
    // Clean up resources
    if (vms[vmIndex].pinMutex) {
      vSemaphoreDelete(vms[vmIndex].pinMutex);
      vms[vmIndex].pinMutex = nullptr;
    }

    if (vms[vmIndex].messageQueue) {
      vQueueDelete(vms[vmIndex].messageQueue);
      vms[vmIndex].messageQueue = nullptr;
    }
    
    vms[vmIndex].running = false;
    vms[vmIndex].needsTermination = false;
    vms[vmIndex].forceTerminate = false;
    vms[vmIndex].taskHandle = nullptr;
    
    Serial.printf("Stopped VM %d\n", vmIndex);
  }
}

void monitorAndRescheduleVMs() {
  for (int i = 0; i < MAX_VMS; i++) {
    if (vms[i].running) {
      checkFileChanges(i);
    }
  }
}