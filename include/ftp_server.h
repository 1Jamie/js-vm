#pragma once

#include <WiFi.h>
#include <WiFiClient.h>

class FTPServer {
public:
    static bool begin(const char* ssid, const char* password);
    static void handle();
    
private:
    static WiFiServer controlServer;
    static WiFiServer dataServer;
    static WiFiClient controlClient;
    static WiFiClient dataClient;
    static bool initialized;
    static void handleClient();
    static void sendResponse(int code, const char* message);
    static void processCommand();
    static void handlePASV();
    static void handlePORT();
    static void handleLIST();
    static void handleRETR(const char* filename);
    static void handleSTOR(const char* filename);
    static void handleDELE(const char* filename);
    static char cmdBuffer[256];
    static char username[32];
    static char password[32];
    static bool loggedIn;
    static bool dataMode;
    static bool passiveMode;
    static IPAddress dataIp;
    static uint16_t dataPort;
    static void setupDataConnection();
    static bool connectToClient();
    static void resetState();
    static void cleanupDataConnection();
    static unsigned long lastCmdTime;
    static const unsigned long CMD_TIMEOUT = 300000; // 5 minutes
};
