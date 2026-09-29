#include "SimScenarioDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SAM3D.h"
#include "SimEntityActor.h"

ASimScenarioDirector::ASimScenarioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	AircraftActorClass = ASimEntityActor::StaticClass();
	MissileActorClass = ASimEntityActor::StaticClass();
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
	Sim->ResetSimulation(OriginLatitudeDeg, OriginLongitudeDeg, OriginAltitudeM);
	Sim->OnSimEvent.AddDynamic(this, &ASimScenarioDirector::HandleSimEvent);
	if (bStartDemoOnBeginPlay)
	{
		Sim->StartDemoScenario();
	}
}

void ASimScenarioDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* MyWorld = GetWorld())
	{
		if (USimWorldSubsystem* Sim = MyWorld->GetSubsystem<USimWorldSubsystem>())
		{
			Sim->OnSimEvent.RemoveDynamic(this, &ASimScenarioDirector::HandleSimEvent);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASimScenarioDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SpawnMissingEntityActors();
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
		TSubclassOf<ASimEntityActor> ClassToSpawn = State.bIsMissile ? MissileActorClass : AircraftActorClass;
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
			Actor->SetActorScale3D(State.bIsMissile ? MissileScale : AircraftScale);
#if WITH_EDITOR
			Actor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *State.Name, State.Id));
#endif
			SpawnedActors.Add(State.Id, Actor);
		}
	}
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
