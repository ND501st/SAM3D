#include "SimScenarioDirector.h"

#include "SimPositionWork.h"

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
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
#include "SimBasicModels.h"
#include "SimCampaignHUD.h"
#include "SimEntityActor.h"
#include "SimMissionMenuHUD.h"
#include "SimModelGallery.h"
#include "SimOsaVehicle.h"
#include "SimSkyWeatherActor.h"
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
	else if (FParse::Value(FCommandLine::Get(), TEXT("-SimAirShots="), CommandLineScenario) && !CommandLineScenario.IsEmpty())
	{
		// The air battle's photographer (roadmap B13): its own air battle when no scenario is named.
		ScenarioFile = TEXT("scenarios/air_battle_shots.json");
		CabinFile.Reset();
		UE_LOG(LogSAM3D, Log, TEXT("The air battle's photographer: %s"), *ScenarioFile);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("-SimFxShots="), CommandLineScenario) ||
		FParse::Value(FCommandLine::Get(), TEXT("-SimFxShotsNight="), CommandLineScenario))
	{
		// The effects' photographer (docs/fx/README.md 4.1) shoots in a running scenario: the first practice when none
		// is named (it brings its own aircraft where the scenario has none to shoot).
		ScenarioFile = TEXT("scenarios/osa_practice.json");
		CabinFile.Reset();
		UE_LOG(LogSAM3D, Log, TEXT("The effects' photographer: %s"), *ScenarioFile);
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
	// After the georeference moves: CesiumSunSky turns its own sun by its clock when the globe moves under it.
	FindLevelSun();
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
	// -SimInstructor: start with the trainer's instructor panel open (roadmap 10.3; F9 in the game).
	if (Vehicle != nullptr && FParse::Param(FCommandLine::Get(), TEXT("SimInstructor")))
	{
		Vehicle->SetInstructorOpen(true);
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

void ASimScenarioDirector::FindLevelSun()
{
	LevelSun.Reset();
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (TActorIterator<ASimSkyWeatherActor>(World))
	{
		return;  // the sky actor turns its own sun by the scenario's clock
	}
	// Any actor's atmosphere sun: a DirectionalLight actor's, or a component of another (OsaPractice's is CesiumSunSky's,
	// which no ADirectionalLight iterator finds: its sun stood at Cesium's own clock and 111000 lux).
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UDirectionalLightComponent*> Lights(*It);
		for (UDirectionalLightComponent* Light : Lights)
		{
			if (Light == nullptr || !Light->IsUsedAsAtmosphereSunLight() || !Light->IsRegistered())
			{
				continue;
			}
			if (Light->GetMobility() != EComponentMobility::Movable)
			{
				Light->SetMobility(EComponentMobility::Movable);  // no static lighting in the project: nothing baked is lost
			}
			const float OwnLux = Light->Intensity;
			if (LevelSunLux > 0.0f)
			{
				Light->SetIntensity(LevelSunLux);
			}
			LevelSun = Light;
			NextLevelSunAt = 0.0f;
			UE_LOG(LogSAM3D, Log, TEXT("Sky: the level's sun %s.%s follows the scenario's sun, at %.0f lux (the level's own %.0f)"),
				*It->GetName(), *Light->GetName(), Light->Intensity, OwnLux);
			UpdateLevelSun();
			return;
		}
	}
}

void ASimScenarioDirector::ForgetScenarioActors()
{
	int32 Gone = 0;
	for (TPair<int32, TObjectPtr<ASimEntityActor>>& It : SpawnedActors)
	{
		if (It.Value != nullptr)
		{
			It.Value->Destroy();
			++Gone;
		}
	}
	SpawnedActors.Empty();
	for (TObjectPtr<ASimPositionWork>& Work : PositionWorks)
	{
		if (Work != nullptr)
		{
			Work->Destroy();
		}
	}
	PositionWorks.Empty();
	for (TObjectPtr<AActor>& Site : NeighbourSites)
	{
		if (Site != nullptr)
		{
			Site->Destroy();
		}
	}
	NeighbourSites.Empty();
	NeighbourSiteAt.Empty();
	NeighbourSiteGrounded.Empty();
	bNeighboursSpawned = false;
	bNeighbourFallbackPlaced = false;
	UE_LOG(LogSAM3D, Log, TEXT("A new scenario in place of the last: its %d entity actors, works and neighbour sites removed"), Gone);
}

void ASimScenarioDirector::UpdateLevelSun()
{
	UDirectionalLightComponent* Light = LevelSun.Get();
	const USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	double ElevationDeg = 0.0;
	double AzimuthDeg = 0.0;
	bool bDaylight = true;
	float NightShare = 0.0f;
	if (Light == nullptr || Sim == nullptr || !Sim->GetSunNow(ElevationDeg, AzimuthDeg, bDaylight, NightShare))
	{
		return;
	}
	// The direction to the sun in Unreal (X north, Y east, Z up); the light shines the other way (as ASimSkyWeatherActor).
	const double El = FMath::DegreesToRadians(ElevationDeg);
	const double Az = FMath::DegreesToRadians(AzimuthDeg);
	const FVector ToSun(FMath::Cos(El) * FMath::Cos(Az), FMath::Cos(El) * FMath::Sin(Az), FMath::Sin(El));
	Light->SetWorldRotation((-ToSun).Rotation());
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
	const void* RunnerNow = Sim->GetFxRunner();
	if (RunnerNow != SeenRunner)
	{
		if (SeenRunner != nullptr)
		{
			ForgetScenarioActors();
		}
		SeenRunner = RunnerNow;
	}
	if (LevelSun.IsValid() && TimeSinceBeginPlay >= NextLevelSunAt)
	{
		NextLevelSunAt = TimeSinceBeginPlay + 1.0f;
		UpdateLevelSun();
	}
	if (!bTerrainRequested && TerrainTileset != nullptr && Sim->IsScenarioRunning() &&
		TimeSinceBeginPlay >= TerrainSampleDelaySeconds)
	{
		bTerrainRequested = Sim->RequestCesiumTerrain(TerrainTileset, TerrainHalfSizeMeters, TerrainSpacingMeters);
	}
	SpawnMissingEntityActors();
	UpdatePositionWorks();
	if (!bNeighboursSpawned && Sim->IsScenarioRunning())
	{
		SpawnNeighbourSites(Sim);
	}
	else if (bNeighboursSpawned && TimeSinceBeginPlay >= NextNeighbourGroundAt)
	{
		NextNeighbourGroundAt = TimeSinceBeginPlay + 1.0f;
		GroundNeighbourSites(TimeSinceBeginPlay - NeighboursSpawnedAt >= 60.0f);
	}
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

	for (const FSimEntityState& State : Sim->GetEntityStatesCached())  // the frame's one snapshot, not a copy
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
				else if (State.Kind == TEXT("launcher") || State.Kind == TEXT("dummy"))
				{
					Actor->UseOsaStandIn();  // a launcher, or a dummy position's mock-up of one
					Actor->SetActorScale3D(FVector(1.0));
				}
				else
				{
					Actor->UseGroundStandIn();
				}
			}
			else if (!State.bIsGround && Actor->GetClass() == ASimEntityActor::StaticClass() && Actor->UseBasicModel(State.Model))
			{
				// Aircraft at their real size (docs/scenery/), and missiles too: ours, the ARMs and the cruise missiles wear
				// their munition's mesh (FSimMunitionModels, docs/fx/README.md 4.3). MissileScale is only for the cone that
				// stands in for a missile without a model.
				Actor->SetActorScale3D(FVector(1.0));
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

void ASimScenarioDirector::SpawnNeighbourSites(USimWorldSubsystem* Sim)
{
	const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
	const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
	UWorld* MyWorld = GetWorld();
	if (Runner == nullptr || Lib == nullptr || MyWorld == nullptr)
	{
		return;
	}
	bNeighboursSpawned = true;
	NeighboursSpawnedAt = TimeSinceBeginPlay;
	int32 Vehicles = 0;
	TArray<FSimViewPlace> Places;  // each site, and each scenery model, for the views (F12)
	// One model standing on the ground at (east, north) of the scenario, facing a true heading: kept with the sites, so
	// it is put on the level's ground as they are.
	auto Stand = [&](const std::string& Model, double East, double North, double HeadingDeg)
	{
		const simcore::Vec3 Enu = Runner->GroundEnu(East, North);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Site = MyWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (Site == nullptr)
		{
			return FVector::ZeroVector;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Site, TEXT("Root"));
		Site->SetRootComponent(Root);
		Root->RegisterComponent();
		Site->Tags.Add(USimWorldSubsystem::GroundTag);  // a site is set on the ground, never the ground (roadmap R5)
		const FVector At = Sim->ToUnreal(Enu);  // through the level's georeference where it has one (Cesium's X is east)
		Site->SetActorLocationAndRotation(At, FRotator(0.0, Sim->HeadingToYawDeg(At, HeadingDeg), 0.0));
		FSimBasicModels::Build(UTF8_TO_TCHAR(Model.c_str()), Root);
		NeighbourSites.Add(Site);
		NeighbourSiteAt.Add(At);
		// On a map's level its drawn ground at once (one ground, roadmap R5: the sim's GroundEnu is on it already).
		FVector Ground;
		const bool bOnMap = Sim->GetGroundMap() != nullptr && Sim->GroundAt(At, Ground, nullptr, Site);
		if (bOnMap)
		{
			Site->SetActorLocation(Ground);
		}
		NeighbourSiteGrounded.Add(bOnMap);
		++Vehicles;
		return At;
	};
	const std::vector<std::pair<std::string, simcore::Vec3>> Fighting = Runner->NeighbourLaunchers();
	for (const simcore::ScenarioNeighbour& Nb : Runner->Neighbours())
	{
		// An Osa battery that fights stands where its launchers fire from (the battery's combat formation); the others
		// as their system's site.
		std::vector<std::pair<std::string, simcore::Vec3>> Layout;
		if (Runner->NeighbourFights(Nb.id))
		{
			for (const std::pair<std::string, simcore::Vec3>& L : Fighting)
			{
				if (L.first == Nb.id)
				{
					Layout.push_back({"osa_9a33", simcore::Vec3{L.second.x - Nb.position.x, L.second.y - Nb.position.y, 0.0}});
				}
			}
		}
		else
		{
			Layout = Lib->SiteLayout(Nb.system);
		}
		for (const std::pair<std::string, simcore::Vec3>& V : Layout)
		{
			// Each vehicle faces out from the site's centre (an S-75's launchers round its radar).
			const double HeadingDeg = V.second.HorizontalLength() > 1.0 ? FMath::RadiansToDegrees(FMath::Atan2(V.second.x, V.second.y)) : 0.0;
			Stand(V.first, Nb.position.x + V.second.x, Nb.position.y + V.second.y, HeadingDeg);
		}
		FSimViewPlace Place;
		Place.Name = UTF8_TO_TCHAR(Nb.name.c_str());
		Place.Location = Sim->ToUnreal(Runner->GroundEnu(Nb.position.x, Nb.position.y));
		Place.SizeM = Runner->NeighbourFights(Nb.id) ? 600.0 : 40.0;
		Places.Add(Place);
	}
	// The scenario's scenery: radars and whatever else stands there (the dynamic campaign testbed's).
	for (const simcore::ScenarioScenery& Sc : Runner->Scenery())
	{
		FSimViewPlace Place;
		Place.Name = UTF8_TO_TCHAR(Sc.name.c_str());
		Place.Location = Stand(Sc.model, Sc.position.x, Sc.position.y, Sc.heading_deg);
		Place.bFriendly = Sc.friendly;
		Place.SizeM = 15.0;
		Places.Add(Place);
	}
	Sim->SetViewPlaces(TEXT("Scenario"), MoveTemp(Places));
	if (!Runner->Neighbours().empty() || !Runner->Scenery().empty())
	{
		UE_LOG(LogSAM3D, Log, TEXT("Neighbours: %d air defences and %d scenery models near the scenario, %d vehicles drawn"),
			static_cast<int32>(Runner->Neighbours().size()), static_cast<int32>(Runner->Scenery().size()), Vehicles);
	}
}

void ASimScenarioDirector::GroundNeighbourSites(bool bFirstMinuteOver)
{
	UWorld* MyWorld = GetWorld();
	if (MyWorld == nullptr)
	{
		return;
	}
	int32 Grounded = 0;
	for (int32 i = 0; i < NeighbourSites.Num(); ++i)
	{
		AActor* Site = NeighbourSites[i];
		if (Site == nullptr || NeighbourSiteGrounded[i])
		{
			Grounded += Site != nullptr ? 1 : 0;
			continue;
		}
		// The level's ground (USimWorldSubsystem::GroundAt: a map's drawn ground, else a ground-only trace 3 km either way).
		// A trace straight down a Cesium tile's seam can slip through, so a metre to each side too.
		const USimWorldSubsystem* Sim = MyWorld->GetSubsystem<USimWorldSubsystem>();
		const FVector At = NeighbourSiteAt[i];
		for (const FVector& Side : {FVector::ZeroVector, FVector(100.0, 0.0, 0.0), FVector(-100.0, 0.0, 0.0), FVector(0.0, 100.0, 0.0), FVector(0.0, -100.0, 0.0)})
		{
			FVector Ground;
			if (Sim != nullptr && Sim->GroundAt(At + Side, Ground, nullptr, Site))
			{
				Site->SetActorLocation(FVector(At.X, At.Y, Ground.Z));
				NeighbourSiteGrounded[i] = true;
				++Grounded;
				break;
			}
		}
	}
	if (Grounded == NeighbourSites.Num())
	{
		NextNeighbourGroundAt = TNumericLimits<float>::Max();  // all down (or none): no more traces
		if (Grounded > 0)
		{
			UE_LOG(LogSAM3D, Log, TEXT("Neighbours: all %d vehicles on the level's ground"), Grounded);
		}
		return;
	}
	if (bFirstMinuteOver && !bNeighbourFallbackPlaced)
	{
		// The far ones' tiles may never load with collision: put each as far under or over the scenario's ground as the
		// nearest one that found the level's (the globe's curve and the datum differ with the place), and keep trying.
		bNeighbourFallbackPlaced = true;
		for (int32 i = 0; i < NeighbourSites.Num(); ++i)
		{
			if (NeighbourSites[i] == nullptr || NeighbourSiteGrounded[i])
			{
				continue;
			}
			double BestD = TNumericLimits<double>::Max();
			double Offset = 0.0;
			for (int32 j = 0; j < NeighbourSites.Num(); ++j)
			{
				const double D = FVector::DistSquared2D(NeighbourSiteAt[i], NeighbourSiteAt[j]);
				if (NeighbourSites[j] != nullptr && NeighbourSiteGrounded[j] && D < BestD)
				{
					BestD = D;
					Offset = NeighbourSites[j]->GetActorLocation().Z - NeighbourSiteAt[j].Z;
				}
			}
			NeighbourSites[i]->SetActorLocation(NeighbourSiteAt[i] + FVector(0.0, 0.0, Offset));
		}
		UE_LOG(LogSAM3D, Log, TEXT("Neighbours: %d of %d vehicles on the level's ground, the rest by their nearest"),
			Grounded, NeighbourSites.Num());
	}
	if (bFirstMinuteOver)
	{
		NextNeighbourGroundAt += 4.0f;  // every 5 s after the first minute
	}
}

void ASimScenarioDirector::UpdatePositionWorks()
{
	UWorld* MyWorld = GetWorld();
	const USimWorldSubsystem* Sim = MyWorld ? MyWorld->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr)
	{
		return;
	}
	const TArray<FSimPositionWorkState> Works = Sim->GetPositionWorks();
	for (int32 I = 0; I < Works.Num(); ++I)
	{
		const FSimPositionWorkState& W = Works[I];
		if (I >= PositionWorks.Num())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASimPositionWork* Work = MyWorld->SpawnActor<ASimPositionWork>(ASimPositionWork::StaticClass(), W.Location,
				FRotator(0.0, W.YawDeg, 0.0), Params);
			if (Work == nullptr)
			{
				return;
			}
			Work->WorkIndex = I;
			Work->Build(W.bEmplacement, W.bNets);
#if WITH_EDITOR
			Work->SetActorLabel(FString::Printf(TEXT("PositionWork_%d"), I));
#endif
			PositionWorks.Add(Work);
		}
		if (ASimPositionWork* Work = PositionWorks[I])
		{
			if (Work->HasNets() != W.bNets)
			{
				Work->SetNets(W.bNets);
			}
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
