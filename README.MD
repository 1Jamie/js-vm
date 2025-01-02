# ESP32-S3 JavaScript VM Platform Documentation (Arduino IDE)

This document provides comprehensive documentation for the ESP32-S3 JavaScript VM platform, using the Arduino IDE for development. This system runs multiple JavaScript virtual machines (VMs) concurrently on an ESP32-S3, executing code from files in the SPIFFS file system.  It includes inter-VM communication, file monitoring, and GPIO interaction.


## 1. Overview

This code implements a multi-VM JavaScript environment on the ESP32-S3 using the Duktape JavaScript engine and the Arduino IDE. It allows users to manage multiple JavaScript programs, each running independently.  The system monitors files for changes and provides a serial interface for management and debugging.  Additionally, it includes an FTP server for file management.

**Hardware:**

* ESP32-S3 DevKitC-1 (or compatible)
* SPIFFS file system (on-chip flash storage)

**Functionality:**

* Loads and executes JavaScript code from files in the SPIFFS file system.
* Manages multiple concurrent JavaScript VMs.
* Provides JavaScript bindings for GPIO control (digital/analog read/write, pinMode).
* Enables inter-VM communication via message queues.
* Includes UDP networking capabilities within the JavaScript VMs.
* Monitors SPIFFS for file changes, automatically restarting or terminating VMs upon changes.
* Offers a serial interface for controlling VMs (start, stop, restart, scan SPIFFS).
* Provides an FTP server for file management (requires the `ESP-FTP-Server-Lib`).


## 2. Dependencies & Installation

* **ESP32 board support:** Ensure the ESP32 board is installed in the Arduino IDE (via Boards Manager).
* **Duktape:** This will need to be included as a library. You'll likely find it as a zip file online; install it through the Arduino IDE's "Add .ZIP Library" option (Sketch > Include Library). Search for a suitable "Duktape Arduino" library.
* **ESP-FTP-Server-Lib:** This library is not a standard Arduino library. You'll need to find a compatible version for the Arduino ESP32 environment and install it as a .ZIP library. Search online repositories.
* **WiFi Library:** The Arduino ESP32 core already includes this.
* **SPIFFS Library:** The Arduino ESP32 core already includes this.

**Installation Steps:**

1. **Install ESP32 Board:** Open the Arduino IDE, go to File > Preferences, and add the Espressif ESP32 boards manager URL: `https://dl.espressif.com/dl/package_esp32_index.json`. Then go to Tools > Board > Boards Manager, search for "esp32", and install the latest version.  Select your ESP32-S3 board from the Tools > Board menu.
2. **Install Libraries:** Download the required Duktape and ESP-FTP-Server-Lib libraries as ZIP files, and add them to your Arduino IDE using Sketch > Include Library > Add .ZIP Library.
3. **Select Port:** Choose the correct serial port connected to your ESP32-S3 from the Tools > Port menu.


## 3. Code Structure & JavaScript Bindings

The code is organized into several sections:

* **Configuration:** Defines constants (maximum VMs, WiFi credentials, etc.).
* **VM Structure:** Defines the `VM` struct for each JavaScript VM.
* **Memory Management:** Functions for checking available heap memory.
* **VM Management:** Functions for creating, starting, and stopping VMs (`createVM`, `startVM`, `stopVM`).
* **File System Handling (SPIFFS):** Functions for managing SPIFFS files.
* **Duktape Bindings:** Functions exposing ESP32 features to JavaScript.
* **Serial Communication:** Functions for handling serial input.
* **Setup and Loop:** The main program functions.


This section details the JavaScript functions available within each VM. These provide access to ESP32 hardware and features.  Each function needs the `vmIndex` parameter to identify the calling VM.  `vmIndex` is available as a global variable within the Duktape context.


### JavaScript Bindings

**GPIO Control:**

* `digitalWrite(pin, value, vmIndex)`: Controls a digital pin.
    * `pin`: GPIO pin number (0-39).
    * `value`: HIGH or LOW.
    * `vmIndex`: Calling VM index.

* `digitalRead(pin, vmIndex)`: Reads a digital pin.
    * `pin`: GPIO pin number (0-39).
    * `vmIndex`: Calling VM index.
    * **Returns:** HIGH or LOW.

