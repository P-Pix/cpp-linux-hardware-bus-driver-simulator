cmake_minimum_required(VERSION 3.18)
project(cpp_linux_hardware_bus_driver_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LHBD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

add_library(lhbd_core
    src/protocol/Crc32.cpp
    src/protocol/FrameCodec.cpp
    src/transport/MockTransport.cpp
    src/transport/UnixSocketTransport.cpp
    src/utils/Logger.cpp
    src/core/DeviceDriver.cpp
)

target_include_directories(lhbd_core PUBLIC include)

target_compile_features(lhbd_core PUBLIC cxx_std_20)

if(LHBD_ENABLE_WARNINGS)
    target_compile_options(lhbd_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(lhbd_cli src/apps/device_cli.cpp)
target_link_libraries(lhbd_cli PRIVATE lhbd_core)

add_executable(lhbd_tests tests/test_main.cpp)
target_link_libraries(lhbd_tests PRIVATE lhbd_core)

include(CTest)
enable_testing()
add_test(NAME lhbd_unit_tests COMMAND lhbd_tests)
