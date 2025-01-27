# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/ahirvela/gitdev/pico-simple-mqtt/_deps/picotool-src"
  "/home/ahirvela/gitdev/pico-simple-mqtt/_deps/picotool-build"
  "/home/ahirvela/gitdev/pico-simple-mqtt/_deps"
  "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/tmp"
  "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/src/picotoolBuild-stamp"
  "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/src"
  "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/src/picotoolBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/src/picotoolBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ahirvela/gitdev/pico-simple-mqtt/src/picotool/src/picotoolBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
