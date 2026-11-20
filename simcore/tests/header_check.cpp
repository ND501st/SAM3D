// Compiled with -fno-exceptions -fno-rtti to prove the headers are Unreal-compatible.
// Also defines the macros Unreal/Windows define, to catch identifier collisions early.
#define check(x) ((void)(x))
#define verify(x) ((void)(x))
#define ensure(x) (x)
#define PI 3.14159265f
#define TEXT(x) x
#define IN
#define OUT
#define ERROR 0
#define near
#define far
#define small char
#define interface struct

#include "simcore/simcore.hpp"
#include "simcore/air_radio_def.hpp"
#include "simcore/air_war/air_radio.hpp"
#include "simcore/air_war/abstract_radio.hpp"
#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/intercept.hpp"
#include "simcore/em/comms_intercept.hpp"

int simcore_header_check_symbol() {
    simcore::World world(simcore::GeoPoint::FromDegrees(0.0, 0.0, 0.0));
    return static_cast<int>(world.Time());
}
