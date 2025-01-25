cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH /home/carter/Applications/Programming/pi_pico/pico-sdk/)

include(pico_sdk_import.cmake)

project(psram_adafruit C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(psram_adafruit
        psram_adafruit.c
        )

pico_generate_pio_header(psram_adafruit
${CMAKE_CURRENT_LIST_DIR}/psram_adafruit.pio
)

target_link_libraries(psram_adafruit pico_stdlib hardware_pio hardware_dma)

pico_enable_stdio_usb(psram_adafruit 1)
pico_enable_stdio_uart(psram_adafruit 0)

pico_add_extra_outputs(psram_adafruit)
