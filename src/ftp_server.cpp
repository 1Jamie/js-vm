#include "../include/ftp_server.h"
#include <FFat.h>

// Static member initialization
WiFiServer FTPServer::controlServer(21);
WiFiServer FTPServer::dataServer(2121);
WiFiClient FTPServer::controlClient;
WiFiClient FTPServer::dataClient;
bool FTPServer::initialized = false;
char FTPServer::cmdBuffer[256];
char FTPServer::username[32] = "esp32";
char FTPServer::password[32] = "esp32";
bool FTPServer::loggedIn = false;
bool FTPServer::dataMode = false;
bool FTPServer::passiveMode = false;
IPAddress FTPServer::dataIp;
uint16_t FTPServer::dataPort = 0;
unsigned long FTPServer::lastCmdTime = 0;

void FTPServer::resetState() {
    loggedIn = false;
    dataMode = false;
    passiveMode = false;
    dataPort = 0;
    cleanupDataConnection();
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
}

void FTPServer::cleanupDataConnection() {
    if (dataClient) {
        dataClient.stop();
    }
    // Always ensure data server is running for passive mode
    dataServer.begin();
}

bool FTPServer::begin(const char* ssid, const char* password) {
    if (initialized) {
        return true;
    }

    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Wait for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connection failed");
        return false;
    }

    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Start FTP server
    controlServer.begin();
    dataServer.begin();
    
    Serial.println("FTP server started");
    Serial.println("Username: esp32");
    Serial.println("Password: esp32");
    
    initialized = true;
    resetState();
    return true;
}

void FTPServer::handle() {
    if (!initialized) return;

    // Check for client timeout
    if (controlClient && controlClient.connected()) {
        if (millis() - lastCmdTime > CMD_TIMEOUT) {
            Serial.println("Client timeout, disconnecting");
            controlClient.stop();
            resetState();
            return;
        }
    }

    // Handle disconnected control client
    if (controlClient && !controlClient.connected()) {
        Serial.println("Client disconnected");
        controlClient.stop();
        resetState();
        return;
    }

    // Accept new client if none connected
    if (!controlClient || !controlClient.connected()) {
        WiFiClient newClient = controlServer.available();
        if (newClient) {
            // If we somehow still have an old client, clean it up
            if (controlClient) {
                controlClient.stop();
            }
            controlClient = newClient;
            Serial.println("New client connected");
            resetState();
            lastCmdTime = millis();
            sendResponse(220, "ESP32 FTP Server ready");
        }
        return;
    }

    // Handle client commands
    if (controlClient.available()) {
        size_t len = controlClient.readBytesUntil('\n', cmdBuffer, sizeof(cmdBuffer));
        if (len > 0) {
            lastCmdTime = millis();
            
            // Clean up command string
            if (len > 1 && cmdBuffer[len-2] == '\r') {
                len -= 2;
            } else if (cmdBuffer[len-1] == '\r') {
                len -= 1;
            }
            cmdBuffer[len] = 0;
            
            Serial.printf("FTP CMD: %s\n", cmdBuffer);
            processCommand();
            memset(cmdBuffer, 0, sizeof(cmdBuffer));  // Clear buffer after processing
        }
    }
}

void FTPServer::sendResponse(int code, const char* message) {
    char response[256];
    snprintf(response, sizeof(response), "%d %s\r\n", code, message);
    controlClient.print(response);
    Serial.printf("FTP Response: %s", response);
}

