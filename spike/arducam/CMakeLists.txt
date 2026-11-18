# ArduCAM Mega spike (#37): upstream's driver (zephyr main, drivers/video/video_arducam_mega.c,
# added 2026-03-24) vendored against our v4.2 tree, and a capture loop. No Toit.
cmake_minimum_required(VERSION 3.20.0)
set(DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arducam_spike)
target_include_directories(app PRIVATE include ${ZEPHYR_BASE}/drivers/video)
target_sources(app PRIVATE src/main.c src/video_arducam_mega.c)
