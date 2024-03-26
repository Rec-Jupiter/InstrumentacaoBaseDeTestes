cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(InstrBase C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(InstrBase
    src/main.cpp
)

pico_enable_stdio_usb(InstrBase 1)
pico_enable_stdio_uart(InstrBase 1)

pico_add_extra_outputs(InstrBase)

#add_subdirectory(pico-ssd1306)
target_link_libraries(InstrBase
    pico_stdlib
    pico_multicore
    #    pico_ssd1306
    hardware_i2c)
