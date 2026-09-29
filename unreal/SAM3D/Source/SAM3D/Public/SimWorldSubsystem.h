#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWorldSubsystem.generated.h"

/** Snapshot of one simulated entity, converted to Unreal units (cm, degrees). */
USTRUCT(BlueprintType)
struct SAM3D_API FSimEntityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	int32 Id = -1;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	bool bIsMissile = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	bool bAlive = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	FRotator Rotation = FRotator::ZeroRotator;
};

UENUM(BlueprintType)
enum class ESimEventType : uint8
{
	MissileLaunched,
	MissileDetonated,
	MissileMissed,
	MissileSelfDestructed,
	MissileHitGround,
	AircraftDestroyed
};

USTRUCT(BlueprintType)
struct SAM3D_API FSimEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	ESimEventType Type = ESimEventType::MissileLaunched;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	double TimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	int32 MissileId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	int32 AircraftId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	double MissDistanceMeters = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimEvent, const FSimEvent&, Event);

/**
 * Owns the simcore::World for a game world and advances it at a fixed rate.
 *
 * Coordinate mapping (see docs/ARCHITECTURE.md):
 *   simcore ENU metres (x=East, y=North, z=Up)  ->  Unreal cm (X=North, Y=East, Z=Up)
 * Swapping X/Y converts the right-handed ENU frame into Unreal's left-handed frame.
 */
UCLASS()
class SAM3D_API USimWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem / FTickableGameObject
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Create a fresh simulation anchored at a geodetic origin. Keep this equal to the CesiumGeoreference origin. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void ResetSimulation(double OriginLatitudeDeg, double OriginLongitudeDeg, double OriginAltitudeM);

	/** One generic radar + launcher at the origin vs. one inbound target, with automatic fire control. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void StartDemoScenario();

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	TArray<FSimEntityState> GetEntityStates() const;

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	bool GetEntityState(int32 Id, FSimEntityState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Sim")
	double GetSimTimeSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void SetSimPaused(bool bInPaused) { bPaused = bInPaused; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Sim")
	bool IsSimPaused() const { return bPaused; }

	/** Fired for every simcore event (launch, detonation, kill...). */
	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Sim")
	FOnSimEvent OnSimEvent;

	/** Direct access for C++ gameplay code. Null until ResetSimulation() is called. */
	simcore::World* GetSimWorld() const { return SimWorld.Get(); }

	static FVector EnuToUnreal(const simcore::Vec3& Enu);
	static simcore::Vec3 UnrealToEnu(const FVector& Unreal);

private:
	void RunDemoFireControl();
	void BroadcastEvents();
	FSimEntityState MakeAircraftState(const simcore::Aircraft& InAircraft) const;
	FSimEntityState MakeMissileState(const simcore::Missile& InMissile) const;

	TUniquePtr<simcore::World> SimWorld;
	double Accumulator = 0.0;
	double FixedStepSeconds = 0.01;
	bool bPaused = false;

	// Demo scenario state (generic placeholder parameters, not real system data).
	bool bDemoActive = false;
	bool bDemoLaunched = false;
	int32 DemoTargetId = -1;
	double DemoLaunchRangeM = 12000.0;
	simcore::Vec3 DemoSite;
	simcore::RadarParams DemoRadar;
	simcore::MissileParams DemoMissile;
	simcore::FlatTerrain DemoGround{0.0};
};
