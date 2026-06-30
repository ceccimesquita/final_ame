cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mpu6050_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# TFLite Micro (instalação local) 
add_subdirectory(/home/cecilia/pico-tflmicro pico-tflmicro)

# Executável principal
add_executable(mpu6050_pico
    src/main.cpp
    src/mpu6050.cpp
)

target_include_directories(mpu6050_pico PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/model
)

target_link_libraries(mpu6050_pico
    pico_stdlib
    hardware_i2c
    m
    pico-tflmicro
)

pico_enable_stdio_usb(mpu6050_pico  1)
pico_enable_stdio_uart(mpu6050_pico 0)
pico_add_extra_outputs(mpu6050_pico)
