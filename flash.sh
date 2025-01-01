#!/bin/bash

# Configuration
ARDUINO_CLI="./bin/arduino-cli"
BOARD="esp32:esp32:esp32s3"
BOARD_OPTIONS="PartitionScheme=custom"
PORT="/dev/ttyACM0"
SKETCH="js-vm.ino"

# Make sure we're in the right directory
cd "$(dirname "$0")"

# Create build flags with custom partition file
BUILD_FLAGS="-DBOARD_BUILD_PARTITIONS=\"partitions.csv\""

echo " Compiling sketch..."
$ARDUINO_CLI compile --fqbn "$BOARD" --board-options "$BOARD_OPTIONS" --build-property "build.extra_flags=$BUILD_FLAGS" $SKETCH
if [ $? -ne 0 ]; then
    echo " Compilation failed!"
    exit 1
fi
echo " Compilation successful!"

echo " Uploading to ESP32-S3..."
$ARDUINO_CLI upload --fqbn "$BOARD" --board-options "$BOARD_OPTIONS" --port "$PORT" $SKETCH
if [ $? -ne 0 ]; then
    echo " Upload failed!"
    exit 1
fi
echo " Upload successful!"

echo " Flash complete!"
