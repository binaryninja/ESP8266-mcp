idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES tinymcp nvs_flash pthread
)

# C++ compilation with exceptions and RTTI enabled
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++14 -Os -frtti -fexceptions
)
