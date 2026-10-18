#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimCabinHUD.generated.h"

/**
 * The player's HUD: in the cabin a crosshair, the name and state of the control under it, feedback from the last action
 * and the key help; outside, the camera view's name (F1-F11). Everywhere the radio lines, the time compression, the
 * tutorial guide's step (N skips it) and the keys card (G).
 */
UCLASS()
class SAM3D_API ASimCabinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Show every key (toggled with G). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Guide")
	bool bShowKeys = false;

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
	/** The camera view's name, the view's last note and the time compression, at the top. */
	void DrawView(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
	/** The tutorial guide's step: a panel at the top right (the left while the board is up). */
	void DrawGuide(class USimWorldSubsystem* Sim);
	/** Every key, by what it is for (G). */
	void DrawKeys(const class ASimOsaVehicle* Vehicle);
	/** Text broken into lines no wider than Width. */
	TArray<FString> Wrap(const FString& Text, float Width, class UFont* Font);
	/** Esc in a mission: resume, restart, the missions, quit; the debrief's summary once the scenario has ended. */
	void DrawMissionMenu(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
};
