cmake_minimum_required(VERSION 3.5)

# Set component requirements
set(COMPONENT_REQUIRES
    nvs_flash
    esp_event
    driver
    esp_timer
    esp_system
    freertos
    duktape
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(js-vm)

include_directories(include)

set(SOURCE_FILES
    src/duktape_bindings.cpp
    src/file_system.cpp
    src/networking.cpp
    src/serial_handler.cpp
    src/vm_manager.cpp
)
