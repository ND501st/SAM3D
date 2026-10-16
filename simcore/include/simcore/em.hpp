// simcore/em.hpp — umbrella header for the electromagnetic environment layer (simcore::em).
//
// Radar, passive receivers (RWR/ESM/ELINT/TDOA), jamming and chaff, radio nets and command posts,
// and per-unit knowledge kept apart from truth. See docs/EM_ARCHITECTURE.md.
#pragma once

#include "simcore/em/random.hpp"
#include "simcore/em/common.hpp"
#include "simcore/em/linalg.hpp"
#include "simcore/em/propagation.hpp"
#include "simcore/em/emitter.hpp"
#include "simcore/em/detection.hpp"
#include "simcore/em/geolocation.hpp"
#include "simcore/em/tracking.hpp"
#include "simcore/em/knowledge.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/scene_ops.hpp"
#include "simcore/em/radar_system.hpp"
#include "simcore/em/esm_system.hpp"
#include "simcore/em/knowledge_system.hpp"
#include "simcore/em/comms_system.hpp"
#include "simcore/em/doctrine.hpp"
#include "simcore/em/deception.hpp"
#include "simcore/em/debug_draw.hpp"
#include "simcore/em/world.hpp"
#include "simcore/em/data.hpp"
#include "simcore/em/demo_scenario.hpp"
