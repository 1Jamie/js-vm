// networking.cpp
#include "include/networking.h"
#include "include/vm_manager.h"
#include "include/file_system.h"

// Initialize these here as they are declared as extern in the header
WiFiUDP udp;

void initWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void initUDP(uint16_t port) {
  if (!udp.begin(port)) {
    Serial.println("Failed to start UDP server");
    return;
  }
}

void handleUDP() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;
      
      // Create a new VM with the received JavaScript code
      String filename = "udp_" + String(millis()) + ".js";
      
      // Write the file to storage
      File file = FFat.open(filename.c_str(), "w");
      if (file) {
        file.write((uint8_t*)incomingPacket, len);
        file.close();
        
        // Create and start the VM
        createVM(filename, incomingPacket, filename);
      }
    }
  }
}