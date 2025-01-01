#include "include/file_system.h"
#include "include/vm_manager.h"
#include <FFat.h>

// === File System Handling (FFat) === 
bool isJSFile(const char* filename) {
  String fname = String(filename);
  return fname.endsWith(".js");
}

bool initFS() {
  // Format FFat if mounting fails
  if (!FFat.begin(false)) {
    Serial.println("FFat Mount Failed. Attempting to format...");
    if (!FFat.format()) {
      Serial.println("FFat Format Failed");
      return false;
    }
    if (!FFat.begin(true)) {
      Serial.println("FFat Mount Failed after formatting");
      return false;
    }
  }

  // Create example files if they don't exist
  if (!FFat.exists("/print.js")) {
    File file = FFat.open("/print.js", "w");
    if (file) {
      file.println("function print(str) { _print(str); }");
      file.close();
      Serial.println("Created /print.js");
    }
  }

  if (!FFat.exists("/loop.js")) {
    File file = FFat.open("/loop.js", "w");
    if (file) {
      file.println("// This is the main loop that runs continuously");
      file.println("while (true) {print('hello again!'); wait(5000);}");
      file.close();
      Serial.println("Created /loop.js");
    }
  }

  return true;
}

void checkFileChanges(int vmIndex) {
  if (vmIndex >= 0 && vmIndex < MAX_VMS && vms[vmIndex].running) {
    auto& vm = vms[vmIndex];
    if (millis() - vm.lastFileCheckTime >= FS_CHECK_INTERVAL) {
      vm.lastFileCheckTime = millis();
      
      File file = FFat.open(vm.fullPath, "r"); 
      if (!file) {
        return;
      }

      size_t newSize = file.size();
      time_t newTime = file.getLastWrite();
      file.close();

      // Only reload if both size and modification time have changed
      if (vm.fileSize > 0 && vm.lastModified > 0 && 
          (newSize != vm.fileSize && newTime != vm.lastModified)) {
        
        // Stop the current VM first
        stopVM(vmIndex);
        
        // Reopen file to read content
        file = FFat.open(vm.fullPath, "r"); 
        if (!file) {
          return;
        }

        String content = file.readString();
        file.close();

        // Update file info before creating new VM
        String oldFilename = vm.filename;
        String oldFullPath = vm.fullPath;
        
        // Create new VM
        createVM(oldFilename, content.c_str(), oldFullPath);
      } else {
        // Update file info without reloading
        vm.fileSize = newSize;
        vm.lastModified = newTime;
      }
    }
  }
}

void listFiles(File dir, int indent) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    
    for (int i = 0; i < indent; i++) {
      Serial.print("  ");
    }
    
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listFiles(entry, indent + 1);
    } else {
      Serial.print("  ");
      Serial.print(entry.size(), DEC);
      Serial.println(" bytes");
    }
    entry.close();
  }
}

void initFSInfo() {
  if (!FFat.begin(true)) {
    Serial.println("An Error has occurred while mounting filesystem");
    return;
  } else {
    Serial.println("Filesystem opened!");
    
    // Get all information of your filesystem
    Serial.println("=== Filesystem Information ===");
    Serial.print("Total space: ");
    Serial.print(FFat.totalBytes());
    Serial.print(" bytes (");
    Serial.print(FFat.totalBytes() / 1024);
    Serial.println("KB)");
    Serial.print("Used space: ");
    Serial.print(FFat.usedBytes());
    Serial.print(" bytes (");
    Serial.print(FFat.usedBytes() / 1024);
    Serial.println("KB)");
    Serial.println();

    // Print all files
    Serial.println("=== Files in Filesystem ===");
    File root = FFat.open("/");
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }
    listFiles(root, 0);
    root.close();

    // Create initial files if they don't exist
    if (!FFat.exists("/print.js")) {
      File printFile = FFat.open("/print.js", "w");
      if (printFile) {
        printFile.println("function print(str) { _print(str); }");
        printFile.close();
        Serial.println("Created /print.js");
      }
    }

    if (!FFat.exists("/loop.js")) {
      File loopFile = FFat.open("/loop.js", "w");
      if (loopFile) {
        loopFile.println("// This is the main loop that runs continuously");
        loopFile.print("while (true) {print('hello again!'); wait(5000);}");
        loopFile.close();
        Serial.println("Created /loop.js");
      }
    }
  }
}