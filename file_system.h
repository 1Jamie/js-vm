// file_system.h
#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <Arduino.h>
#include <SPIFFS.h>

void initSPIFFS();
void scanSPIFFSAndStartVMs();
bool isJSFile(const String& filename);
void checkFileChanges(int vmIndex);
void listFiles(File dir, int indent); // Add this declaration

#endif 