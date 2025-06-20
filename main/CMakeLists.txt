idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES tinymcp
)

# Enable C++17 and optimize for size
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=gnu++17 -fno-exceptions -fno-rtti -Os
)
