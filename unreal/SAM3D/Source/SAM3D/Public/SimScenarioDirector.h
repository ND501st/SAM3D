#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimWorldSubsystem.h"
#include "SimScenarioDirector.generated.h"

class ASimEntityActor;

/**
 * Drop one of these into a level and press Play.
 * It resets the simulation at the configured geodetic origin, optionally starts the demo
 * engagement, spawns an ASimEntityActor for every simulated aircraft/missile, and prints
 * sim events on screen.
 */
UCLASS(Blueprintable)
class SAM3D_API ASimScenarioDirector : public AActor
{
	GENERATED_BODY()

public:
	ASimScenarioDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Geodetic origin of the local sim frame. Match your CesiumGeoreference origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	double OriginLatitudeDeg = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	double OriginLongitudeDeg = 25.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	double OriginAltitudeM = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	bool bStartDemoOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	bool bShowEventsOnScreen = true;

	/** Actor classes to spawn for sim entities (defaults to ASimEntityActor with a cone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	TSubclassOf<ASimEntityActor> AircraftActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	TSubclassOf<ASimEntityActor> MissileActorClass;

	/** Placeholder cone is 1 m; scale it up so it's visible at range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	FVector AircraftScale = FVector(15.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	FVector MissileScale = FVector(4.0);

private:
	UFUNCTION()
	void HandleSimEvent(const FSimEvent& Event);

	void SpawnMissingEntityActors();

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ASimEntityActor>> SpawnedActors;
};
