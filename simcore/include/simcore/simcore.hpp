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
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/world.hpp"
#include "simcore/acmi.hpp"
