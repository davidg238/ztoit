# Toit VM core as a Zephyr library. Sources come from $TOIT_SRC (env.sh).
if(CONFIG_TOIT)
  if(NOT DEFINED ENV{TOIT_SRC})
    message(FATAL_ERROR "TOIT_SRC is not set: source ztoit's env.sh")
  endif()
  set(T $ENV{TOIT_SRC}/src)

  # Same split as toit's src/CMakeLists.txt: every core file, minus host entry
  # points. Platform variants compile to nothing behind their own #ifdefs.
  file(GLOB toit_core ${T}/*.cc)
  list(FILTER toit_core EXCLUDE REGEX
    "/(toit|toit_run|toit_run_image|vessel|run|main_utf_8_helper|error_win|toit_esp32|toit_ec618|primitive_zephyr)\\.cc$")
  list(APPEND toit_core ${T}/event_sources/timer.cc ${T}/resources/timer.cc
    ${T}/event_sources/zsock_zephyr.cc ${T}/resources/udp_zephyr.cc ${T}/resources/tcp_zephyr.cc
    ${T}/event_sources/zephyr_keys.cc ${T}/resources/ztoit.cc
    ${T}/event_sources/ble_udp_zephyr.cc ${T}/resources/udp_ble.cc
    ${T}/resources/image.cc)
  # Toit's standard gpio library, when the board's devicetree has a
  # toit,platform node (dts/bindings/misc/toit,platform.yaml, ztoit#62).
  # Without it the gpio primitives stay unlinked: PRIMITIVE_LOOKUP_FAILED.
  if(CONFIG_DT_HAS_TOIT_PLATFORM_ENABLED)
    list(APPEND toit_core ${T}/primitive_zephyr.cc ${T}/resources/pins_zephyr.cc
      ${T}/resources/gpio_zephyr.cc ${T}/event_sources/zephyr.cc)
  endif()
  set(toit_gc
    ${T}/third_party/dartino/object_memory.cc
    ${T}/third_party/dartino/object_memory_copying.cc
    ${T}/third_party/dartino/object_memory_mark_sweep.cc
    ${T}/third_party/dartino/gc_metadata.cc
    ${T}/third_party/dartino/two_space_heap.cc)

  zephyr_library_named(toit_vm)
  zephyr_library_sources(${toit_core} ${toit_gc})
  zephyr_library_include_directories(${T} $ENV{TOIT_SRC}/include
    $ENV{TOIT_SRC}/third_party/dragonbox/include)
  zephyr_library_compile_definitions(TOIT_DEPLOY _DEFAULT_SOURCE)
  zephyr_library_link_libraries_ifdef(CONFIG_MBEDTLS mbedTLS)
  if(CONFIG_MBEDTLS_VERSION_4_x)  # legacy headers moved to private/ (mbedtls4/README)
    zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mbedtls4)  # the app includes VM headers too
  endif()
  zephyr_library_compile_options(-fno-exceptions -fno-rtti -Wno-unused -Wno-sign-compare)
  set_source_files_properties(${T}/interpreter_core.cc ${T}/interpreter_run.cc
    PROPERTIES COMPILE_OPTIONS "-O3")
  execute_process(COMMAND git -C $ENV{TOIT_SRC} describe --tags --always --dirty
    OUTPUT_VARIABLE toit_git OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_source_files_properties(${T}/utils.cc PROPERTIES COMPILE_DEFINITIONS
    "TOIT_MODEL=\"zephyr-${CONFIG_BOARD}\";VM_GIT_INFO=\"${toit_git}\";VM_GIT_VERSION=\"${toit_git}\"")
endif()