void FTPServer::processCommand() {
    char cmd[5] = {0};
    char arg[251] = {0};
    
    if (sscanf(cmdBuffer, "%4s %250s", cmd, arg) < 1) {
        return;
    }

    if (strcmp(cmd, "USER") == 0) {
        if (strcmp(arg, username) == 0) {
            sendResponse(331, "Password required");
        } else {
            sendResponse(530, "Invalid username");
        }
    }
    else if (strcmp(cmd, "PASS") == 0) {
        if (strcmp(arg, password) == 0) {
            loggedIn = true;
            sendResponse(230, "Login successful");
        } else {
            sendResponse(530, "Invalid password");
        }
    }
    else if (!loggedIn) {
        sendResponse(530, "Not logged in");
    }
    else if (strcmp(cmd, "AUTH") == 0) {
        sendResponse(504, "Auth not supported");
    }
    else if (strcmp(cmd, "SYST") == 0) {
        sendResponse(215, "UNIX Type: L8");
    }
    else if (strcmp(cmd, "FEAT") == 0) {
        controlClient.println("211-Features:");
        controlClient.println(" SIZE");
        controlClient.println(" MDTM");
        controlClient.println(" PORT");
        controlClient.println(" PASV");
        controlClient.println(" DELE");
        controlClient.println(" CWD");
        controlClient.println(" MKD");
        controlClient.println(" RMD");
        controlClient.println("211 End");
    }
    else if (strcmp(cmd, "PWD") == 0) {
        sendResponse(257, "\"/\" is current directory");
    }
    else if (strcmp(cmd, "CWD") == 0) {
        // We only support root directory, so any CWD is fine
        sendResponse(250, "Directory changed to /");
    }
    else if (strcmp(cmd, "SIZE") == 0) {
        String fname = arg;
        if (!fname.startsWith("/")) {
            fname = "/" + fname;
        }
        File file = FFat.open(fname.c_str(), "r");
        if (!file) {
            sendResponse(550, "File not found");
        } else {
            char sizeStr[32];
            snprintf(sizeStr, sizeof(sizeStr), "%d", file.size());
            sendResponse(213, sizeStr);
            file.close();
        }
    }
    else if (strcmp(cmd, "MKD") == 0) {
        // We don't support directories, just pretend it worked
        sendResponse(257, "Directory created");
    }
    else if (strcmp(cmd, "RMD") == 0) {
        // We don't support directories, just pretend it worked
        sendResponse(250, "Directory removed");
    }
    else if (strcmp(cmd, "TYPE") == 0) {
        sendResponse(200, "Type set to I");
    }
    else if (strcmp(cmd, "PORT") == 0) {
        handlePORT();
    }
    else if (strcmp(cmd, "PASV") == 0) {
        handlePASV();
    }
    else if (strcmp(cmd, "LIST") == 0) {
        handleLIST();
    }
    else if (strcmp(cmd, "RETR") == 0) {
        handleRETR(arg);
    }
    else if (strcmp(cmd, "STOR") == 0) {
        handleSTOR(arg);
    }
    else if (strcmp(cmd, "DELE") == 0) {
        handleDELE(arg);
    }
    else if (strcmp(cmd, "QUIT") == 0) {
        sendResponse(221, "Goodbye");
        controlClient.stop();
        if (dataClient) dataClient.stop();
    }
    else {
        Serial.printf("Unknown command: %s\n", cmd);
        sendResponse(500, "Unknown command");
    }
}

void FTPServer::handlePORT() {
    int ip[4], port[2];
    if (sscanf(cmdBuffer, "PORT %d,%d,%d,%d,%d,%d",
               &ip[0], &ip[1], &ip[2], &ip[3], &port[0], &port[1]) != 6) {
        sendResponse(501, "Invalid PORT command");
        return;
    }

    dataIp = IPAddress(ip[0], ip[1], ip[2], ip[3]);
    dataPort = (port[0] << 8) | port[1];
    
    Serial.printf("Active Mode - IP: %s, Port: %d\n", 
                 dataIp.toString().c_str(), dataPort);
    
    passiveMode = false;
    dataMode = true;
    sendResponse(200, "PORT command successful");
}

void FTPServer::handlePASV() {
    cleanupDataConnection();
    
    // Stop any existing data server and restart it
    dataServer.end();
    delay(100);  // Give some time for the socket to close
    dataServer.begin();
    
    IPAddress ip = WiFi.localIP();
    int port = 2121;
    
    char response[64];
    snprintf(response, sizeof(response), "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
             ip[0], ip[1], ip[2], ip[3], port >> 8, port & 255);
    
    passiveMode = true;
    dataMode = true;
    Serial.printf("PASV: Listening on %s:%d\n", ip.toString().c_str(), port);
    sendResponse(227, response);
}

bool FTPServer::connectToClient() {
    if (passiveMode) {
        Serial.println("Waiting for passive connection...");
        unsigned long timeout = millis() + 5000;
        while (!dataClient && millis() < timeout) {
            dataClient = dataServer.available();
            if (!dataClient) {
                delay(1);
            } else {
                Serial.printf("Passive client connected from %s:%d\n", 
                    dataClient.remoteIP().toString().c_str(), 
                    dataClient.remotePort());
                return true;
            }
        }
        Serial.println("Passive connection timeout");
        return false;
    } else {
        Serial.printf("Connecting to client %s:%d\n", dataIp.toString().c_str(), dataPort);
        bool result = dataClient.connect(dataIp, dataPort);
        if (result) {
            Serial.println("Active connection established");
        } else {
            Serial.println("Active connection failed");
        }
        return result;
    }
}

