// main.cpp
#include <Arduino.h>
#include "vm_manager.h"
#include "networking.h"
#include "file_system.h"
#include "serial_handler.h"

// Configuration (Adjust as needed)
#define WIFI_SSID "Your-SSID"
#define WIFI_PASSWORD "your-password"
#define UDP_PORT 4210
#define FTP_USER "esp32"
#define FTP_PASSWORD "esp32"


void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 JavaScript VM Platform (Duktape)...");

  // Initialize Networking (WiFi)
  initWiFi(WIFI_SSID, WIFI_PASSWORD);
  
  // Initialize UDP
  initUDP(UDP_PORT);

  // Initialize FTP Server
  initFTP(FTP_USER, FTP_PASSWORD);

  // Initialize SPIFFS
  initSPIFFS();

  // Scan SPIFFS for .js files and start VMs
  scanSPIFFSAndStartVMs();
}

void loop() {
  handleSerialInput();
  handleFTP(); 
  monitorAndRescheduleVMs();
  vTaskDelay(10); 
}