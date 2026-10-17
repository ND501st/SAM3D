#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "SimWorldSubsystem.h"
#include "SimOsaVehicle.generated.h"

class UCameraComponent;
class USimCabinComponent;
class USimCrewBodiesComponent;
class USimCrewVoiceComponent;
class USimPu12MastComponent;
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

	/**
	 * The cabin camera's fixed exposure as a scene luminance (cd/m2): the project runs without the extended
	 * luminance range, so auto exposure's min/max are luminance, not EV100. Tuned by eye against the cabin photos with
	 * the dome lamps lit (3 left the panels burnt out white, 12 gives the photographs' cream enamel and shadows).
	 */
	UPROPERTY(EditAnywhere, Category = "SAM3D|Cabin")
	float CabinExposureBrightness = 12.0f;

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

	// ---- The crew (phase 8, docs/crew/): H the training overlay, O the crew menu (orders at the commander's seat,
	// reports elsewhere; 1-9 choose, 0 next page), T take this seat from its NPC or give it back.

	/** The training overlay is on: the drill master's next move for this seat is highlighted (refreshed each second). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	bool IsAssistOn() const { return bAssist; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FSimDrillAdvice GetAdvice() const { return Advice; }

	/** The task the overlay aims at: К бою until ready, then the capture, then the launch. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetAssistTask() const { return AssistTask; }

	/** The overlay's level (guided, hint, demonstrate; H cycles them and off). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetAssistLevel() const { return AssistLevel; }

	/** The player's last move at this seat, judged (the overlay's "that cost" line). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FSimMoveVerdict GetLastVerdict() const { return LastVerdict; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	bool IsCrewMenuOpen() const { return bMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	TArray<FSimCrewMenuEntry> GetCrewMenuEntries() const { return MenuEntries; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	int32 GetCrewMenuPage() const { return MenuPage; }

	/** This seat's console id, and whether the player holds it (true) or its NPC works it. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetStationId() const { return StationId; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	bool IsHoldingSeat() const { return bHoldingSeat; }

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

	/** The cabin tour as pictures: after DelaySeconds, each seat and instrument once, a screenshot of each into Folder
	 *  (<n>_<view>.png), then the game quits. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void StartCabinShots(const FString& Folder, float DelaySeconds = 20.0f);

	/**
	 * For scripted checks and screenshots: enter the cabin with the camera at a point of the layout's body frame
	 * (metres: x right, y forward, z up) looking along Rotation (yaw clockwise from forward, pitch up), with this field
	 * of view. Seats, mouse look and the tour are suspended until ClearCabinViewOverride.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void SetCabinViewOverride(FVector BodyLocationM, FRotator Rotation, float FovDeg = 80.0f);

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void ClearCabinViewOverride();

	// ---- The PU-12M (docs/PU12_DEVELOPMENT.md P6): when the scenario's own vehicle is the command post, the pawn wears
	// its traced exterior and mast instead of the Osa's boxes. In the battery commander's seat, [ and ] pick a target on
	// the post's board and 1-5 give it to that launcher by voice; in the plotter's seat, [ and ] pick a target on the
	// IT-45 and P writes it on the board.

	/** The scenario's own vehicle is the PU-12M command post. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	bool IsCommandPost() const { return bIsPost; }

	/** The target number picked at this seat (board or IT-45; -1: none; the IT-45's numbers start at 00). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	int32 GetPickedTarget() const { return PickedTarget; }

	/** Pick the next (+1) or previous (-1) target on this seat's picture: the board at the commander's, the IT-45 at the
	 *  plotter's. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void PickTarget(int32 Direction);

	/** At the commander's seat: give the picked target to the launcher at this place on the IT boards (0-based). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	bool DesignatePicked(int32 LauncherIndex);

	/** At the plotter's seat: write the picked IT-45 target on the board. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	bool PlotPicked();

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

	/** The crewmates' bodies in the cabin (roadmap 8.10), in the cabin's frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USimCrewBodiesComponent> CrewBodies;

	/** The crew's voices: their callouts on the intercom and the battery net (roadmap 8.10). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USimCrewVoiceComponent> CrewVoice;

	/** First-person camera at the current station's eye point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UCameraComponent> CabinCamera;

	/** The PU-12M's exterior (SM_PU12M) and mast, made when a command-post scenario starts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UStaticMeshComponent> PostExterior;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USimPu12MastComponent> PostMast;

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
	void HandleCrew(class APlayerController* Pc, USimWorldSubsystem* Sim, float DeltaSeconds);
	void ConfigureForScenario(USimWorldSubsystem* Sim);
	void HandlePost(class APlayerController* Pc);
	TArray<int32> PickList() const;

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

	bool bViewOverride = false;
	FVector OverrideLocation = FVector::ZeroVector;
	FRotator OverrideRotation = FRotator::ZeroRotator;

	bool bAssist = false;
	float AssistClock = 0.0f;
	FString AssistTask;
	FString AssistLevel;
	FSimMoveVerdict LastVerdict;
	FSimDrillAdvice Advice;
	bool bMenuOpen = false;
	int32 MenuPage = 0;
	TArray<FSimCrewMenuEntry> MenuEntries;
	FString StationId;
	bool bHoldingSeat = false;

	bool bConfigured = false;
	bool bIsPost = false;
	int32 PickedTarget = INDEX_NONE;

	bool bTouring = false;
	FString ShotFolder;
	float ShotDelayLeft = -1.0f;
	int32 ShotTaken = INDEX_NONE;
	float TourSecondsPerView = 6.0f;
	float TourClock = 0.0f;
	int32 TourIndex = 0;
	TArray<FSimCabinTourView> TourViews;
};
