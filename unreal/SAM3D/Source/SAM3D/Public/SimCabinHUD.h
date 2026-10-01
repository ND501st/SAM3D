#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimCabinHUD.generated.h"

/**
 * Placeholder HUD for the 3D cabin: a crosshair, the name and state of the control under it, feedback from the
 * last action, and the key help. Draws nothing while the player's ASimOsaVehicle is outside the cabin.
 */
UCLASS()
class SAM3D_API ASimCabinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