* `analogRead(pin, vmIndex)`: Reads an analog pin.
    * `pin`: GPIO pin number (0-39). Must be a valid analog pin.
    * `vmIndex`: Calling VM index.
    * **Returns:** Analog value (0-4095).

* `analogWrite(pin, value, vmIndex)`: Writes an analog value (PWM).
    * `pin`: GPIO pin number (0-39). Must be a valid PWM pin.
    * `value`: Value (0-255).
    * `vmIndex`: Calling VM index.

* `pinMode(pin, mode, vmIndex)`: Sets pin mode.
    * `pin`: GPIO pin number (0-39).
    * `mode`: INPUT, OUTPUT, INPUT_PULLUP.
    * `vmIndex`: Calling VM index.


**UDP Networking:**

* `udpSend(ip, port, message, vmIndex)`: Sends a UDP packet.
    * `ip`: Destination IP address (string).
    * `port`: Destination port.
    * `message`: Message (string).
    * `vmIndex`: Calling VM index.

* `udpReceive(vmIndex)`: Receives a UDP packet.
    * `vmIndex`: Calling VM index.
    * **Returns:** Received message (string), or "" if none.


**Inter-VM Communication:**

* `sendMessage(receiverID, message, vmIndex)`: Sends a message to another VM.
    * `receiverID`: Recipient VM index.
    * `message`: Message (string).
    * `vmIndex`: Calling VM index.

* `receiveMessage(vmIndex)`: Receives a message.
    * `vmIndex`: Calling VM index.
    * **Returns:** Received message (string), or "" if none.

**Other Utilities:**

* `print(message)`: Prints to the serial console.
    * `message`: Message (string).

* `wait(ms)`: Pauses execution.
    * `ms`: Delay in milliseconds.



## 4. Uploading the Code

1. Open the Arduino IDE.
2. Open the `.ino` file (the main Arduino sketch).
3. Select your ESP32-S3 board and port.
4. Click the "Upload" button to compile and upload the code.


## 5. Example JavaScript Code

```javascript
pinMode(2, OUTPUT, vmIndex); // Set pin 2 as output
while (true) {
  digitalWrite(2, HIGH, vmIndex);
  print("LED ON");
  wait(1000);
  digitalWrite(2, LOW, vmIndex);
  print("LED OFF");
  wait(1000);
}
```

## 6. Usage Instructions

1. Upload JavaScript files (`.js`) to the SPIFFS partition.  Methods for doing so vary; you might need to use the Arduino IDE's SPIFFS upload capabilities or another method.
2. Use the serial monitor (Tools > Serial Monitor) to interact with commands:
   * `status`: VM status.
   * `stop <vmIndex>`: Stop a VM.
   * `start <vmIndex>`: Start a VM.
   * `restart <vmIndex>`: Restart a VM.
   * `scan`: Scan SPIFFS for `.js` files.
   * `create <filename>`: Create a new VM.
   * `reboot`: Reboot the ESP32.
   * `help`: Display help.
   * `print <filename>`: Print file content from SPIFFS.

## 7. Troubleshooting

* **Memory errors:** Reduce `DEFAULT_VM_MEMORY` or the number of VMs.
* **SPIFFS errors:** Ensure proper SPIFFS formatting and mounting.  You may need to use the SPIFFS tools provided by the Arduino IDE.
* **Communication errors:** Check the serial monitor for error messages.
* **Library Issues:** Verify library compatibility and correct installation.

## 8. FTP Server

The ESP32 includes a built-in FTP server for easy file management. The server supports standard FTP clients like FileZilla, WinSCP, or command-line FTP.

### Connection Details
- Host: ESP32's IP address (shown on serial console at startup)
- Port: 21
- Username: esp32
- Password: esp32
- Mode: Both Active and Passive modes supported

### Supported FTP Commands
- Basic: USER, PASS, SYST, FEAT, PWD, TYPE
- File Operations: STOR, RETR, DELE, LIST, SIZE
- Directory Operations: CWD, MKD, RMD (flat filesystem)
- Transfer Modes: PORT, PASV

### File System Notes
- Uses FFat filesystem
- Flat file structure (no subdirectories)
- All paths are relative to root (/)
- Files are stored persistently
