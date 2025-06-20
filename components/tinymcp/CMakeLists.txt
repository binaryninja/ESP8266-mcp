idf_component_register(
    SRCS
        "MCPServer.cpp"
        "EspSocketTransport.cpp"
        "src/tinymcp_json.cpp"
        "src/tinymcp_message.cpp"
        "src/tinymcp_request.cpp"
        "src/tinymcp_response.cpp"
        "src/tinymcp_notification.cpp"
        "src/tinymcp_session.cpp"
        "src/tinymcp_socket_transport.cpp"
        "src/tinymcp_tools.cpp"
    INCLUDE_DIRS
        "."
        "include"
    REQUIRES lwip freertos json pthread driver spiffs
)

# C++ compilation with exceptions and RTTI enabled
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++14 -Os -ffunction-sections -fdata-sections -frtti -fexceptions
)