void FTPServer::handleLIST() {
    sendResponse(150, "Opening data connection");
    
    if (!connectToClient()) {
        sendResponse(425, "Can't open data connection");
        return;
    }
    
    File root = FFat.open("/");
    if (!root || !root.isDirectory()) {
        sendResponse(550, "Failed to open directory");
        cleanupDataConnection();
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        char fileInfo[512];
        String filename = file.name();
        if (filename.startsWith("/")) {
            filename = filename.substring(1);
        }
        
        if (file.isDirectory()) {
            snprintf(fileInfo, sizeof(fileInfo),
                "drwxr-xr-x 1 root root %13s Jan 1 2025 %s\r\n",
                "4096",
                filename.c_str());
        } else {
            snprintf(fileInfo, sizeof(fileInfo),
                "-rw-r--r-- 1 root root %13d Jan 1 2025 %s\r\n",
                file.size(),
                filename.c_str());
        }
        dataClient.print(fileInfo);
        file = root.openNextFile();
    }
    
    root.close();
    cleanupDataConnection();
    sendResponse(226, "Transfer complete");
}

void FTPServer::handleRETR(const char* filename) {
    if (!dataMode) {
        sendResponse(425, "Use PORT or PASV first");
        return;
    }
    
    String fname = filename;
    if (!fname.startsWith("/")) {
        fname = "/" + fname;
    }
    
    Serial.printf("Preparing to send file: %s\n", fname.c_str());
    
    File file = FFat.open(fname.c_str(), "r");
    if (!file) {
        Serial.println("File not found");
        sendResponse(550, "File not found");
        return;
    }
    
    sendResponse(150, "Opening data connection for file transfer");
    
    if (!connectToClient()) {
        Serial.println("Data connection failed");
        sendResponse(425, "Can't open data connection");
        file.close();
        return;
    }
    
    Serial.printf("Starting file transfer, size: %d bytes\n", file.size());
    uint8_t buf[1024];
    size_t totalSent = 0;
    
    while (file.available()) {
        size_t len = file.read(buf, sizeof(buf));
        if (len > 0) {
            size_t sent = dataClient.write(buf, len);
            totalSent += sent;
            if (sent != len) {
                Serial.println("Send error - connection broken?");
                break;
            }
        }
    }
    
    file.close();
    cleanupDataConnection();
    
    Serial.printf("Transfer complete, %d bytes sent\n", totalSent);
    sendResponse(226, "Transfer complete");
}

void FTPServer::handleSTOR(const char* filename) {
    if (!dataMode) {
        sendResponse(425, "Use PORT or PASV first");
        return;
    }
    
    String fname = filename;
    if (!fname.startsWith("/")) {
        fname = "/" + fname;
    }
    
    Serial.printf("Preparing to receive file: %s\n", fname.c_str());
    
    File file = FFat.open(fname.c_str(), "w");
    if (!file) {
        Serial.println("Failed to create file");
        sendResponse(553, "Could not create file");
        return;
    }
    
    sendResponse(150, "Opening data connection for file transfer");
    
    if (!connectToClient()) {
        Serial.println("Data connection failed");
        sendResponse(425, "Can't open data connection");
        file.close();
        return;
    }
    
    Serial.println("Starting file transfer");
    uint8_t buf[1024];
    int bytesWritten = 0;
    unsigned long timeout = millis() + 5000; // 5 second timeout for initial data
    
    while (dataClient.connected()) {
        if (dataClient.available()) {
            size_t len = dataClient.read(buf, sizeof(buf));
            if (len > 0) {
                size_t written = file.write(buf, len);
                bytesWritten += written;
                timeout = millis() + 5000; // Reset timeout after receiving data
                if (written != len) {
                    Serial.println("Write error - disk full?");
                    break;
                }
            }
        } else if (millis() > timeout) {
            Serial.println("Transfer timeout");
            break;
        } else {
            delay(1);
        }
    }
    
    file.close();
    cleanupDataConnection();
    
    if (bytesWritten > 0) {
        Serial.printf("Transfer complete, %d bytes written\n", bytesWritten);
        sendResponse(226, "Transfer complete");
    } else {
        Serial.println("Transfer failed - no data received");
        sendResponse(451, "Transfer failed");
        FFat.remove(fname.c_str()); // Remove failed file
    }
}

void FTPServer::handleDELE(const char* filename) {
    String fname = filename;
    if (!fname.startsWith("/")) {
        fname = "/" + fname;
    }

    // Check if file exists first
    if (!FFat.exists(fname.c_str())) {
        sendResponse(550, "File not found");
        return;
    }

    // Try to delete the file
    if (FFat.remove(fname.c_str())) {
        sendResponse(250, "File deleted successfully");
    } else {
        sendResponse(450, "Failed to delete file");
    }
}
