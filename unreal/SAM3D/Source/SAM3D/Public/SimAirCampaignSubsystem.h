#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "SimAirCampaignSubsystem.generated.h"

/** The player's battery's posture in the campaign (simcore::airwar::BatteryPosture), for the campaign screen. */
USTRUCT(BlueprintType)
struct FSimBatteryPosture
{
	GENERATED_BODY()

	/** Share of each hour the battery's radars radiate when not fighting (0: strict EMCON). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	float Emission = 0.3f;

	/** Switching off when ARMs come, brief bursts: 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	float EmconDiscipline = 0.5f;

	/** Little voice traffic: 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	float RadioDiscipline = 0.5f;

	/** Empty messages on the net to confuse the enemy's intercept service. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	bool bDummyTraffic = false;

	/** The dummies' decoy emitters radiate while the battery's radars are off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	bool bDecoyEmitters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	bool bNets = true;

	/** The battery's radars stay silent; the crews fight by the TV on the PU-12's designations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	bool bFightSilent = false;

	/** Hours between moves (0: the battery stays until ordered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|AirWar")
	float RelocateEveryH = 0.0f;
};

/**
 * The enemy air command's campaign (docs/air_war/, simcore/air_war/), kept by the game instance so that it lives on
 * while levels reload for each raid.
 *
 * Flow: the missions menu starts a campaign against one adversary (NewCampaign) and opens the level with ?SimCampaign,
 * where ASimCampaignHUD shows the campaign screen. AdvanceToNextRaid runs the war until a raid comes through the
 * battery's sector. FightPendingRaid writes the raid's scenario to Saved/AirWar/ and reopens the level with
 * ?SimAirWarRaid=<id>&SimSeat=<seat>: ASimScenarioDirector plays it like any mission. While it plays, this subsystem
 * steps the mission bridge after the world subsystem's simulation (it observes the runner; it never steps it); when the
 * scenario ends it hands the outcome to the campaign and returns to the campaign screen. A raid left early (the player
 * quits to the menu) is finished at the world's cleanup with what happened so far. LetPendingRaidPass resolves it
 * abstractly.
 *
 * Standalone games only (as the missions menu): in a networked game the server would own the campaign (not yet).
 */
UCLASS()
class SAM3D_API USimAirCampaignSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	USimAirCampaignSubsystem();

	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Start a campaign against one adversary: "rebels", "neutral", "warsaw_pact" or "nato" (data/air_war/faction_<id>.json). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool NewCampaign(const FString& FactionId, int32 Seed, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	bool HasCampaign() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetFactionId() const { return FactionId; }

	/** Run the war until a raid comes through the battery's sector, or MaxHours pass. True if a raid is waiting. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool AdvanceToNextRaid(float MaxHours = 24.0f);

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	bool HasPendingRaid() const;

	/** What our side knows of the waiting raid (the warning), never the enemy's reasons. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetPendingRaidWarning() const;

	/** Fight the waiting raid from that seat ("" = outside): writes its scenario and reopens the level. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool FightPendingRaid(UObject* WorldContext, const FString& Seat, FString& OutError);

	/** Let the waiting raid be fought without the player (resolved abstractly). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	void LetPendingRaidPass();

	/** A raid scenario is being played now. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	bool IsRaidActive() const { return bRaidActive; }

	/** The active raid's scenario, relative to the data folder (for ASimScenarioDirector). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetRaidScenarioFile() const { return RaidScenarioFile; }

	/** What the last raid came to, for the campaign screen. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetLastRaidReport() const { return LastRaidReport; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FSimBatteryPosture GetPosture() const;

	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	void SetPosture(const FSimBatteryPosture& Posture);

	/** Move the battery to (east, north) km in the theatre frame; false (and why) if it can't go there. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool RelocateBattery(FVector2D EastNorthKm, FString& OutWhy);

	/** Write the campaign to Saved/AirWar/campaign.json (done after every step of it). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool SaveCampaign();

	/** A saved campaign exists (what it is: GetSavedCampaignLabel). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	bool HasSavedCampaign() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetSavedCampaignLabel() const;

	/** Load the saved campaign (its seed and actions replayed). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	bool LoadSavedCampaign(FString& OutError);

	/** The save file's path. */
	static FString SaveFile();

	/** End the campaign: the enemy's staff is revealed on the campaign screen. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	void EndCampaign()
	{
		bEnded = true;
		Stepped();
	}

	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	bool IsEnded() const { return bEnded; }

	/** Reopen the current level on the campaign screen. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|AirWar")
	static void OpenCampaignScreen(UObject* WorldContext);

	/** The level options that play the active raid from a seat. */
	static FString RaidOptions(int32 PackageId, const FString& Seat);

	/** Write a raid of a campaign as a scenario file in Saved/AirWar/ and give its path relative to the data folder. With
	 *  Deliveries, the chain of command's messages to the session's seats are in it, and its warning comes when the
	 *  pipeline brings it (roadmap P1); without, the warning net's warning at the builder's own time. */
	static bool WriteRaidScenario(const simcore::airwar::AirCampaign& Campaign, const simcore::airwar::Package& Package,
		const FString& DataDir, FString& OutRelativeFile, simcore::airwar::RaidManifest& OutManifest, FString& OutError,
		const simcore::DeliverySource* Deliveries = nullptr);

	/** The campaign itself, for the campaign screen (C++ only). */
	const simcore::airwar::AirCampaign* GetCampaign() const { return Campaign.Get(); }

	/** The battery's march to (east, north) km on the country's roads, in hours from the order to move off to its head
	 *  there (roadmap G9.5); < 0 when there is no way or no roads (and why). C++ only. */
	double MarchHoursTo(FVector2D EastNorthKm, FString* OutWhy = nullptr);

	/** Our side's information pipeline (docs/command/): what has reached the player's character, from whom and how.
	 *  Null if the chain of command's data did not load. C++ only. */
	const simcore::command::InfoNet* GetInfoNet() const { return Net.Get(); }

	/** Our side's picture of the front as the battery's regiment holds it (track F, F5.3; simcore/command/campaign_situation.hpp):
	 *  the line of contact as last reported, our units and the enemy as reported, the air defence staff's conclusion on the
	 *  enemy's air front if one reached the regiment, each mark with its date and age. Built from the pipeline's facts and the
	 *  feed's records only, never from the war's state. Null with the fronts off (the screen then draws what it always did) or
	 *  without the pipeline. Valid until the campaign next steps. C++ only. */
	const simcore::command::SituationMap* GetRegimentSituation();

	/** The post the player's character holds in the chain of command (data/command/chain_pannonia.json). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|AirWar")
	FString GetPlayerPost() const;

	/** The player's character tells someone what he knows (roadmap P2, docs/command/06_INTEGRATION.md section 6): Fact and
	 *  To are the pipeline's indices, Mean a means' id. False (and why) if he can't. Kept with the campaign. C++ only. */
	bool TellFact(int32 Fact, int32 To, const FString& Mean, FString& OutWhy);

	/** An order of the player's own (the battery commander's: the posture he set), which reaches the vehicle commanders
	 *  only as he tells it (roadmap P2). Its fact index; -1 none. C++ only. */
	int32 GiveOrder(const FString& Text, const FString& Summary);

	/** Where the player's tellings and reports are kept beside the campaign. */
	static FString PipelineSaveFile();

	/** The data folder (the world subsystem's resolution: the repo's data/). */
	static FString DataDirectory();

private:
	void FinishRaid(simcore::ScenarioRunner& Runner, const TCHAR* Why);
	void HandleWorldTearDown(UWorld* World);
	/** Build the information pipeline for the campaign (and bring it up to the campaign's hour); with bLoadInputs, put
	 *  back the player's tellings and reports from the save. */
	void StartInfoNet(bool bLoadInputs = false);
	/** After every step of the campaign: the pipeline catches up, and the campaign is saved. */
	void Stepped();
	/** The country's roads, loaded on first use (G2.5, G9.5); false when they don't load. */
	bool EnsureRoads();

	TUniquePtr<simcore::airwar::AirCampaign> Campaign;
	TUniquePtr<simcore::airwar::RaidManifest> Manifest;
	TUniquePtr<simcore::airwar::MissionBridge> Bridge;
	TUniquePtr<simcore::command::InfoNet> Net;
	TUniquePtr<simcore::command::CampaignFeed> Feed;
	/** The regiment's situation map for the campaign screen (F5.3), rebuilt only when the pipeline's facts change. */
	simcore::command::CampaignSituation Situation;
	/** The country's roads and the battery's march tactics for its moves to a new position (G9.5; loaded on first use). */
	TUniquePtr<simcore::nav::SessionNav> Roads;
	TUniquePtr<simcore::BatteryTactics> RoadTactics;
	bool bRoadsTried = false;
	FString FactionId;
	/** The theatre the campaign is fought on (relative to data/): a new campaign the whole nation's (the nation on the map
	 *  N4, rebalanced in docs/air_war/09_BALANCE.md section 8); a saved one the theatre it was saved with. */
	FString TheatreFile = TEXT("air_war/theatre_pannonia_nation.json");
	int32 CampaignSeed = 1;
	bool bRaidActive = false;
	bool bBridgeStarted = false;
	bool bEnded = false;
	FString RaidScenarioFile;
	FString LastRaidReport;
	/** Real seconds left before returning to the campaign screen after a raid ended (< 0: not counting). */
	float ReturnCountdown = -1.0f;
	TWeakObjectPtr<UWorld> RaidWorld;
	FDelegateHandle CleanupHandle;
};
