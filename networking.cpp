// networking.cpp
#include "networking.h"
#include "file_system.h" // Include for SPIFFS

// Initialize these here as they are declared as extern in the header
WiFiUDP udp;
FTPServer ftp;

void initWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initUDP(uint16_t port) {
  udp.begin(port); 
}

void initFTP(const char* user, const char* password) {
  ftp.addUser(user, password);       
  ftp.addFilesystem("SPIFFS", &SPIFFS); 
  ftp.begin();                         
  Serial.println("FTP Server started");
}

void handleFTP() {
  ftp.handle(); 
}