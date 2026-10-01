#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "SimWorldSubsystem.h"
#include "SimOsaVehicle.generated.h"

class UCameraComponent;
class USimCabinComponent;
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

/** One stop of the hands-free cabin tour: a seat, optionally looking straight at one instrument. */
struct FSimCabinTourView
{
	int32 Station = 0;
	bool bLookAt = false;
	FVector Target = FVector::ZeroVector;
	FString Label;
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
 * right mouse drag orbits the camera. C enters the 3D cabin (USimCabinComponent): mouse looks around, Tab
 * changes seat, left/right click works switches, selectors, keys, covers and buttons, drag or scroll turns hand
 * wheels. Kinematic for now: swap in Chaos Vehicles once there is a real vehicle mesh with wheel bones.
 */
UCLASS(Blueprintable)
class SAM3D_API ASimOsaVehicle : public APawn
{
	GENERATED_BODY()

public:
	ASimOsaVehicle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

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

	/** Keys for the cabin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	FKey CabinKey = EKeys::C;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	FKey NextStationKey = EKeys::Tab;

	/** Degrees of view turn per unit of mouse movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double CabinLookDegPerMouseUnit = 2.5;

	/** Hand-wheel turns per unit of horizontal mouse movement while dragging a wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double WheelTurnsPerMouseUnit = 0.04;

	/** Hand-wheel turns per mouse scroll notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double WheelTurnsPerScrollNotch = 1.0 / 24.0;

	/** How far the player can reach from the eye point (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double CabinReachCm = 150.0;

	/** HUD drawn for the possessing player (crosshair and control names in the cabin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	TSubclassOf<class AHUD> CabinHUDClass;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsInCabin() const { return bInCabin; }

	/** Enter or leave the cabin. Entering needs a loaded cabin layout. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool SetInCabin(bool bEnter);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinHoverText() const { return HoverText; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinFeedbackText() const { return FeedbackSecondsLeft > 0.0f ? FeedbackText : FString(); }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinStationLabel() const { return StationLabel; }

	/**
	 * Hands-free test mode: enter the cabin and step through every seat and a close-up of every instrument,
	 * SecondsPerView each, round and round, logging each view (LogSAM3D). Lets a run be checked from screenshots
	 * and the log alone. C stops it.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void StartCabinTour(float SecondsPerView = 6.0f);

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void StopCabinTour();

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsTouringCabin() const { return bTouring; }

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

	/** Panels, controls and lamps inside the hull, built from the cabin layout. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USimCabinComponent> Cabin;

	/** First-person camera at the current station's eye point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UCameraComponent> CabinCamera;

private:
	void Drive(float DeltaSeconds, class APlayerController* Pc);
	void FollowGround();
	void UpdateSim(USimWorldSubsystem* Sim);
	void AnimateFromSim(const USimWorldSubsystem* Sim);
	void HandleHotkeys(class APlayerController* Pc, USimWorldSubsystem* Sim);
	void HandleCabin(float DeltaSeconds, class APlayerController* Pc);
	void ApplyStationView();
	void ShowFeedback(ESimPlayerActionResult Result);
	void UpdateTour(float DeltaSeconds);
	void ShowTourView();

	double SpeedMps = 0.0;
	double HeadingDeg = 0.0;

	bool bInCabin = false;
	int32 Station = 0;
	FString StationLabel;
	/** Head turn from the station's view direction (degrees). */
	double LookYawDeg = 0.0;
	double LookPitchDeg = 0.0;
	int32 HoveredItem = INDEX_NONE;
	int32 DraggedWheel = INDEX_NONE;
	FString HoverText;
	FString FeedbackText;
	float FeedbackSecondsLeft = 0.0f;

	bool bTouring = false;
	float TourSecondsPerView = 6.0f;
	float TourClock = 0.0f;
	int32 TourIndex = 0;
	TArray<FSimCabinTourView> TourViews;
};
