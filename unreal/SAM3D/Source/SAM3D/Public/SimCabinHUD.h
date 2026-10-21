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

	/** Show the PU-12 plotting board (toggled with B). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Board")
	bool bShowBoard = false;

	/** Radio lines shown at the bottom left, and how long one stays (sim seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Radio")
	int32 RadioLines = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Radio")
	float RadioLineSeconds = 40.f;

private:
	/** The radio lines, the newest at the bottom, ending above Bottom (clear of the PU-12M's aids). */
	void DrawRadio(class USimWorldSubsystem* Sim, class UFont* Font, float Bottom);
	/** Where the PU-12M's aids start at this seat (the radio lines stay above it); the screen's foot otherwise. */
	float PostAidsTop(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle) const;
	/** A faint dark backing for a block of text over the bright instruments. */
	void DrawBacking(float X, float Y, float W, float H);
	void DrawCrew(const class ASimOsaVehicle* Vehicle, class UFont* Font);
	void DrawBoard(class USimWorldSubsystem* Sim, class UFont* Font);
	void DrawPost(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle, class UFont* Font);
	/** Esc in a mission: resume, restart, the missions, quit; the debrief's summary once the scenario has ended. */
	void DrawMissionMenu(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
};
