#include "SimScenarioDirector.h"

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SAM3D.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "SimAirCampaignSubsystem.h"
#include "SimCampaignHUD.h"
#include "SimEntityActor.h"
#include "SimMissionMenuHUD.h"
#include "SimModelGallery.h"
#include "SimOsaVehicle.h"
#include "UnrealClient.h"

ASimScenarioDirector::ASimScenarioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	AircraftActorClass = ASimEntityActor::StaticClass();
	MissileActorClass = ASimEntityActor::StaticClass();
	PlayerVehicleClass = ASimOsaVehicle::StaticClass();
}

void ASimScenarioDirector::BeginPlay()
{
	Super::BeginPlay();

	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	if (Sim == nullptr)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("SimWorldSubsystem not available in this world type"));
		return;
	}
	Sim->OnSimEvent.AddDynamic(this, &ASimScenarioDirector::HandleSimEvent);
	Sim->OnDebriefEntry.AddDynamic(this, &ASimScenarioDirector::HandleDebriefEntry);
	// The basic models' checks: -SimModelGallery=<folder>, -SimShots=<folder> (docs/scenery/README.md).
	ASimModelGallery::StartIfAsked(GetWorld());

	// ?SimMenu (the mission's menu: "the missions") opens the missions menu whatever else is asked.
	if (Sim->HasUrlOption(TEXT("SimMenu")))
	{
		ShowMissionMenu();
		return;
	}
	// The air war (docs/air_war/): ?SimCampaign shows the campaign screen; ?SimAirWarRaid=<id> plays the raid the campaign
	// wrote for it (Saved/AirWar/), from ?SimSeat. A raid the campaign no longer waits for goes back to the screen.
	USimAirCampaignSubsystem* AirWar = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
	if (Sim->HasUrlOption(TEXT("SimCampaign")) || (Sim->HasUrlOption(TEXT("SimAirWarRaid")) && (AirWar == nullptr || !AirWar->IsRaidActive())))
	{
		ShowCampaignScreen();
		return;
	}
	const bool bAirWarRaid = Sim->HasUrlOption(TEXT("SimAirWarRaid")) && AirWar != nullptr && AirWar->IsRaidActive();
	// ?SimMission=<id> (a mission chosen in the menu) plays scenarios/<id>.json, in ?SimVehicle (a battery link) and
	// ?SimSeat. -SimScenario=scenarios/<file>.json plays that scenario instead of the level's (a server or a scripted
	// run). Either way with its own vehicle's cabin.
	const FString Mission = Sim->HasUrlOption(TEXT("SimMission")) ? Sim->GetLaunchOption(TEXT("SimMission")) : FString();
	FString CommandLineScenario;
	const bool bValidMission = !Mission.IsEmpty() && !Mission.Contains(TEXT("/")) && !Mission.Contains(TEXT("\\")) && !Mission.Contains(TEXT(".."));
	if (bAirWarRaid)
	{
		UE_LOG(LogSAM3D, Log, TEXT("Air war raid: %s"), *AirWar->GetRaidScenarioFile());
		ScenarioFile = AirWar->GetRaidScenarioFile();
		CabinFile.Reset();
		bShowEventsOnScreen = false;
		for (const FString& Op : GetWorld()->URL.Op)
		{
			if (Op.StartsWith(TEXT("SimSeat="), ESearchCase::IgnoreCase))
			{
				StartSeat = Op.Mid(8);
			}
		}
	}
	else if (bValidMission)
	{
		UE_LOG(LogSAM3D, Log, TEXT("Mission from the menu: %s"), *Mission);
		ScenarioFile = FString::Printf(TEXT("scenarios/%s.json"), *Mission);
		CabinFile.Reset();
		bShowEventsOnScreen = false;  // played, not debugged: the HUD's radio lines and the debrief at the end, no engineering lines
		for (const FString& Op : GetWorld()->URL.Op)
		{
			if (Op.StartsWith(TEXT("SimSeat="), ESearchCase::IgnoreCase))
			{
				StartSeat = Op.Mid(8);
			}
		}
		FParse::Value(FCommandLine::Get(), TEXT("SimMenuTest="), MenuTestFolder);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("SimScenario="), CommandLineScenario) && !CommandLineScenario.IsEmpty())
	{
		UE_LOG(LogSAM3D, Log, TEXT("Scenario from the command line: %s"), *CommandLineScenario);
		ScenarioFile = CommandLineScenario;
		CabinFile.Reset();
	}
	else if (ShouldShowMissionMenu())
	{
		ShowMissionMenu();
		return;
	}

	if (!ScenarioFile.IsEmpty())
	{
		StartScenario(Sim);
		return;
	}
	Sim->ResetSimulation(OriginLatitudeDeg, OriginLongitudeDeg, OriginAltitudeM);
	if (bStartDemoOnBeginPlay)
	{
		Sim->StartDemoScenario();
	}
}

