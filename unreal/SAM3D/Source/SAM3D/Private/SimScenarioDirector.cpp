#include "SimScenarioDirector.h"

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SAM3D.h"
#include "SimEntityActor.h"
#include "SimOsaVehicle.h"

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
	if (!CabinFile.IsEmpty() && !Sim->LoadCabin(CabinFile, Error) && GEngine != nullptr)
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
		Pc->Possess(Vehicle);
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
				Actor->UseGroundStandIn();
			}
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
