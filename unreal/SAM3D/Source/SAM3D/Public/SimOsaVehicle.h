#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "SimOsaVehicle.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USimWorldSubsystem;

/** A key that gives a crew member an order (placeholder for the order menu of phase 6). */
USTRUCT(BlueprintType)
struct SAM3D_API FSimCrewHotkey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	FString Crew;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	FString Procedure;

	/** After current orders instead of dropping them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	bool bQueue = false;
};

/**
 * Placeholder drivable 9K33 Osa: engine box shapes, kinematic driving over the ground, a turret that follows
 * the tracking antenna's azimuth wheel and a search antenna that folds with the sim's stowage progress.
 *
 * The simulation stays in charge: the pawn reports its position and whether it is stopped to the scenario
 * (SetUnitLocation, the vehicle_stopped input), and only animates what the sim says. Driving off with the
 * antenna-launcher unit unlocked faults it in the sim, as the data file specifies.
 *
 * Controls: W/S throttle and reverse, A/D steer, Space brake; number keys give crew orders (CrewHotkeys);
 * right mouse drag orbits the camera. Kinematic for now: swap in Chaos Vehicles once there is a real
 * vehicle mesh with wheel bones.
 */
UCLASS(Blueprintable)
class SAM3D_API ASimOsaVehicle : public APawn
{
	GENERATED_BODY()

public:
	ASimOsaVehicle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Road speed limit. 70 km/h (data/systems/osa.json, low confidence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Vehicle")
	double MaxSpeedMps = 19.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Vehicle")
	double AccelerationMps2 = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Vehicle")
	double BrakingMps2 = 5.0;

	/** Heading change per second at full lock and 10 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Vehicle")
	double TurnRateDegPerS = 20.0;

	/** Below this speed the vehicle counts as stopped for the sim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Vehicle")
	double StoppedBelowMps = 0.2;

	/** Sim ids this pawn reads and writes (data/procedures/osa_ak.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sim")
	FString StoppedInput = TEXT("vehicle_stopped");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sim")
	FString TurretAzimuthWheel = TEXT("ssc.az_wheel");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sim")
	FString SearchAntennaSubsystem = TEXT("search_antenna");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	TArray<FSimCrewHotkey> CrewHotkeys;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Vehicle")
	double GetSpeedMps() const { return SpeedMps; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UStaticMeshComponent> Hull;

	/** Rotates in azimuth: the antenna-launcher unit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USceneComponent> TurretPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UStaticMeshComponent> Turret;

	/** Folds back 90 degrees for transport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USceneComponent> AntennaHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UStaticMeshComponent> SearchAntenna;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UCameraComponent> Camera;

private:
	void Drive(float DeltaSeconds, class APlayerController* Pc);
	void FollowGround();
	void UpdateSim(USimWorldSubsystem* Sim);
	void AnimateFromSim(const USimWorldSubsystem* Sim);
	void HandleHotkeys(class APlayerController* Pc, USimWorldSubsystem* Sim);

	double SpeedMps = 0.0;
	double HeadingDeg = 0.0;
};
