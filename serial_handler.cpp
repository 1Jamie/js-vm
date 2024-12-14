// serial_handler.cpp
#include "serial_handler.h"
#include "vm_manager.h"
#include "file_system.h"

// === Serial Communication and Debugging === 
void printVMStatus() {
  Serial.println("=== VM Status ===");
  for (int i = 0; i < vmCount; i++) {
    Serial.printf("VM %d: %s, Running: %s, Core: %d, File Exists: %s\n", 
                  i, vms[i].filename.c_str(), vms[i].running ? "true" : "false", 
                  vms[i].core, vms[i].fileExists ? "true" : "false");
  }
  Serial.println("===============");
}

void handleSerialInput() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "status") {
      printVMStatus();
    } else if (command.startsWith("stop ")) {
      int vmIndex = command.substring(5).toInt();
      stopVM(vmIndex);
    } else if (command.startsWith("start ")) {
      int vmIndex = command.substring(6).toInt();
      startVM(vmIndex);
    } else if (command.startsWith("restart ")) {
      int vmIndex = command.substring(8).toInt();
      if (vms[vmIndex].running) {
        stopVM(vmIndex);
      }
      startVM(vmIndex);
    } else if (command == "scan") {
      Serial.println("Starting scan!");
      scanSPIFFSAndStartVMs();
    } else if (command.startsWith("create ")) {
      String filename = command.substring(7);
      int vmIndex = createVM(filename);
      if (vmIndex != -1) {
        vms[vmIndex].lastFileCheckTime = get_ms();
        vms[vmIndex].fileExists = true;
        startVM(vmIndex);
      }
    } else if (command == "reboot") {
      ESP.restart();
    } else if (command.startsWith("print ")) { 
      String filename = command.substring(6);
      String fullPath = "/SPIFFS/" + filename;

      SPIFFS.end(); 
      if (!SPIFFS.begin(true)) {
        Serial.println("Failed to remount SPIFFS");
        return;
      }

      File file = SPIFFS.open(fullPath, "r");

      if (!file) {
        Serial.println("File not found");
      } else {
        Serial.printf("Printing file contents for: %s\n", fullPath.c_str());
        Serial.printf("File size: %d bytes\n", file.size());

        if (file.seek(0)) { 
          if (file.size() == 0) {
            Serial.println("File is empty");
          } else {
            Serial.println("File contents:");
            while (file.available()) {
              Serial.write(file.read());
            }
          }
        } else {
          Serial.println("Failed to seek to the beginning of the file.");
        }
      }

      file.close();
    } else if (command == "help") {
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
  }
}