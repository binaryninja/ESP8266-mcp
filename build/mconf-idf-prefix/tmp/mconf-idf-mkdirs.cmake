# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/mike/esp/ESP8266_RTOS_SDK/tools/kconfig"
  "/home/mike/ESP8266-mcp/build/kconfig_bin"
  "/home/mike/ESP8266-mcp/build/mconf-idf-prefix"
  "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/tmp"
  "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/src/mconf-idf-stamp"
  "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/src"
  "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/src/mconf-idf-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/src/mconf-idf-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mike/ESP8266-mcp/build/mconf-idf-prefix/src/mconf-idf-stamp${cfgdir}") # cfgdir has leading slash
endif()
