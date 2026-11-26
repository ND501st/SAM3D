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
 * Keys: Up/Down (W/S) a mission, Left/Right (A/D) where to start, H the helper (off, hints, on: docs/crew/14_HELPER.md,
 * kept in GameUserSettings.ini for every mission), T (Shift+T back) the time of day and , . the hour in "An hour"
 * (roadmap R7: the mission's own, dawn, day, dusk, night or an hour, by the scenario's own sun; a mission whose entry
 * holds its time offers only its own: ?SimTime=<preset>[?SimHour=<h>]), Enter to choose who to play, Q to quit; a click
 * picks a mission, turns the helper or the time of day, or presses Start. Enter or Start opens the crew: every crewman of the mission's vehicles with his duties
 * (Up/Down or 1-9 one, Enter or a second click plays him, Esc back to the missions).
 *
 * The campaign (track O, O1; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 9): 1-4 a new campaign against
 * that enemy (data/campaign/campaigns.json), its mission's crew to choose from, then ?SimWar=new:<id>?SimSeat=<station>;
 * C the saved one (?SimWar=continue). Shift+1-4 and Shift+C open the air war's developer screen instead.
 *
 * Scripted check: -SimMenuTest=<folder> photographs the menu (menu.png), then -SimMenuPick=<mission>:<vehicle>:<seat>
 * (or campaign:<adversary>:<seat>) picks and photographs (menu_pick.png) and starts it; without a pick it quits.
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
	static FString OptionsFor(const FString& MissionId, const FString& VehicleId, const FString& SeatId, const FString& Tutorial = FString(),
		const FString& TimeOptions = FString());

	/** The time of day's level options for a mission (roadmap R7): "?SimTime=dusk", "?SimTime=hour?SimHour=17.50", or empty
	 *  for the mission's own time and always for a mission that holds its time (the menu's hold). */
	static FString TimeOptionsFor(const FSimMission& Mission, int32 TimeIndex, float Hour);

	const TArray<FSimMission>& GetMissions() const { return Missions; }

private:
	struct FStartChoice
	{
		FString Vehicle;
		FString Seat;
		FString Label;
		FString Title;  // the crewman (the crew data's title), "" outside
		TArray<FString> Duties;
		bool bCommands = false;
	};
	TArray<FStartChoice> StartChoices(const FSimMission& Mission) const;
	int32 DefaultStart(const FSimMission& Mission) const;
	void MoveSelection(int32 Step);
	float DrawWrapped(const FString& Text, float X, float Y, float Width, UFont* Font, const FLinearColor& Colour, float LineGap = 2.0f);
	void HandleInput();
	/** The crew of the selected mission: choose who to play, then start. */
	void OpenCrewChoice();
	void HandleCrewChoiceInput(APlayerController* Pc);
	void DrawCrewChoice(float W, float H);
	bool bChoosingCrew = false;
	TArray<FBox2D> CrewBoxes;
	FBox2D CrewPlayBox = FBox2D(ForceInit);
	FBox2D CrewBackBox = FBox2D(ForceInit);
	/** The air war's developer screen (Shift+1-4): start a campaign against adversary 0-3 (rebels, neutral, Warsaw Pact,
	 *  NATO) and open its screen. */
	void StartCampaign(int32 Index);
	/** The developer screen's Shift+C: continue the campaign under way, or the saved one. */
	void ContinueCampaign();
	/** The game's campaign (track O, O1; 1-4): a new one against CampaignChoices[Index], its mission's crew to choose
	 *  from first, then ?SimWar=new:<id>. */
	void NewCampaign(int32 Index);
	/** C: the saved campaign, in its seat (?SimWar=continue). */
	void ContinueSavedCampaign();
	void DrawCampaignBox(float X, float Y, float Width);
	TArray<FSimCampaignChoice> CampaignChoices;
	/** While the crew is chosen for a new campaign: its ?SimWar and the line shown over the crew. */
	FString CampaignWar;
	FString CampaignLabel;
	/** Why a campaign key did nothing (not yet playable, nothing saved). */
	FString CampaignNote;
	bool bHasSavedCampaign = false;
	FString SavedCampaignLabel;
	FString SavedCampaignAdversary;
	FString SavedCampaignSeat;
	void RunScriptedCheck(float DeltaSeconds);

	TArray<FSimMission> Missions;
	FString LoadError;
	int32 Selected = 0;
	int32 StartIndex = 0;
	/** Where each mission's row and the Start button were drawn, for the mouse. */
	TArray<FBox2D> RowBoxes;
	FBox2D StartBox = FBox2D(ForceInit);
	/** The helper's line (a click turns it). */
	FBox2D HelperBox = FBox2D(ForceInit);
	void CycleHelper();
	/** The time of day: the index into the mission's TimeOptions, the hour for "hour", and where its line was drawn. */
	int32 TimeIndex = 0;
	float TimeHour = 10.5f;
	FBox2D TimeBox = FBox2D(ForceInit);
	void ResetTime();
	void CycleTime(int32 Step);
	FString TimeLabel() const;
	/** The air war box: the four adversaries and "continue". */
	TArray<FBox2D> CampaignBoxes;

	FString TestFolder;
	FString TestPick;
	float TestClock = 0.0f;
	int32 TestStep = 0;
};
