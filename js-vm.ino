// main.cpp
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <FFat.h>
#include "include/vm_manager.h"
#include "include/duktape_bindings.h"
#include "include/file_system.h"
#include "include/networking.h"
#include "include/serial_handler.h"

// Configuration (Adjust as needed)
#define WIFI_SSID "Lastditchwifi-2.4"
#define WIFI_PASSWORD "Mune0420"
#define UDP_PORT 1337

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize filesystem
  if (!initFS()) {
    Serial.println("Failed to initialize filesystem");
    return;
  }
  Serial.println("Filesystem initialized successfully");

  // Initialize WiFi and UDP
  initWiFi(WIFI_SSID, WIFI_PASSWORD);
  initUDP(UDP_PORT);
  Serial.println("Connected! IP address: " + WiFi.localIP().toString());
  Serial.printf("UDP Server listening on port %d\n", UDP_PORT);
}

void loop() {
  handleSerial();
  handleUDP();
  
  // Monitor VMs
  monitorAndRescheduleVMs();
  
  // Small delay to prevent watchdog triggers
  delay(10);
}