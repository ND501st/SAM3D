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
  tests/test_console.cpp
  tests/test_scenario.cpp
  tests/test_engagement_model.cpp
  tests/test_cabin.cpp
  tests/test_scopes.cpp
  tests/test_em_models.cpp
  tests/test_em_data.cpp
  tests/test_em_scenarios.cpp
  tests/test_em_link.cpp
  tests/test_launcher.cpp
  tests/test_readiness.cpp
  tests/test_pu12.cpp
  tests/test_c2_sources.cpp
  tests/test_fuel.cpp)
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

# Play a scenario file headless and print the debrief; also run as a test on the practice scenario.
add_executable(scenario_demo examples/scenario_demo.cpp)
simcore_configure_target(scenario_demo)
add_test(NAME scenario_demo
         COMMAND scenario_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenarios/osa_practice.json"
                 "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/osa_practice.acmi")

# The same practice scenario with the Osa's radars driven by the EM layer (own_unit.em).
add_test(NAME scenario_demo_em
         COMMAND scenario_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenarios/osa_practice_em.json"
                 "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/osa_practice_em.acmi")

# Electromagnetic layer MVP: radar, ESM, triangulation, jamming, C2 on a synthetic battlefield.
# Prints the timeline and the eleven MVP checks; also run as a test.
add_executable(em_demo examples/em_demo.cpp)
simcore_configure_target(em_demo)
add_test(NAME em_demo
         COMMAND em_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/em_demo.json"
                 "${CMAKE_CURRENT_BINARY_DIR}/em_demo.acmi")

# Scaling benchmark for the electromagnetic layer (not a test: timing depends on the machine).
add_executable(em_bench examples/em_bench.cpp)
simcore_configure_target(em_bench)
