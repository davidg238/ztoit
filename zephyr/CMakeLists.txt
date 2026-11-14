cmake_minimum_required(VERSION 3.20.0)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../modules/toit)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ztoit LANGUAGES C CXX)
target_sources(app PRIVATE src/main.cc src/thread.cc)
target_include_directories(app PRIVATE $ENV{TOIT_SRC}/src $ENV{TOIT_SRC}/include)
target_compile_definitions(app PRIVATE TOIT_DEPLOY _DEFAULT_SOURCE)

# The boot program is toit/boot.toit (a minimal system process, see there),
# which runs $PROGRAM as app.toit. Built as snapshot -> relocatable 32-bit
# image -> a const byte array in flash (`program_image`, main.cc): it is read
# once, to install the program into the flash registry, so it must not take RAM.
set(PROGRAM ${CMAKE_CURRENT_SOURCE_DIR}/toit/hello.toit CACHE FILEPATH "Toit program to run")
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/toit)
add_custom_command(
  OUTPUT ${GEN}/program.image
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/toit/boot.toit ${GEN}/boot.toit
  COMMAND ${CMAKE_COMMAND} -E copy ${PROGRAM} ${GEN}/app.toit
  COMMAND ${CMAKE_COMMAND} -E copy $ENV{TOIT_SRC}/system/services.toit ${GEN}/services.toit
  COMMAND ${CMAKE_COMMAND} -E copy $ENV{TOIT_SRC}/system/extensions/shared/network_base.toit ${GEN}/network_base.toit
  COMMAND $ENV{TOIT_HOST} compile --snapshot -O2 -o ${GEN}/program.snapshot ${GEN}/boot.toit
  COMMAND $ENV{TOIT_HOST} tool snapshot-to-image --format=binary -m32
          -o ${GEN}/program.image ${GEN}/program.snapshot
  DEPENDS ${PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/toit/boot.toit
  VERBATIM)
generate_inc_file_for_target(app ${GEN}/program.image ${GEN}/program_image.inc)
target_include_directories(app PRIVATE ${GEN})
