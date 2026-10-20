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

	/** The lesson card's lower edge this frame (0: no card); the crew menu opens below it. */
	float GuideBottom = 0.0f;

	/** Radio lines shown at the bottom left, and how long one stays (sim seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Radio")
	int32 RadioLines = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Radio")
	float RadioLineSeconds = 40.f;

	/** The air-picture net's reports fade much sooner: the plotter writes from what he heard, not from the screen. */
	UPROPERTY(EditAnywhere, Category = "SAM3D|HUD")
	float AirPictureLineSeconds = 6.f;

private:
	/** The radio lines, the newest at the bottom, ending above Bottom (clear of the PU-12M's aids). */
	void DrawRadio(class USimWorldSubsystem* Sim, class UFont* Font, float Bottom, const FString& Station);
	/** The player's own voice paths, bottom left (his headset, his box, its lamps, a call): returns the height drawn. */
	float DrawComms(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle, class UFont* Font, float Bottom);
	/** Where the PU-12M's aids start at this seat (the radio lines stay above it); the screen's foot otherwise. */
	float PostAidsTop(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle) const;
	/** A faint dark backing for a block of text over the bright instruments. */
	void DrawBacking(float X, float Y, float W, float H);
	void DrawCrew(const class ASimOsaVehicle* Vehicle, class UFont* Font);
	void DrawBoard(class USimWorldSubsystem* Sim, class UFont* Font);
	void DrawPost(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle, class UFont* Font);
	/** The camera view's name, the view's last note and the time compression, at the top. */
	void DrawView(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
	/** The lesson's step card (docs/research/TUTORIAL_DESIGN_2026-10.md): where it is, the action in one line, the
	 *  control and its position, what for, and a status line that always says what happens next. Top right (the left
	 *  while the board is up). */
	void DrawGuide(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
	/** The step's control on the screen: a bracket round it with the position to set beside it, or an arrow at the
	 *  screen's edge pointing to it when it is out of view. */
	void DrawLessonMarker(const struct FSimTutorialStep& Step, const class ASimOsaVehicle* Vehicle);
	/** A wheel step: where the wheel is against the mark, and which way to turn it. Its height; drawn only with bDraw.
	 *  S is the card's size factor, TextScale its words'. */
	float DrawWheelGauge(const struct FSimTutorialStep& Step, float X, float Y, float Width, float S, float TextScale, bool bDraw);
	/** A tick, drawn (the engine's font has no glyph for one); its box is Size square from the top left. */
	void DrawTick(float X, float Y, float Size, const FLinearColor& Color);
	/** An arrowhead pointing along Dir, its tip at Tip. */
	void DrawArrowhead(FVector2f Tip, FVector2f Dir, float Size, const FLinearColor& Color, float Thickness);
	/** Every key, by what it is for (G). */
	void DrawKeys(const class ASimOsaVehicle* Vehicle);
	/** Text broken into lines no wider than Width. */
	TArray<FString> Wrap(const FString& Text, float Width, class UFont* Font, float Scale = 1.0f);
	/** Esc in a mission: resume, restart, the missions, quit; the debrief's summary once the scenario has ended. */
	void DrawMissionMenu(class USimWorldSubsystem* Sim, const class ASimOsaVehicle* Vehicle);
};
