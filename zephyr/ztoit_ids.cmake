# cmake -P, at every build (CMakeLists.txt, target ztoit_ids): write OUT, the
# obs/build strings (ztoit#24), only when they change, so an unchanged tree does
# not recompile agent.c. Args: OUT, SRC (this repo), TOIT_SRC, TOIT_HOST.
set(ENV{TOIT_SRC} ${TOIT_SRC})
execute_process(COMMAND sh ${SRC}/tools/sdk-id.sh
                OUTPUT_VARIABLE sdk OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND git -C ${SRC} describe --always --dirty
                OUTPUT_VARIABLE build OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${TOIT_HOST} version OUTPUT_VARIABLE host OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT host STREQUAL sdk)
  message(WARNING "ztoit: the firmware reports SDK \"${sdk}\" but the host toolkit "
                  "(${TOIT_HOST}) is \"${host}\": rerun tools/fork-sdk.sh")
endif()
set(text "#define ZTOIT_SDK \"${sdk}\"\n#define ZTOIT_BUILD \"ztoit ${build}\"\n")
set(old "")
if(EXISTS ${OUT})
  file(READ ${OUT} old)
endif()
if(NOT old STREQUAL text)
  file(WRITE ${OUT} "${text}")
endif()
