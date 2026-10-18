#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimWorldSubsystem.h"
#include "SimMissionMenuHUD.generated.h"

/**
 * The missions menu (roadmap M2: a person plays a mission without touching the editor). Lists the missions of
 * data/scenarios/missions.json by group; the selected one shows its scenario's name and description and where the player
 * starts: the vehicle (a battery link) and the seat, the commander's being the game's "in command" half and any other
 * "in the crew". Starting reopens the level with ?SimMission=<id>[?SimVehicle=post][?SimSeat=<station>], which
 * ASimScenarioDirector plays.
 *
 * Keys: Up/Down (W/S) a mission, Left/Right (A/D) where to start, Enter to start, Q to quit; a click picks a mission or
 * presses Start.
 *
 * Scripted check: -SimMenuTest=<folder> photographs the menu (menu.png), then -SimMenuPick=<mission>:<vehicle>:<seat>
 * picks and photographs (menu_pick.png) and starts it; without a pick it quits.
 */
UCLASS()
class SAM3D_API ASimMissionMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASimMissionMenuHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

	/** Select a mission and where to start in it; false if there is no such mission, vehicle or seat. */
	bool Select(const FString& MissionId, const FString& VehicleId, const FString& SeatId);

	/** Reopen the level with the selected mission. */
	void StartSelected();

	/** The level options that start a mission in that vehicle and seat (seat "" = outside the vehicle), with its tutorial
	 *  guide if it has one (?SimTutorial). */
	static FString OptionsFor(const FString& MissionId, const FString& VehicleId, const FString& SeatId, const FString& Tutorial = FString());

	const TArray<FSimMission>& GetMissions() const { return Missions; }

private:
	struct FStartChoice
	{
		FString Vehicle;
		FString Seat;
		FString Label;
	};
	TArray<FStartChoice> StartChoices(const FSimMission& Mission) const;
	int32 DefaultStart(const FSimMission& Mission) const;
	void MoveSelection(int32 Step);
	float DrawWrapped(const FString& Text, float X, float Y, float Width, UFont* Font, const FLinearColor& Colour, float LineGap = 2.0f);
	void HandleInput();
	/** The air war: start a campaign against adversary 0-3 (rebels, neutral, Warsaw Pact, NATO) and open its screen. */
	void StartCampaign(int32 Index);
	/** Continue the campaign under way, or the saved one. */
	void ContinueCampaign();
	void DrawCampaignBox(float X, float Y, float Width);
	void RunScriptedCheck(float DeltaSeconds);

	TArray<FSimMission> Missions;
	FString LoadError;
	int32 Selected = 0;
	int32 StartIndex = 0;
	/** Where each mission's row and the Start button were drawn, for the mouse. */
	TArray<FBox2D> RowBoxes;
	FBox2D StartBox = FBox2D(ForceInit);
	/** The air war box: the four adversaries and "continue". */
	TArray<FBox2D> CampaignBoxes;

	FString TestFolder;
	FString TestPick;
	float TestClock = 0.0f;
	int32 TestStep = 0;
};
