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
    "/(toit|toit_run|toit_run_image|vessel|run|main_utf_8_helper|error_win|toit_esp32|toit_ec618)\\.cc$")
  list(APPEND toit_core ${T}/event_sources/timer.cc ${T}/resources/timer.cc
    ${T}/event_sources/zsock_zephyr.cc ${T}/resources/udp_zephyr.cc ${T}/resources/tcp_zephyr.cc)
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
  zephyr_library_compile_options(-fno-exceptions -fno-rtti -Wno-unused -Wno-sign-compare)
  set_source_files_properties(${T}/interpreter_core.cc ${T}/interpreter_run.cc
    PROPERTIES COMPILE_OPTIONS "-O3")
  execute_process(COMMAND git -C $ENV{TOIT_SRC} describe --tags --always --dirty
    OUTPUT_VARIABLE toit_git OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_source_files_properties(${T}/utils.cc PROPERTIES COMPILE_DEFINITIONS
    "TOIT_MODEL=\"zephyr-${CONFIG_BOARD}\";VM_GIT_INFO=\"${toit_git}\";VM_GIT_VERSION=\"${toit_git}\"")
endif()
