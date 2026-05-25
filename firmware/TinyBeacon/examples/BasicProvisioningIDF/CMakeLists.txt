cmake_minimum_required(VERSION 3.16)

# Append parent directory to CMake search path to find TinyBeacon component
list(APPEND EXTRA_COMPONENT_DIRS "../../")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(BasicProvisioningIDF)
