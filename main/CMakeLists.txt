idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES tinymcp nvs_flash pthread
)

# C++ compilation with exceptions disabled for ESP8266 stability
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++17 -Os -frtti -fno-exceptions
)
