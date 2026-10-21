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
class USimViewComponent;
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
	/** A look round from the seat (not bLookAt): the head turned this far from the seat's view, and the lens (0: 80). */
	double YawDeg = 0.0;
	double PitchDeg = 0.0;
	float FovDeg = 0.0f;
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
 *
 * The camera views (F1-F11, as in DCS) and the time keys (Ctrl/Alt/Shift+Z) are its USimViewComponent's.
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

	/** Up from the seat and walking in the cabin, or down into the seat beside you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	FKey StandKey = EKeys::X;

	/** Walking pace in the cabin (cm/s): a crewman stooping under a low roof. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double WalkSpeedCmPerS = 90.0;

	/** In a battery link: to the other vehicle (the Osa <-> the PU-12M). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	FKey SwitchVehicleKey = EKeys::V;

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

	/** How far for the seat's own controls (cm): the crewman stands and steps to a far panel of his seat (simcore's
	 *  crew_body.hpp: it costs drill time, it isn't out of reach), e.g. the deputy commander's gyro panel, 1.5-1.75 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	double StandingReachCm = 220.0;

	/** HUD drawn for the possessing player (crosshair and control names in the cabin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Cabin")
	TSubclassOf<class AHUD> CabinHUDClass;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsInCabin() const { return bInCabin; }

	/** Up from the seat, walking in the cabin. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsWalking() const { return bInCabin && bWalking; }

	/** X: up from the seat onto the cabin's floor, or down into the seat he stands by (FeedbackText says why not). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void ToggleWalking();

	/** Enter or leave the cabin. Entering needs a loaded cabin layout. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool SetInCabin(bool bEnter);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinHoverText() const { return HoverText; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinFeedbackText() const { return FeedbackSecondsLeft > 0.0f ? FeedbackText : FString(); }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString GetCabinStationLabel() const { return StationLabel; }

	/** The next seat of the cabin (Tab, or F1 again). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void NextStation();
	/** The head's turn (yaw and pitch from the seat's view) that looks at a point in the world from where the eye is now
	 *  (the seat, or standing at a far panel). False if the seat has no view. */
	bool LookAnglesTo(const FVector& World, double& OutYawDeg, double& OutPitchDeg) const;
	/** The wheel just grabbed: start turning the head onto its display, if it has one. */
	void StartWheelWatch(int32 Item);

	/** The camera views (F1-F11) and the time keys. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	USimViewComponent* GetViews() const { return Views; }

	/** The 3D cabin (the HUD finds a lesson's highlight in it). */
	USimCabinComponent* GetCabin() const { return Cabin; }

	/** A lesson (simcore/tutorial.hpp): turn the head onto what the step highlights, from the lesson's seat (L). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Tutorial")
	void LookAtLessonTarget();

	// ---- The crew (phase 8, docs/crew/): H the helper, O the crew menu (orders at the commander's seat, reports
	// elsewhere; 1-9 choose, 0 next page), T take this seat from its NPC or give it back, N let the helper make its move.

	/** The helper is on (docs/crew/13_HELPER.md): the move this seat's NPC would make now is highlighted, as a lesson's
	 *  step is, without the lesson's card (refreshed twice a second). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Helper")
	bool IsAssistOn() const { return bAssist; }

	/** The helper for the seat the camera is at. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Helper")
	FSimSeatHelp GetHelp() const { return Help; }

	/** The helper's level (unassisted, hint, guided, demonstrate). H cycles off, hints, on, watch the crewman. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Helper")
	FString GetAssistLevel() const { return AssistLevel; }

	/** The next level round (H in the cabin, H in the mission's menu), kept for the next missions too. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Helper")
	void CycleHelper(bool bWithWatch);

	/** The helper's target at the post (a target to give a launcher, or to plot): its number, and the launcher. -1 none. */
	int32 GetHelpTargetNumber() const;
	FString GetHelpCallsign() const;

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

	// ---- A mission from the menu (ASimMissionMenuHUD): the player starts in a seat; Esc opens the mission's menu.

	/** Start in this station's seat (the menu's choice): into the cabin at that seat, the seat taken from its NPC, once
	 *  the cabin is built. "" stays outside. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Missions")
	void StartInSeat(const FString& SeatId);

	/** The mission's menu: the sim paused (single player), resume, restart, the missions, quit; with the debrief once the
	 *  scenario has ended. Esc opens and closes it. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Missions")
	void SetMissionMenuOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Missions")
	bool IsMissionMenuOpen() const { return bMissionMenuOpen; }

	/** Restart and the missions menu are single player (a networked game only resumes or quits). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Missions")
	bool CanLeaveMission() const;

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
	bool IsCrossOnBoard() const { return bOnBoard; }
	float GetLastPlotMissCm() const { return LastPlotMissCm; }
	float GetLastPlotTime() const { return LastPlotTime; }

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
	/** The plotting board under the cross (the plotter's): the mouse wheel picks the number, a click writes it there. */
	void HandleBoard(class APlayerController* Pc, const FVector& Start, const FVector& Forward);

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

	/** The camera views on the F keys (docs/PLAYING.md). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USimViewComponent> Views;

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
	/** A lesson: Enter, L, its highlight in the cabin; returns the control the ghost hand should reach for. */
	FString HandleLesson(class APlayerController* Pc, USimWorldSubsystem* Sim, float DeltaSeconds);
	/** -SimTutorialAutoplay=<folder>: the lesson played as a learner would, through the same clicks, wheel turns and
	 *  menu lines, each step photographed as he sees it before it is done; the game quits at the end. */
	void RunLessonAutoplay(USimWorldSubsystem* Sim, const struct FSimTutorialStep& Step, float DeltaSeconds);
	/** The crew menu opens (O): this seat's orders or reports, on the page with the lesson's line if one is asked for. */
	void OpenCrewMenu(USimWorldSubsystem* Sim);
	/** The cabin seat with this station id (INDEX_NONE if none). */
	int32 FindStation(const FString& StationId) const;
	void ConfigureForScenario(USimWorldSubsystem* Sim);
	void HandlePost(class APlayerController* Pc);
	TArray<int32> PickList() const;
	static TArray<FSimCrewHotkey> DefaultCrewHotkeys();
	/** Shuts the daylight out of a closed compartment (the PU-12M's): the level's sky lights dark while the camera is in
	 *  it, their brightness back when it leaves. */
	void SetDaylightShut(bool bShut);

	double SpeedMps = 0.0;
	double HeadingDeg = 0.0;

	bool bInCabin = false;
	FString PendingSeat;
	bool bMissionMenuOpen = false;
	void HandleMissionMenu(APlayerController* Pc);
	int32 Station = 0;
	FString StationLabel;
	/** Standing at a far panel of the seat (L onto one of its controls beyond arm's reach): the eye's step from the seat
	 *  (cm, the vehicle's frame), for the seat StandStation only; another seat, or the cabin left, sits down again. */
	FVector StandOffset = FVector::ZeroVector;
	int32 StandStation = INDEX_NONE;
	/** Walking (X): the eye (cm, the vehicle's frame) and where he looks (yaw, pitch in the vehicle's frame). The seat he
	 *  got up from stays his (Station): his orders and reports, and the seat he holds. */
	bool bWalking = false;
	FVector WalkEye = FVector::ZeroVector;
	double WalkYawDeg = 0.0;
	double WalkPitchDeg = 0.0;

	/** Head turn from the station's view direction (degrees). */
	double LookYawDeg = 0.0;
	double LookPitchDeg = 0.0;
	int32 HoveredItem = INDEX_NONE;
	int32 DraggedWheel = INDEX_NONE;
	/** Holding a hand wheel turns the head onto the display it is worked against (the layout's "watch"), and back to
	 *  where it was when the wheel is let go: the look to turn to, and the look to come back to. */
	bool bWheelWatch = false;
	bool bWatchReturn = false;
	double WatchYawDeg = 0.0;
	double WatchPitchDeg = 0.0;
	double ReturnYawDeg = 0.0;
	double ReturnPitchDeg = 0.0;
	FString HoverText;
	FString FeedbackText;
	float FeedbackSecondsLeft = 0.0f;

	bool bViewOverride = false;
	FVector OverrideLocation = FVector::ZeroVector;
	FRotator OverrideRotation = FRotator::ZeroRotator;

	bool bAssist = false;
	float AssistClock = 0.0f;
	FString AssistLevel;
	FSimMoveVerdict LastVerdict;
	FSimSeatHelp Help;
	/** The first move the helper shows now (null: none shown). */
	const FSimHelpStep* HelpTop() const;
	bool bMenuOpen = false;
	int32 MenuPage = 0;
	TArray<FSimCrewMenuEntry> MenuEntries;
	FString StationId;
	bool bHoldingSeat = false;

	/** The runner the vehicle is configured for (in a battery link, the player may move to the other vehicle). */
	const void* ConfiguredFor = nullptr;
	bool bIsPost = false;
	/** The sky lights dimmed by SetDaylightShut, with their own intensities. */
	TMap<TWeakObjectPtr<class USkyLightComponent>, float> ShutSkyLights;
	int32 PickedTarget = INDEX_NONE;
	/** The cross is on the plotting board (HandleBoard): the HUD draws the picked number beside it. */
	bool bOnBoard = false;
	/** The last mark written by hand: how far the click was (cm), and when (world seconds), for a moment's feedback. */
	float LastPlotMissCm = -1.0f;
	float LastPlotTime = -100.0f;

	FString AutoplayFolder;
	bool bAutoplayAsked = false;
	int32 AutoStep = INDEX_NONE;
	float AutoClock = 0.0f;
	bool bAutoLooked = false;
	bool bAutoShot = false;
	bool bAutoUi = false;
	bool bAutoReachChecked = false;
	bool bAutoWatched = false;
	int32 AutoClicks = 0;
	float AutoEndClock = -1.0f;
	/** The autoplay's hands on the controls: throttle forward, and the brake. */
	double AutoThrottle = 0.0;
	bool bAutoBrake = false;

	bool bTouring = false;
	FString ShotFolder;
	float ShotDelayLeft = -1.0f;
	int32 ShotTaken = INDEX_NONE;
	float TourSecondsPerView = 6.0f;
	float TourClock = 0.0f;
	int32 TourIndex = 0;
	TArray<FSimCabinTourView> TourViews;
};
