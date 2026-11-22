// simcore/nav.hpp — the route service (roadmap G1, docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.1): mobility classes,
// the road graph built from the map's data, the ground off the road cell by cell (G1.3, nav/ground.hpp), routes between
// anchors, overlays true and believed; the march plan and the column on it (roadmap G2, nav/march.hpp); movers on routes
// and their levels of detail (G2.1, nav/movers.hpp); the carriers of the information pipeline on the roads (G2.5,
// nav/carriers.hpp); the movement table, regulators and exposure on the march (G3, nav/movement.hpp).
#pragma once

#include "simcore/nav/ground.hpp"
#include "simcore/nav/routes.hpp"
#include "simcore/nav/anchors.hpp"
#include "simcore/nav/march.hpp"
#include "simcore/nav/movers.hpp"
#include "simcore/nav/carriers.hpp"
#include "simcore/nav/movement.hpp"
#include "simcore/nav/session.hpp"
