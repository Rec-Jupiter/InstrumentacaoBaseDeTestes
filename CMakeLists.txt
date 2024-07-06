cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(InstrBase C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(InstrBase
        src/main.cpp
        src/helpers.h
        src/hw_config.c
        src/sd_card_driver.c
        src/sd_card_driver.h
        src/config.h
        src/main.h
        src/helpers.c
        src/display.pio
        src/display_driver.c
        src/display_driver.h)

pico_generate_pio_header(InstrBase ${CMAKE_CURRENT_LIST_DIR}/src/display.pio)

pico_enable_stdio_usb(InstrBase 1)
#pico_enable_stdio_uart(InstrBase 1)

pico_add_extra_outputs(InstrBase)

add_subdirectory(src/extern/pico-ssd1306)
add_subdirectory(src/extern/hx711-pico-c)
add_subdirectory(src/extern/no-os-fat32-sd/src build)
target_link_libraries(InstrBase
    pico_stdlib
    pico_multicore
    hardware_pio
    hx711-pico-c
    pico_ssd1306
    hardware_i2c
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)


pico_add_extra_outputs(InstrBase)
