#pragma once

// Single entry point for including the engine-agnostic simcore library in Unreal code.
// THIRD_PARTY_INCLUDES_* suppresses Unreal's stricter warnings for external headers.

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "simcore/simcore.hpp"
THIRD_PARTY_INCLUDES_END
