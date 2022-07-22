# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf")
    list(APPEND mcuboot_OVERLAY_CONFIG
      "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf"
      )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bchip_dfu)

zephyr_include_directories(src)
zephyr_include_directories(libs/simp)
zephyr_include_directories(libs/flash)
zephyr_include_directories(libs/dfu_module)

#Library adds
target_sources(app PRIVATE libs/dfu_module/dfu_module_52.c)
target_sources(app PRIVATE libs/simp/uart_drv.c)
target_sources(app PRIVATE libs/simp/simp.c)
target_sources(app PRIVATE libs/flash/app_flash.c)

target_sources(app PRIVATE src/main.c)

zephyr_library_include_directories(.)