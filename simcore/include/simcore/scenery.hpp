// simcore/scenery.hpp — what the game draws around the simulation (docs/scenery/): ultra-basic models for every aircraft,
// vehicle, site and building, every munition's shape as a mesh (bombs, missiles, rockets, the guns' rounds), the map's
// trees, buildings and bridges as meshes, and the register of third-party art that may replace them.
// Presentation only. (The map itself: simcore/terrain_map.hpp.)
#pragma once

#include "simcore/scenery/asset_register.hpp"
#include "simcore/scenery/basic_models.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/scenery/map_meshes.hpp"
#include "simcore/scenery/munitions.hpp"
#include "simcore/scenery/trimesh.hpp"
