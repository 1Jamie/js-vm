// networking.h
#ifndef NETWORKING_H
#define NETWORKING_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// Declare externally as it's used in other modules
extern WiFiUDP udp;

void initWiFi(const char* ssid, const char* password);
void initUDP(uint16_t port);
void handleUDP();

#endif