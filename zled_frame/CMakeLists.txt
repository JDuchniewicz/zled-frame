#-------------------------------------------------------------------------------
# ZLED Frame Project
#
# Copyright (c) 2024 Jakub Duchniewicz
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zled_frame LANGUAGES C)

target_sources(app PRIVATE
            src/main.c
            src/pixels.c
            src/network.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

foreach(inc_file
	served_content.html.bin
    )
  generate_inc_file_for_target(
    app
    src/${inc_file}
    ${gen_dir}/${inc_file}.inc
    )
endforeach()
