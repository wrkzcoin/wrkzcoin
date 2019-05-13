cmake_minimum_required(VERSION 2.6)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "-- Argon2: Found ccache package... Activating...")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(Argon2 C)
set(ARGON2_VERSION 1.0)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckCSourceCompiles)
find_package(Threads REQUIRED)

add_library(argon2-interface INTERFACE)
target_include_directories(argon2-interface INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(argon2-internal INTERFACE)
target_include_directories(argon2-internal INTERFACE lib lib/blake2)
target_link_libraries(argon2-internal INTERFACE argon2-interface)

set(ARGON2_SRC
  lib/argon2.c
  lib/core.c
  lib/encoding.c
  lib/genkat.c
  lib/impl-select.c
  lib/thread.c
  lib/blake2/blake2.c
)

add_library(argon2 STATIC ${ARGON2_SRC})

target_compile_definitions(argon2
  PUBLIC "A2_VISCTL"
)

target_link_libraries(argon2
  PUBLIC argon2-interface ${CMAKE_THREAD_LIBS_INIT}
  PRIVATE argon2-internal
)

set_property(TARGET argon2 PROPERTY C_STANDARD 90)
set_property(TARGET argon2 PROPERTY VERSION ${Upstream_VERSION})
set_property(TARGET argon2 PROPERTY SOVERSION 1)
set_property(TARGET argon2 PROPERTY INTERFACE_ARGON2_MAJOR_VERSION 1)
set_property(TARGET argon2 APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING ARGON2_MAJOR_VERSION
)

install(TARGETS argon2 ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES "${CMAKE_SOURCE_DIR}/include/argon2.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

message("-- Argon2: Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message("-- Argon2: Build Type: ${ARCH}")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND NOT "${ARCH}" STREQUAL "default")
  function(add_feature_impl FEATURE GCC_FLAG DEF)
    add_library(argon2-${FEATURE} STATIC
      arch/x86_64/lib/argon2-${FEATURE}.c
    )
    target_link_libraries(argon2-${FEATURE} PRIVATE argon2-internal)
    set_target_properties(argon2-${FEATURE}
      PROPERTIES POSITION_INDEPENDENT_CODE True
    )

    message("-- Argon2: Detecting feature '${FEATURE}'...")
    file(READ arch/x86_64/src/test-feature-${FEATURE}.c SOURCE_CODE)

    # try without flag:
    check_c_source_compiles("${SOURCE_CODE}" FEATURE_${FEATURE}_NOFLAG)
    set(HAS_FEATURE ${FEATURE_${FEATURE}_NOFLAG})
    if(NOT "${HAS_FEATURE}")
      # try with -m<feature> flag:
      set(CMAKE_REQUIRED_FLAGS ${GCC_FLAG})
      check_c_source_compiles("${SOURCE_CODE}" FEATURE_${FEATURE}_FLAG)
      set(CMAKE_REQUIRED_FLAGS "")

      set(HAS_FEATURE ${FEATURE_${FEATURE}_FLAG})
      if(${HAS_FEATURE})
          target_compile_options(argon2-${FEATURE} PRIVATE ${GCC_FLAG})
      endif()
    endif()

    if(${HAS_FEATURE})
      message("-- Feature '${FEATURE}' detected!")
      target_compile_definitions(argon2-${FEATURE} PRIVATE ${DEF})
      target_link_libraries(argon2 PUBLIC argon2-${FEATURE})
    else()
      message("-- Feature '${FEATURE}' not found!")
    endif()
  endfunction()

  target_include_directories(argon2-internal INTERFACE arch/x86_64/lib)


  add_feature_impl(sse2    -msse2    HAVE_SSE2)
  add_feature_impl(ssse3   -mssse3   HAVE_SSSE3)
  add_feature_impl(xop     -mxop     HAVE_XOP)
  add_feature_impl(avx2    -mavx2    HAVE_AVX2)
  add_feature_impl(avx512f -mavx512f HAVE_AVX512F)

  target_sources(argon2 PRIVATE
      arch/x86_64/lib/argon2-arch.c
      arch/x86_64/lib/cpu-flags.c
  )
else()
  target_sources(argon2 PRIVATE
      arch/generic/lib/argon2-arch.c
  )
endif()