bool ASimScenarioDirector::ShouldShowMissionMenu() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() != NM_Standalone)
	{
		return false;  // a server plays its scenario; a client its server's
	}
	return MissionMenu == ESimMissionMenu::Always || (MissionMenu == ESimMissionMenu::StandaloneGame && World->WorldType == EWorldType::Game);
}

void ASimScenarioDirector::ShowMissionMenu()
{
	bMissionMenuShown = true;
	APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0);
	if (Pc == nullptr)
	{
		return;
	}
	Pc->ClientSetHUD(ASimMissionMenuHUD::StaticClass());
	Pc->SetShowMouseCursor(true);
	Pc->SetIgnoreMoveInput(true);
	Pc->SetIgnoreLookInput(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Pc->SetInputMode(Mode);
	UE_LOG(LogSAM3D, Log, TEXT("Missions menu shown"));
}

void ASimScenarioDirector::ShowCampaignScreen()
{
	bMissionMenuShown = true;
	APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0);
	if (Pc == nullptr)
	{
		return;
	}
	Pc->ClientSetHUD(ASimCampaignHUD::StaticClass());
	Pc->SetShowMouseCursor(true);
	Pc->SetIgnoreMoveInput(true);
	Pc->SetIgnoreLookInput(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Pc->SetInputMode(Mode);
	UE_LOG(LogSAM3D, Log, TEXT("Air war: campaign screen shown"));
}

void ASimScenarioDirector::StartScenario(USimWorldSubsystem* Sim)
{
	FString Error;
	if (!Sim->LoadScenario(ScenarioFile, Error))
	{
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("Scenario failed to load: %s"), *Error));
		}
		return;
	}
	const FString Cabin = CabinFile.IsEmpty() ? Sim->GetDefaultCabinFile() : (CabinFile == TEXT("none") ? FString() : CabinFile);
	if (!Cabin.IsEmpty() && !Sim->LoadCabin(Cabin, Error) && GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("Cabin failed to load: %s"), *Error));
	}
	if (bUseCesiumGeoreference)
	{
		if (ACesiumGeoreference* Geo = ACesiumGeoreference::GetDefaultGeoreferenceForActor(this))
		{
			// Put the Unreal origin on the scenario origin, so the sim's local frame and Cesium agree.
			Geo->SetOriginLongitudeLatitudeHeight(Sim->GetOriginLongitudeLatitudeHeight());
			Sim->SetGeoreference(Geo);
		}
	}
	// ?SimTutorial=tutorials/<file>.json (the missions menu's tutorial) or -SimTutorial=: the guide over the scenario.
	const FString Guide = Sim->GetLaunchOption(TEXT("SimTutorial"));
	if (!Guide.IsEmpty())
	{
		if (!Guide.StartsWith(TEXT("tutorials/")) || Guide.Contains(TEXT("..")) || Guide.Contains(TEXT("\\")))
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Tutorial '%s' refused: a guide is tutorials/<file>.json"), *Guide);
		}
		else if (!Sim->LoadTutorial(Guide, Error) && GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("Tutorial failed to load: %s"), *Error));
		}
	}
	if (bSpawnPlayerVehicle)
	{
		SpawnPlayerVehicle(Sim);
	}
}

void ASimScenarioDirector::SpawnPlayerVehicle(USimWorldSubsystem* Sim)
{
	UClass* Class = PlayerVehicleClass ? PlayerVehicleClass.Get() : ASimOsaVehicle::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// At the scenario's position, facing its true heading (converted: with Cesium, Unreal X isn't north).
	const FRotator Facing(0.0, Sim->HeadingToYawDeg(Sim->GetUnitLocation(), Sim->GetUnitHeading()), 0.0);
	ASimOsaVehicle* Vehicle = GetWorld()->SpawnActor<ASimOsaVehicle>(Class, Sim->GetUnitLocation(), Facing, Params);
	APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0);
	if (Vehicle != nullptr && Pc != nullptr)
	{
		// The game mode's stand-in (a floating sphere) is left behind when the player takes the vehicle: remove it.
		APawn* StandIn = Pc->GetPawn();
		Pc->Possess(Vehicle);
		if (StandIn != nullptr && StandIn != Vehicle && StandIn->IsA<ADefaultPawn>())
		{
			StandIn->Destroy();
		}
	}
	PlayerVehicle = Vehicle;
	if (Vehicle != nullptr && !StartSeat.IsEmpty())
	{
		Vehicle->StartInSeat(StartSeat);
	}
	// -SimCabinShots=<folder>: photograph every seat and instrument of the cabin (after -SimShotDelay= seconds, default
	// 20), then quit. With -game -RenderOffscreen, a run checked from its pictures alone.
	FString ShotFolder;
	if (Vehicle != nullptr && FParse::Value(FCommandLine::Get(), TEXT("SimCabinShots="), ShotFolder) && !ShotFolder.IsEmpty())
	{
		float Delay = 20.0f;
		FParse::Value(FCommandLine::Get(), TEXT("SimShotDelay="), Delay);
		Vehicle->StartCabinShots(ShotFolder, Delay);
		return;
	}
	const FString TourEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("SAM3D_CABIN_TOUR"));
	if (Vehicle != nullptr && (bCabinTour || !TourEnv.IsEmpty()))
	{
		const float Seconds = TourEnv.IsEmpty() ? CabinTourSecondsPerView : FCString::Atof(*TourEnv);
		Vehicle->StartCabinTour(Seconds > 0.0f ? Seconds : CabinTourSecondsPerView);
	}
}

void ASimScenarioDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* MyWorld = GetWorld())
	{
		if (USimWorldSubsystem* Sim = MyWorld->GetSubsystem<USimWorldSubsystem>())
		{
			Sim->OnSimEvent.RemoveDynamic(this, &ASimScenarioDirector::HandleSimEvent);
			Sim->OnDebriefEntry.RemoveDynamic(this, &ASimScenarioDirector::HandleDebriefEntry);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASimScenarioDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TimeSinceBeginPlay += DeltaSeconds;
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	if (Sim == nullptr)
	{
		return;
	}
	if (!bTerrainRequested && TerrainTileset != nullptr && Sim->IsScenarioRunning() &&
		TimeSinceBeginPlay >= TerrainSampleDelaySeconds)
	{
		bTerrainRequested = Sim->RequestCesiumTerrain(TerrainTileset, TerrainHalfSizeMeters, TerrainSpacingMeters);
	}
	SpawnMissingEntityActors();
	ShowStatus(Sim);
	if (!MenuTestFolder.IsEmpty())
	{
		RunMenuCheck(DeltaSeconds);
	}
}

void ASimScenarioDirector::RunMenuCheck(float DeltaSeconds)
{
	// The mission the menu check started: the player's first view in his seat, then the mission's menu, then quit.
	ASimOsaVehicle* Vehicle = PlayerVehicle.Get();
	if (MenuTestStep == 0 && TimeSinceBeginPlay >= 25.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(MenuTestFolder, TEXT("started.png")), false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Missions menu check: started.png, the player %s"),
			Vehicle != nullptr && Vehicle->IsHoldingSeat() ? *FString::Printf(TEXT("in the %s's seat"), *Vehicle->GetStationId()) : TEXT("in no seat"));
		MenuTestStep = 1;
	}
	else if (MenuTestStep == 1 && TimeSinceBeginPlay >= 27.0f)
	{
		if (Vehicle != nullptr)
		{
			Vehicle->SetMissionMenuOpen(true);
		}
		MenuTestStep = 2;
	}
	else if (MenuTestStep == 2 && TimeSinceBeginPlay >= 29.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(MenuTestFolder, TEXT("mission_menu.png")), false, false);
		MenuTestStep = 3;
	}
	else if (MenuTestStep == 3 && TimeSinceBeginPlay >= 31.0f)
	{
		UE_LOG(LogSAM3D, Log, TEXT("Missions menu check: done"));
		FGenericPlatformMisc::RequestExit(false);
	}
}

