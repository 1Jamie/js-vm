#pragma once

#include <WiFi.h>
#include <SimpleFTPServer.h>

class FTPServer {
public:
    static bool begin(const char* ssid, const char* password);
    static void handle();
    
private:
    static FtpServer ftpServer;
    static bool initialized;
};
