cmake_minimum_required(VERSION 3.20.0)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../modules/toit)
# The VM core's crypto on Zephyr 4.5's Mbed TLS 4 / TF-PSA-Crypto.
list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/crypto_mbedtls4.conf)
# Unset, find_package takes whichever Zephyr the CMake package registry holds
# (the NCS one on msi), silently (#42).
if(NOT DEFINED ENV{ZEPHYR_BASE})
  message(FATAL_ERROR "ZEPHYR_BASE is not set: source ztoit's env.sh (#42)")
endif()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ztoit LANGUAGES C CXX)
target_sources(app PRIVATE src/main.cc)
# The QEMU test build's wire between two emulated GPIO pins (tests/qemu, #62).
if(CONFIG_GPIO_EMUL)
  target_sources(app PRIVATE ../tests/qemu/gpio-join.c)
endif()
if(CONFIG_SOC_SERIES_ESP32)
  target_sources(app PRIVATE src/esp32_fflush.c)
  zephyr_ld_options(-Wl,--wrap=fflush)
  if(CONFIG_WIFI_ESP32)
    target_sources(app PRIVATE src/esp32_wifi_timers.c)
    zephyr_ld_options(-Wl,--wrap=ets_timer_setfn -Wl,--wrap=ets_timer_done)
  endif()
endif()
if(CONFIG_TOIT_AGENT)
  target_sources(app PRIVATE src/agent/agent.c src/agent/console.c src/agent/msg.c src/agent/obs.c
                             src/agent/stage.c src/agent/store.c src/agent/ed25519.c)
  # Vendored TweetNaCl (ed25519.c's header): kept byte for byte, warnings and all.
  set_source_files_properties(src/agent/ed25519.c PROPERTIES COMPILE_OPTIONS -w)
  target_link_libraries(app PRIVATE mbedTLS)
  # obs/build's strings, taken at every build, not at configure (ztoit#24).
  set(ids ${CMAKE_CURRENT_BINARY_DIR}/ztoit_ids.h)
  add_custom_target(ztoit_ids
    COMMAND ${CMAKE_COMMAND} -DOUT=${ids} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/..
            -DTOIT_SRC=$ENV{TOIT_SRC} -DTOIT_HOST=$ENV{TOIT_HOST} -P ${CMAKE_CURRENT_SOURCE_DIR}/ztoit_ids.cmake
    BYPRODUCTS ${ids} VERBATIM)
  add_dependencies(app ztoit_ids)
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()
option(OT_COUNTERS "Debug: print OpenThread MAC counters every minute (ztoit#18)" OFF)
if(OT_COUNTERS)
  target_sources(app PRIVATE src/ot_counters.c)
endif()
option(FATAL_DUMP "Debug, nRF: report a fatal error or watchdog reset at the next boot (ztoit#23)" OFF)
if(FATAL_DUMP)
  target_sources(app PRIVATE src/fatal_dump.c)
  if(FATAL_DUMP_PEEK)  # "core-address;trx-address", see fatal_dump.c
    list(GET FATAL_DUMP_PEEK 0 peek_core)
    list(GET FATAL_DUMP_PEEK 1 peek_trx)
    target_compile_definitions(app PRIVATE FATAL_DUMP_PEEK PEEK_CORE=${peek_core} PEEK_TRX=${peek_trx})
  endif()
endif()
option(HFCLK_HOLD "Debug, nRF: hold the HF crystal on from boot, as USB does (ztoit#23)" OFF)
if(HFCLK_HOLD)
  target_sources(app PRIVATE src/hfclk_hold.c)
endif()
target_include_directories(app PRIVATE $ENV{TOIT_SRC}/src $ENV{TOIT_SRC}/include)
target_compile_definitions(app PRIVATE TOIT_DEPLOY _DEFAULT_SOURCE)

# The registry's images: the fork's Zephyr system program, then each of
# $CONTAINERS (run at boot). Each is built snapshot -> relocatable 32-bit image ->
# a const byte array in flash (images.inc, main.cc): it is read once, to install
# it into the flash registry, so it must not take RAM.
set(CONTAINERS ${CMAKE_CURRENT_SOURCE_DIR}/toit/hello.toit CACHE STRING "Toit containers (;-list) run at boot")
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/toit)
file(MAKE_DIRECTORY ${GEN})

# The compiler's dependency file makes an edit to any imported file rebuild the image.
function(toit_image stem source)
  add_custom_command(
    OUTPUT ${GEN}/${stem}.snapshot
    COMMAND $ENV{TOIT_HOST} compile --snapshot -O2 -o ${GEN}/${stem}.snapshot
            --dependency-file ${GEN}/${stem}.d --dependency-format ninja ${source}
    DEPENDS ${source}
    DEPFILE ${GEN}/${stem}.d
    VERBATIM)
  add_custom_command(
    OUTPUT ${GEN}/${stem}.image
    COMMAND $ENV{TOIT_HOST} tool snapshot-to-image --format=binary -m32
            -o ${GEN}/${stem}.image ${GEN}/${stem}.snapshot
    DEPENDS ${GEN}/${stem}.snapshot
    VERBATIM)
  generate_inc_file_for_target(app ${GEN}/${stem}.image ${GEN}/${stem}_image.inc)
endfunction()

set(SYSTEM_DIR $ENV{TOIT_SRC}/system/extensions/zephyr)
toit_image(system ${SYSTEM_DIR}/boot.toit)
set(arrays "static const unsigned char image_system[] __aligned(4) = {\n#include \"system_image.inc\"\n};\n")
set(entries "  { image_system, sizeof(image_system), 0 },\n")
set(stems system)
foreach(source ${CONTAINERS})
  get_filename_component(stem ${source} NAME_WE)
  if(stem IN_LIST stems)
    message(FATAL_ERROR "CONTAINERS: two images named ${stem}")
  endif()
  list(APPEND stems ${stem})
  toit_image(${stem} ${source})
  string(APPEND arrays "static const unsigned char image_${stem}[] __aligned(4) = {\n#include \"${stem}_image.inc\"\n};\n")
  string(APPEND entries "  { image_${stem}, sizeof(image_${stem}), FLAG_RUN_BOOT },\n")
endforeach()
# $TESTS (tests/qemu, tools/test-qemu.py): relocatable images the script has
# built, installed but not run at boot, each tagged with its 1-based place in
# the list; a runner container in $CONTAINERS starts them one at a time.
set(TESTS "" CACHE STRING "Toit test images (;-list) for the QEMU test runner")
set(tag 0)
foreach(image ${TESTS})
  math(EXPR tag "${tag} + 1")
  generate_inc_file_for_target(app ${image} ${GEN}/test_${tag}_image.inc)
  string(APPEND arrays "static const unsigned char image_test_${tag}[] __aligned(4) = {\n#include \"test_${tag}_image.inc\"\n};\n")
  string(APPEND entries "  { image_test_${tag}, sizeof(image_test_${tag}), 0, ${tag} },\n")
endforeach()
file(WRITE ${GEN}/images.inc.tmp "// Generated by zephyr/CMakeLists.txt.\n${arrays}static const Image images[] = {\n${entries}};\n")
configure_file(${GEN}/images.inc.tmp ${GEN}/images.inc COPYONLY)
target_include_directories(app PRIVATE ${GEN})
