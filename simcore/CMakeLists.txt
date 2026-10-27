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
    # /MP: the test files compile side by side (one after another, a header change costs a long rebuild).
    target_compile_options(${tgt} PRIVATE /W4 /permissive- /utf-8 /MP /bigobj)
    if(SIMCORE_STRICT)
      target_compile_options(${tgt} PRIVATE /WX /GR-)
    endif()
  else()
    # Optional trailing struct fields (ControlDef::guard_id, ProcedureStep::wait_for...) have defaults and are
    # meant to be left out of brace-initializers, which -Wmissing-field-initializers (in -Wextra) would reject.
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
                                          -Wno-missing-field-initializers)
    # GCC 13's -Wdangling-reference takes the loaders' loops over braced lists of std::make_pair("key", &member) for
    # references to temporaries (the pointers passed to make_pair); nothing dangles there, so it is off for GCC.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
      target_compile_options(${tgt} PRIVATE -Wno-dangling-reference)
    endif()
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
  tests/test_kill_assessment.cpp
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
  tests/test_em_deception.cpp
  tests/test_em_replication.cpp
  tests/test_crew_seats.cpp
  tests/test_scope_feed.cpp
  tests/test_battery.cpp
  tests/test_comms.cpp
  tests/test_voice_comms.cpp
  tests/test_fuel.cpp
  tests/test_orientation.cpp
  tests/test_radio_setup.cpp
  tests/test_threats.cpp
  tests/test_crew.cpp
  tests/test_crew_body.cpp
  tests/test_crew_mobility.cpp
  tests/test_crew_voice.cpp
  tests/test_crew_talk.cpp
  tests/test_crew_training.cpp
  tests/test_crew_systems.cpp
  tests/test_pu12m_console.cpp
  tests/test_radio_sets.cpp
  tests/test_command_post.cpp
  tests/test_pu12m_battery.cpp
  tests/test_pu12m_cabin.cpp
  tests/test_battery_link.cpp
  tests/test_missions.cpp
  tests/test_tutorial.cpp
  tests/test_helper.cpp
  tests/test_em_guidance.cpp
  tests/test_em_threats.cpp
  tests/test_em_arm.cpp
  tests/test_arm_drill.cpp
  tests/test_ranges_history.cpp
  tests/test_instructor.cpp
  tests/test_air_war.cpp
  tests/test_logistics.cpp
  tests/test_weapons.cpp
  tests/test_command.cpp
  tests/test_scenery.cpp
  tests/test_world_map.cpp
  tests/test_country_map.cpp)
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
add_executable(battery_link_demo examples/battery_link_demo.cpp)
simcore_configure_target(battery_link_demo)
add_test(NAME battery_link_demo
         COMMAND battery_link_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenarios/battery_link_practice.json"
                 "${CMAKE_CURRENT_SOURCE_DIR}/../data" battery_link.acmi)

add_executable(scenario_demo examples/scenario_demo.cpp)
simcore_configure_target(scenario_demo)
add_test(NAME scenario_demo
         COMMAND scenario_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenarios/osa_practice.json"
                 "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/osa_practice.acmi")

# The same practice scenario with the Osa's radars driven by the EM layer (own_unit.em).
add_test(NAME scenario_demo_em
         COMMAND scenario_demo "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenarios/osa_practice_em.json"
                 "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/osa_practice_em.acmi")

# The crew's bodies posed at rest and at work, as JSON for tools/blender/render_crew_poses.py (roadmap 8.10); also run
# as a test so the body data, the cabin and the poses stay loadable together.
add_executable(crew_pose_dump examples/crew_pose_dump.cpp)
simcore_configure_target(crew_pose_dump)
add_test(NAME crew_pose_dump
         COMMAND crew_pose_dump "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/crew_poses.json")

# Every mission played from every seat by a player who follows the helper, against the NPC crew (docs/crew/14_HELPER.md);
# writes docs/crew/generated/helper_coverage.md. Not a test: it takes minutes (test_helper.cpp checks a few).
add_executable(helper_coverage examples/helper_coverage.cpp)
simcore_configure_target(helper_coverage)

# The crew scenarios at every skill tier over many seeds, tabulated (roadmap 8.12; not a test: it takes minutes).
add_executable(crew_balance examples/crew_balance.cpp)
simcore_configure_target(crew_balance)

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

# The enemy air command over many seeds, every faction and two battery postures (roadmap A12,
# docs/air_war/09_BALANCE.md). Not a test: a table to read.
add_executable(air_war_balance examples/air_war_balance.cpp)
simcore_configure_target(air_war_balance)

add_executable(air_war_bench examples/air_war_bench.cpp)
simcore_configure_target(air_war_bench)

# The enemy air command (docs/air_war/): a campaign against one adversary, its raids through the battery's sector played
# as generated scenarios by the NPC crew; prints the enemy staff's journal. Also run as a test on two days of the Pact,
# and of NATO on the whole nation's theatre with its raids on the Kesmark map's ground (the nation on the map, N6).
add_executable(air_war_demo examples/air_war_demo.cpp)
simcore_configure_target(air_war_demo)
add_test(NAME air_war_demo
         COMMAND air_war_demo warsaw_pact 2 "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_BINARY_DIR}/air_war" 1)
add_test(NAME air_war_demo_nation
         COMMAND air_war_demo --theatre theatre_pannonia_nation.json nato 2 "${CMAKE_CURRENT_SOURCE_DIR}/../data"
                 "${CMAKE_CURRENT_BINARY_DIR}/air_war_nation" 1 maps/kesmark_range.json)

# The generated maps: a district's layout (data/maps/kesmark_range.json, docs/TEST_RANGE.md) and the whole country's
# (data/maps/pannonia.json, docs/COUNTRY_MAP.md), as JSON for tools/map_view.py; the country's also as routes over its
# roads, railways and pipelines with bridges cut.
add_executable(map_dump examples/map_dump.cpp)
simcore_configure_target(map_dump)
add_executable(country_dump examples/country_dump.cpp)
simcore_configure_target(country_dump)
add_test(NAME country_dump_route
         COMMAND country_dump "${CMAKE_CURRENT_SOURCE_DIR}/../data" maps/pannonia.json --route road 9 -1 68 12 b_ralje@rava)
