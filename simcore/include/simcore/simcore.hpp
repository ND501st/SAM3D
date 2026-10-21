// simcore/simcore.hpp — umbrella header for the SAM3D simulation core.
//
// simcore is header-only, engine-agnostic C++17:
//   * no exceptions, no RTTI (Unreal builds with both disabled)
//   * no dependencies beyond the standard library
//   * SI units, local ENU frame (x=East, y=North, z=Up)
#pragma once

#define SIMCORE_VERSION_MAJOR 0
#define SIMCORE_VERSION_MINOR 1
#define SIMCORE_VERSION_PATCH 0

#include "simcore/vec3.hpp"
#include "simcore/geo.hpp"
#include "simcore/atmosphere.hpp"
#include "simcore/terrain.hpp"
#include "simcore/radar.hpp"
#include "simcore/aircraft.hpp"
#include "simcore/missile.hpp"
#include "simcore/panel.hpp"
#include "simcore/equipment.hpp"
#include "simcore/procedures.hpp"
#include "simcore/json.hpp"
#include "simcore/console_data.hpp"
#include "simcore/debrief.hpp"
#include "simcore/engagement.hpp"
#include "simcore/kill_assessment.hpp"
#include "simcore/group_designation.hpp"
#include "simcore/orientation.hpp"
#include "simcore/crew_audio.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/command_post.hpp"
#include "simcore/radio_sets.hpp"
#include "simcore/cabin.hpp"
#include "simcore/scopes.hpp"
#include "simcore/readouts.hpp"
#include "simcore/world.hpp"
#include "simcore/acmi.hpp"
#include "simcore/em.hpp"
#include "simcore/scope_feed.hpp"
#include "simcore/post_feed.hpp"
#include "simcore/board_reading.hpp"
#include "simcore/pvo_grid.hpp"
#include "simcore/battery_link.hpp"
#include "simcore/console_sync.hpp"
#include "simcore/crew_body.hpp"
#include "simcore/crew_mobility.hpp"
#include "simcore/crew_voice.hpp"
#include "simcore/missions.hpp"
#include "simcore/tutorial.hpp"
#include "simcore/seat_duty.hpp"
#include "simcore/helper.hpp"
#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/noise.hpp"
#include "simcore/weather.hpp"
#include "simcore/weather_data.hpp"
#include "simcore/terrain_map.hpp"
#include "simcore/solar.hpp"
#include "simcore/scenery.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/talk_tables.hpp"
#include "simcore/voice_comms.hpp"
