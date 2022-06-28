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

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/http_client.c)
target_sources(app PRIVATE src/int_dfu_helper.c)

target_sources(app PRIVATE src/app_uart.c)
target_sources(app PRIVATE src/app_cmd.c)
target_sources(app PRIVATE src/app_flash.c)
target_sources(app PRIVATE src/app_flash_cmd.c)




