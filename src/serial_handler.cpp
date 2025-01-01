// serial_handler.cpp
#include "include/serial_handler.h"
#include "include/vm_manager.h"
#include "include/file_system.h"

void printVMInfo(int vmIndex) {
  if (vmIndex >= 0 && vmIndex < MAX_VMS) {
    auto& vm = vms[vmIndex];
    Serial.printf("VM %d:\n", vmIndex);
    Serial.printf("  File: %s\n", vm.filename.c_str());
    Serial.printf("  Status: %s\n", vm.running ? "Running" : "Stopped");
    Serial.printf("  Last Run: %lu ms ago\n", millis() - vm.lastRunTime);
    Serial.printf("  Memory: %lu bytes\n", vm.memoryAllocated);
  }
}

void handleSerialCommand(const String& command) {
  Serial.printf("Received command: %s\n", command.c_str());
  
  String cmd = command;
  cmd.trim();
  
  if (cmd.length() == 0) {
    return;
  }

  // Split command into parts
  int spaceIndex = cmd.indexOf(' ');
  String action = spaceIndex > 0 ? cmd.substring(0, spaceIndex) : cmd;
  String args = spaceIndex > 0 ? cmd.substring(spaceIndex + 1) : "";
  action.toLowerCase();

  Serial.printf("Action: %s, Args: %s\n", action.c_str(), args.c_str());

  if (action == "create") {
    if (args.length() == 0) {
      Serial.println("Usage: create <filename>");
      return;
    }

    String filename = args;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }

    File file = FFat.open(filename, "r");
    if (!file) {
      Serial.printf("Error: File %s not found\n", filename.c_str());
      return;
    }

    String content = file.readString();
    file.close();

    Serial.printf("Creating VM for file: %s\n", filename.c_str());
    int vmId = createVM(filename, content.c_str(), filename);
    
    if (vmId >= 0) {
      Serial.println("VM created successfully:");
      Serial.printf("VM %d:\n", vmId);
      Serial.printf("  File: %s\n", vms[vmId].filename.c_str());
      Serial.printf("  Status: %s\n", vms[vmId].running ? "Running" : "Stopped");
      Serial.printf("  Last Run: %d ms ago\n", millis() - vms[vmId].lastRunTime);
      Serial.printf("  Memory: %d bytes\n", vms[vmId].memoryAllocated);
    }
  }
  else if (action == "write") {
    if (args.length() == 0) {
      Serial.println("Usage: write <filename> <content>");
      return;
    }

    int contentStart = args.indexOf(' ');
    if (contentStart <= 0) {
      Serial.println("Usage: write <filename> <content>");
      return;
    }

    String filename = args.substring(0, contentStart);
    String content = args.substring(contentStart + 1);

    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }

    File file = FFat.open(filename, "w");
    if (!file) {
      Serial.printf("Error: Could not create file %s\n", filename.c_str());
      return;
    }

    file.print(content);
    file.close();

    Serial.printf("Written to file: %s\nContent: %s\n", filename.c_str(), content.c_str());
  }
  else if (action == "vms") {
    Serial.println("Active VMs:");
    for (int i = 0; i < MAX_VMS; i++) {
      if (vms[i].running) {
        Serial.printf("VM %d:\n", i);
        Serial.printf("  File: %s\n", vms[i].filename.c_str());
        Serial.printf("  Status: %s\n", vms[i].running ? "Running" : "Stopped");
        Serial.printf("  Last Run: %d ms ago\n", millis() - vms[i].lastRunTime);
        Serial.printf("  Memory: %d bytes\n", vms[i].memoryAllocated);
      }
    }
  }
  else if (action == "stop") {
    if (args.length() == 0) {
      Serial.println("Usage: stop <vm_id>");
      return;
    }

    int vmId = args.toInt();
    if (vmId >= 0 && vmId < MAX_VMS) {
      Serial.printf("Stopping VM %d\n", vmId);
      stopVM(vmId);
      Serial.printf("VM %d stopped\n", vmId);
    }
  }
  else if (action == "start") {
    if (args.length() == 0) {
      Serial.println("Usage: start <vm_id>");
      return;
    }

    int vmId = args.toInt();
    if (vmId >= 0 && vmId < MAX_VMS) {
      Serial.printf("Starting VM %d\n", vmId);
      startVM(vmId);
      Serial.printf("VM %d started\n", vmId);
    }
  }
  else {
    Serial.println("Unknown command. Available commands:");
    Serial.println("  create <filename> - Create and start a VM from a JS file");
    Serial.println("  write <filename> <content> - Write content to a file");
    Serial.println("  vms - List all active VMs");
    Serial.println("  stop <vm_id> - Stop a VM");
    Serial.println("  start <vm_id> - Start a stopped VM");
  }
}

void handleSerial() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }
}