void ASimScenarioDirector::SpawnMissingEntityActors()
{
	UWorld* MyWorld = GetWorld();
	const USimWorldSubsystem* Sim = MyWorld ? MyWorld->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr)
	{
		return;
	}

	for (const FSimEntityState& State : Sim->GetEntityStates())
	{
		if (!State.bAlive || SpawnedActors.Contains(State.Id))
		{
			continue;
		}
		TSubclassOf<ASimEntityActor> ClassToSpawn = State.bIsGround ? GroundActorClass : (State.bIsMissile ? MissileActorClass : AircraftActorClass);
		if (!ClassToSpawn)
		{
			ClassToSpawn = ASimEntityActor::StaticClass();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASimEntityActor* Actor = MyWorld->SpawnActor<ASimEntityActor>(ClassToSpawn, State.Location, State.Rotation, Params);
		if (Actor != nullptr)
		{
			Actor->EntityId = State.Id;
			Actor->SetActorScale3D(State.bIsGround ? GroundScale : (State.bIsMissile ? MissileScale : AircraftScale));
			if (State.bIsGround && (!GroundActorClass || GroundActorClass == ASimEntityActor::StaticClass()))
			{
				// A real model if there is one (the TZM, the fuel truck, the PU-12M), else the basic model, else a box.
				if (Actor->UseVehicleModel(State.Kind) || Actor->UseBasicModel(State.Model))
				{
					Actor->SetActorScale3D(FVector(1.0));  // the models are built to size
				}
				else
				{
					Actor->UseGroundStandIn();
				}
			}
			else if (!State.bIsGround && Actor->GetClass() == ASimEntityActor::StaticClass() && Actor->UseBasicModel(State.Model))
			{
				Actor->SetActorScale3D(FVector(1.0));  // aircraft and missiles at their real size (docs/scenery/)
			}
			UE_LOG(LogSAM3D, Log, TEXT("Entity %d '%s' drawn as %s"), State.Id, *State.Name,
				Actor->GetBasicModel().IsEmpty() ? TEXT("a placeholder") : *Actor->GetBasicModel());
#if WITH_EDITOR
			Actor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *State.Name, State.Id));
#endif
			SpawnedActors.Add(State.Id, Actor);
		}
	}
}

void ASimScenarioDirector::ShowStatus(const USimWorldSubsystem* Sim) const
{
	if (!bShowEventsOnScreen || GEngine == nullptr || !Sim->IsScenarioRunning())
	{
		return;
	}
	const UEnum* StateEnum = StaticEnum<ESimSubsystemState>();
	auto State = [&](const TCHAR* Id)
	{
		return StateEnum->GetNameStringByValue(static_cast<int64>(Sim->GetSubsystemState(Id)));
	};
	auto Lamp = [&](const TCHAR* Id) { return Sim->GetSimInput(Id) ? TEXT("ON") : TEXT("--"); };
	const FString Tracked = Sim->GetTrackedTarget();
	const FString Status = FString::Printf(
		TEXT("t=%.0fs  power %s  APU %s  antenna %s (%.0f%%)  SOC %s  SSC %s  missile prep %s  track %s  |  "
			 "captured %s  too low %s  caution %s  in zone %s%s"),
		Sim->GetSimTimeSeconds(), *State(TEXT("power")), *State(TEXT("apu_free")), *State(TEXT("search_antenna")),
		100.0 * Sim->GetSubsystemProgress(TEXT("search_antenna")), *State(TEXT("soc_transmitter")),
		*State(TEXT("ssc_transmitter")), *State(TEXT("missile_prep")), Tracked.IsEmpty() ? TEXT("none") : *Tracked,
		Lamp(TEXT("target_captured")), Lamp(TEXT("target_too_low")), Lamp(TEXT("target_near_zone")),
		Lamp(TEXT("target_in_zone")), Sim->IsScenarioFinished() ? TEXT("  |  SCENARIO ENDED") : TEXT(""));
	GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan, Status);
}

void ASimScenarioDirector::HandleSimEvent(const FSimEvent& Event)
{
	if (!bShowEventsOnScreen || GEngine == nullptr)
	{
		return;
	}
	const UEnum* EventEnum = StaticEnum<ESimEventType>();
	const FString TypeName = EventEnum ? EventEnum->GetNameStringByValue(static_cast<int64>(Event.Type)) : TEXT("?");
	const FString Message = FString::Printf(TEXT("t=%.2fs  %s  missile %d  aircraft %d  miss %.1f m"),
		Event.TimeSeconds, *TypeName, Event.MissileId, Event.AircraftId, Event.MissDistanceMeters);
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, Message);
}

void ASimScenarioDirector::HandleDebriefEntry(const FSimDebriefEntry& Entry)
{
	if (!bShowEventsOnScreen || GEngine == nullptr)
	{
		return;
	}
	FColor Colour = FColor::White;
	if (Entry.Kind == TEXT("HIT"))
	{
		Colour = FColor::Green;
	}
	else if (Entry.Kind == TEXT("MISS") || Entry.Kind == TEXT("FAULT") || Entry.Kind == TEXT("DEVIATION"))
	{
		Colour = FColor::Red;
	}
	else if (Entry.Kind == TEXT("LAUNCH"))
	{
		Colour = FColor::Orange;
	}
	const FString Who = Entry.Who.IsEmpty() ? TEXT("-") : Entry.Who;
	GEngine->AddOnScreenDebugMessage(-1, 12.0f, Colour,
		FString::Printf(TEXT("%6.1fs  %s  %s  %s  %s"), Entry.TimeSeconds, *Who, *Entry.Kind, *Entry.What, *Entry.Detail));
}
