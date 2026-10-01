cmake_minimum_required(VERSION 3.20)
project(simcore VERSION 0.1.0 LANGUAGES CXX)

# simcore is header-only. This CMake file builds the tests and examples so the
# core can be developed and verified on any machine (PC, Mac, CI) without Unreal.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simcore INTERFACE)
target_include_directories(simcore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(SIMCORE_STRICT "Build with Unreal-like settings: no exceptions/RTTI, warnings as errors" ON)

function(simcore_configure_target tgt)
  target_link_libraries(${tgt} PRIVATE simcore)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /permissive- /utf-8)
    if(SIMCORE_STRICT)
      target_compile_options(${tgt} PRIVATE /WX /GR-)
    endif()
  else()
    # Optional trailing struct fields (ControlDef::guard_id, ProcedureStep::wait_for...) have defaults and are
    # meant to be left out of brace-initializers, which -Wmissing-field-initializers (in -Wextra) would reject.
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
                                          -Wno-missing-field-initializers)
    if(SIMCORE_STRICT)
      target_compile_options(${tgt} PRIVATE -Werror -fno-rtti)
    endif()
  endif()
endfunction()

# Tests (the tiny test framework uses std::function, so exceptions stay on for tests,
# but simcore headers themselves never throw).
enable_testing()
add_executable(simcore_tests
  tests/test_main.cpp
  tests/test_geo_atmosphere.cpp
  tests/test_radar.cpp
  tests/test_engagement.cpp
  tests/test_procedures.cpp
  tests/test_equipment.cpp
  tests/test_console.cpp)
simcore_configure_target(simcore_tests)
target_include_directories(simcore_tests PRIVATE examples)  # placeholder_vehicle.hpp
target_compile_definitions(simcore_tests PRIVATE SIMCORE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")
add_test(NAME simcore_tests COMMAND simcore_tests)

# Header self-containment + "no exceptions" check: compile the umbrella header alone.
add_library(simcore_header_check OBJECT tests/header_check.cpp)
simcore_configure_target(simcore_header_check)
if(NOT MSVC AND SIMCORE_STRICT)
  target_compile_options(simcore_header_check PRIVATE -fno-exceptions)
endif()

add_executable(engagement_demo examples/engagement_demo.cpp)
simcore_configure_target(engagement_demo)

# Osa console session from the march to the debrief; also run as a test so CI checks the data file end to end.
add_executable(procedure_demo examples/procedure_demo.cpp)
simcore_configure_target(procedure_demo)
add_test(NAME procedure_demo COMMAND procedure_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/procedures/osa_ak.json")
