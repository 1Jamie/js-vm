// file_system.h
#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>

// Filesystem functions
bool initFS();
void checkFileChanges(int vmIndex);
bool isJSFile(const char* filename);
void listFiles(File dir, int indent);

#endif