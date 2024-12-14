// file_system.cpp
#include "file_system.h"
#include "vm_manager.h"

// === File System Handling (SPIFFS) === 
bool isJSFile(const String& filename) {
  return filename.length() > 4 && filename.substring(filename.length() - 3) == ".js";
}

void checkFileChanges(int vmIndex) {
  VM& vm = vms[vmIndex];
  if (get_ms() - vm.lastFileCheckTime >= SPIFFS_CHECK_INTERVAL) {
    vm.lastFileCheckTime = get_ms();

    File file = SPIFFS.open(vm.fullPath, "r"); // Open with full path
    bool fileExistsNow;
    if (file) {
      file.close();
      fileExistsNow = true;
    } else {
      fileExistsNow = false;
    }

    if (!fileExistsNow && vm.fileExists) {
      Serial.printf("File removed: %s\n", vm.filename.c_str());
      // Only terminate if the VM is currently running
      if (vm.running) {
        vm.needsTermination = true; 
      }
      vm.fileExists = false;
      return;
    } else if (fileExistsNow && !vm.fileExists) {
      Serial.printf("File added: %s\n", vm.filename.c_str());
      vm.needsRestart = true;
      vm.fileExists = true;
      return;
    }

    if (fileExistsNow) {
      File file = SPIFFS.open(vm.fullPath, "r"); // Open with full path
      if (!file) {
        Serial.printf("Error opening file: %s\n", vm.filename.c_str());
        return;
      }
      unsigned long fileSize = file.size();
      file.close();

      if (fileSize != vm.fileSize) {
        vm.fileChanged = true;
        Serial.printf("File changed: %s\n", vm.filename.c_str());
      } else {
        vm.fileChanged = false;
      }
      vm.fileSize = fileSize;
    }
  }
}

void scanSPIFFSAndStartVMs() {
  File root = SPIFFS.open("/"); 
  if (!root) {
    Serial.println("Failed to open SPIFFS root");
    return;
  }

  listFiles(root, 0);

  root.close();
}

void listFiles(File dir, int indent) {
  File file = dir.openNextFile();
  while (file) {
    for (int i = 0; i < indent; i++) Serial.print(" ");
    if (file.isDirectory()) {
      Serial.print(file.name());
      Serial.println("/");
      listFiles(file, indent + 1);
    } else {
      Serial.println(file.name());
      if (isJSFile(file.name()) && strncmp(file.name(), "/SPIFFS/", 8) == 0) {
        String filename = file.name() + 8; 
        int vmIndex = createVM(filename);
        if (vmIndex != -1) {
          vms[vmIndex].lastFileCheckTime = get_ms();
          vms[vmIndex].fileExists = true;
          startVM(vmIndex);
        }
      }
    }
    file = dir.openNextFile();
  }
}

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  } else {
    Serial.println("SPIFFS opened!");

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
    while (file) {
      if (file.isDirectory()) {
        Serial.print("  DIR : ");
      } else {
        Serial.print("  FILE: ");
      }
      Serial.println(file.name());
      file = root.openNextFile();
    }
    root.close();
    Serial.println("===============");

    // Create a simple print function for the Duktape VMs and write it to the SPIFFS directory
    // if it doesn't exist /SPIFFS/print.js
    if (!SPIFFS.exists("/SPIFFS/print.js")) {
      File printFile = SPIFFS.open("/SPIFFS/print.js", "w");
      printFile.print("print('hello from javascript!');");
      printFile.close();
    }
    if (!SPIFFS.exists("/SPIFFS/loop.js")) {
      File loopFile = SPIFFS.open("/SPIFFS/loop.js", "w");
      loopFile.print("while (true) {print('hello again!'); wait(5000);}");
      loopFile.close();
    }
  }
}