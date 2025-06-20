# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/mike/esp/ESP8266_RTOS_SDK/components/bootloader/subproject"
  "/home/mike/ESP8266-mcp/build/bootloader"
  "/home/mike/ESP8266-mcp/build/bootloader-prefix"
  "/home/mike/ESP8266-mcp/build/bootloader-prefix/tmp"
  "/home/mike/ESP8266-mcp/build/bootloader-prefix/src/bootloader-stamp"
  "/home/mike/ESP8266-mcp/build/bootloader-prefix/src"
  "/home/mike/ESP8266-mcp/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mike/ESP8266-mcp/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mike/ESP8266-mcp/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
