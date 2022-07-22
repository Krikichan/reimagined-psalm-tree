#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-5-Clause-Nordic
#
cmake_minimum_required(VERSION 3.20.0)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf")
    list(APPEND mcuboot_OVERLAY_CONFIG
      "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf"
      )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(n91_dfu)

zephyr_include_directories(src)
zephyr_include_directories(libs/simp)
zephyr_include_directories(libs/flash)
zephyr_include_directories(libs/dfu_module)

target_sources(app PRIVATE src/main.c)

#Library adds
target_sources(app PRIVATE libs/dfu_module/http_client.c)
target_sources(app PRIVATE libs/dfu_module/int_dfu_helper.c)
target_sources(app PRIVATE libs/dfu_module/dfu_modul.c)

target_sources(app PRIVATE libs/simp/uart_drv.c)
target_sources(app PRIVATE libs/simp/simp.c)

target_sources(app PRIVATE libs/flash/app_flash.c)





