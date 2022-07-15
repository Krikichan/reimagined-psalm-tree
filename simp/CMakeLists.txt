# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(simp_demo)

#Add Folder Structure
zephyr_include_directories(src)
zephyr_include_directories(libs/simp)

#Library adds
target_sources(app PRIVATE libs/simp/uart_drv.c)
target_sources(app PRIVATE libs/simp/simp.c)

target_sources(app PRIVATE src/main.c)
