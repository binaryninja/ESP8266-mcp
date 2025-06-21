idf_component_register(
    SRCS
        "MCPServer.cpp"
        "EspSocketTransport.cpp"
    INCLUDE_DIRS
        "."
        "include"
    REQUIRES lwip freertos json pthread spiffs
)

# C++ compilation with exceptions disabled for ESP8266 stability
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++17 -Os -ffunction-sections -fdata-sections -frtti -fno-exceptions
)
