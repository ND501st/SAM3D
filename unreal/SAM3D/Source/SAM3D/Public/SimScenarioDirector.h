#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimWorldSubsystem.h"
#include "SimScenarioDirector.generated.h"

class ACesium3DTileset;
class ASimEntityActor;
class ASimOsaVehicle;

/**
 * Drop one of these into a level and press Play.
 *
 * With ScenarioFile set (the default), it loads that scenario (data/scenarios/...), moves the Cesium
 * georeference origin onto the scenario's origin, samples terrain heights from TerrainTileset for the
 * simulation, spawns and possesses the player's vehicle at the unit's position, spawns a proxy actor for
 * every target and missile, and prints the debrief on screen as it happens.
 * With ScenarioFile empty, it runs the older generic demo engagement instead.
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

	/** Scenario to play, relative to the data folder. Empty = the generic demo engagement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	FString ScenarioFile = TEXT("scenarios/osa_practice.json");

	/** 3D cabin layout for the scenario's console, relative to the data folder. Empty = no cabin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	FString CabinFile = TEXT("cabins/osa_ak.json");

	/** Move the level's Cesium georeference so its origin is the scenario origin, and convert through it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cesium")
	bool bUseCesiumGeoreference = true;

	/** Cesium terrain tileset (e.g. Cesium World Terrain) to sample heights from. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cesium")
	TObjectPtr<ACesium3DTileset> TerrainTileset;

	/** Seconds to wait after BeginPlay before sampling, so the tileset has started loading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cesium")
	float TerrainSampleDelaySeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cesium")
	double TerrainHalfSizeMeters = 30000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cesium")
	double TerrainSpacingMeters = 300.0;

	/** Spawn the player's vehicle at the scenario unit's position and possess it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	bool bSpawnPlayerVehicle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	TSubclassOf<ASimOsaVehicle> PlayerVehicleClass;

	/** Demo mode only: geodetic origin of the local sim frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Demo")
	double OriginLatitudeDeg = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Demo")
	double OriginLongitudeDeg = 25.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Demo")
	double OriginAltitudeM = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Demo")
	bool bStartDemoOnBeginPlay = true;

	/** Print sim events / debrief lines and a status line on screen. */
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

	UFUNCTION()
	void HandleDebriefEntry(const FSimDebriefEntry& Entry);

	void StartScenario(USimWorldSubsystem* Sim);
	void SpawnPlayerVehicle(USimWorldSubsystem* Sim);
	void SpawnMissingEntityActors();
	void ShowStatus(const USimWorldSubsystem* Sim) const;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ASimEntityActor>> SpawnedActors;

	bool bTerrainRequested = false;
	float TimeSinceBeginPlay = 0.0f;
};
