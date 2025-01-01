set(SOURCES "duktape.c")
set(INCLUDES "include")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDES}
    REQUIRES "esp_system"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-label -Wno-maybe-uninitialized)
