idf_component_register(
    SRCS
        "MCPServer.cpp"
        "EspSocketTransport.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES lwip freertos json pthread
)

# C++ compilation with exceptions and RTTI enabled
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++14 -Os -ffunction-sections -fdata-sections -frtti -fexceptions
)
