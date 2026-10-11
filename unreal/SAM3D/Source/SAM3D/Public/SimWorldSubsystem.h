#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"
#include "SimEmLayer.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWorldSubsystem.generated.h"

class ACesium3DTileset;
class ACesiumGeoreference;

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

/** State of one piece of equipment behind the console (simcore::SubsystemState). */
UENUM(BlueprintType)
enum class ESimSubsystemState : uint8
{
	Off,
	Starting,
	Ready,
	Stopping,
	Fault
};

/** One line of the session debrief: crew report, fault, deviation, sensor event, launch, hit or miss. */
USTRUCT(BlueprintType)
struct SAM3D_API FSimDebriefEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Scenario")
	double TimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Scenario")
	FString Kind;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Scenario")
	FString Who;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Scenario")
	FString What;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Scenario")
	FString Detail;
};

/** What happened when the player moved a control (simcore::StepResult). */
UENUM(BlueprintType)
enum class ESimPlayerActionResult : uint8
{
	/** Moved; no procedure is being judged (free play), or it is already complete. */
	Moved,
	/** Moved; the correct next step of the player's procedure. */
	Accepted,
	/** Moved; the correct step, but before its lamp/condition. Counted in the debrief. */
	AcceptedEarly,
	/** Moved; not the step that was due. Counted in the debrief. */
	OutOfOrder,
	/** Not moved: the correct step, but too soon after the previous one. */
	NotYet,
	/** Not moved: a closed cover or lock, or a key switch jumping a position. */
	Blocked,
	/** Not moved: no such control or position, or no scenario running. */
	Invalid
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimEvent, const FSimEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimDebriefEntry, const FSimDebriefEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimTerrainReady, bool, bSucceeded);

/**
 * Owns the simulation for a game world and advances it at a fixed rate.
 *
 * Two modes:
 *   - Demo: a bare simcore::World with the generic placeholder radar/missile demo (StartDemoScenario).
 *   - Scenario: a simcore::ScenarioRunner loaded from data/scenarios/<file>.json, with the unit's console,
 *     crew, gameplay engagement model and debrief (LoadScenario). The console API below works in this mode.
 *
 * Coordinates: simcore works in ENU metres at the scenario origin. With a Cesium georeference set
 * (SetGeoreference), positions convert ENU -> lat/lon/height -> Unreal through the georeference, which also
 * handles Earth curvature. Without one, a flat mapping is used: Unreal X = North, Y = East, Z = Up (cm).
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

	// ------------------------------------------------------------------ demo mode

	/** Create a fresh demo simulation anchored at a geodetic origin. Ends any running scenario. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void ResetSimulation(double OriginLatitudeDeg, double OriginLongitudeDeg, double OriginAltitudeM);

	/** One generic radar + launcher at the origin vs. one inbound target, with automatic fire control. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void StartDemoScenario();

	// ------------------------------------------------------------------ scenario mode

	/**
	 * Load and start a scenario. ScenarioFile is relative to the data folder (e.g. "scenarios/osa_practice.json").
	 * The data folder is the repo's data/ (two levels above the .uproject) unless DataDirectory is set.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Scenario")
	bool LoadScenario(const FString& ScenarioFile, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	bool IsScenarioRunning() const { return Runner.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	bool IsScenarioFinished() const;

	/** Scenario origin as (longitude, latitude, height) in degrees and metres, the order Cesium uses. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	FVector GetOriginLongitudeLatitudeHeight() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	FString GetDebriefText() const;

	/** Fired for every new debrief line while a scenario runs. */
	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Scenario")
	FOnSimDebriefEntry OnDebriefEntry;

	// The unit (the player's vehicle).
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Scenario")
	void SetUnitLocation(const FVector& UnrealLocation);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	FVector GetUnitLocation() const;

	/** The hull's true heading, degrees clockwise from north (Unreal yaw, X = north). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Scenario")
	void SetUnitHeading(double HeadingDeg);

	/** True heading (degrees clockwise from north) of an Unreal yaw at a place. Unreal yaw is only clockwise from north
	 *  in the flat mapping (X = north); with the Cesium georeference X points east at the origin and the frame turns
	 *  with the Earth, so convert through here. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Sim")
	double YawToHeadingDeg(const FVector& Location, double YawDeg) const;

	/** The Unreal yaw at a place that points along a true heading (degrees clockwise from north). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Sim")
	double HeadingToYawDeg(const FVector& Location, double HeadingDeg) const;

	/** The hull's heading the sim has: the scenario's own_unit.heading_deg until the vehicle drives. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	double GetUnitHeading() const;

	/**
	 * An azimuth in the crew's frame (the azimuth wheel, the search scope) as true azimuth from north. They differ
	 * until the crew has oriented the vehicle's gyro half-compass.
	 */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Scenario")
	double IndicatedToTrueAzimuth(double IndicatedDeg) const;

	// ------------------------------------------------------------------ console (scenario mode)

	/** Position of a switch, selector, key or button; -1 if unknown or a wheel. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	int32 GetControlPosition(const FString& ControlId) const;

	/** Move a switch/selector/key or press a button (1). False if blocked by a cover or lock, or invalid. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Console")
	bool SetControlPosition(const FString& ControlId, int32 Position);

	/** Change the early-warning / command source the crew listens to (a source id from the scenario's command network,
	 *  or "none" for the vehicle's own radar alone). The PU-12 is "pu12". False if unknown or already listened to. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Command")
	bool RequestC2Source(const FString& SourceId, const FString& Why);

	/** The source the crew listens to now ("" while changing or off the net). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Command")
	FString GetActiveC2Source() const;

	/** Ids of the scenario's early-warning / command sources (PU-12 first). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Command")
	TArray<FString> GetC2SourceIds() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	double GetWheelValue(const FString& ControlId) const;

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Console")
	bool SetWheelValue(const FString& ControlId, double Value);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	ESimSubsystemState GetSubsystemState(const FString& SubsystemId) const;

	/** 0 = fully off / stowed, 1 = fully ready / deployed. Drives deploy/stow animations. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	double GetSubsystemProgress(const FString& SubsystemId) const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	bool GetSimInput(const FString& InputId) const;

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Console")
	bool SetSimInput(const FString& InputId, bool bValue);

	/** Give a crew member a procedure: bQueue = after current orders, otherwise drop everything. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Console")
	bool OrderCrew(const FString& CrewName, const FString& ProcedureId, bool bQueue);

	/** Target currently tracked by the tracking radar or TV ("" if none). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Console")
	FString GetTrackedTarget() const;

	// ------------------------------------------------------------------ player and cabin (scenario mode)

	/**
	 * Load the 3D cabin layout for the scenario's console (relative to the data folder, e.g. "cabins/osa_ak.json").
	 * Load the scenario first; loading a scenario drops the cabin.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool LoadCabin(const FString& CabinFile, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool HasCabin() const { return Cabin.IsValid(); }

	/**
	 * The player's hand on a switch, selector, key or button. Goes through the player's procedure tracker, so
	 * early and out-of-order actions reach the debrief (SetControlPosition bypasses it; use that for scripts).
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	ESimPlayerActionResult PlayerSetControl(const FString& ControlId, int32 Position);

	/** The player turning a hand wheel. False if its lock is closed or it isn't a wheel. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool PlayerTurnWheel(const FString& ControlId, double Value);

	/** Judge the player's actions against a procedure of the console; empty = free play. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool SetPlayerProcedure(const FString& ProcedureId);

	/** Whether a lamp of the cabin layout is lit now. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsLampLit(const FString& LampId) const;

	/** The loaded cabin layout (null without one). */
	const simcore::CabinLayout* GetCabinLayout() const { return Cabin.Get(); }

	// ------------------------------------------------------------------ terrain and Cesium

	/** Use this georeference for all ENU <-> Unreal conversions (null = flat mapping). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cesium")
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	/**
	 * Sample terrain heights from a Cesium tileset on a square grid around the scenario origin
	 * and give them to the simulation (line of sight, target heights). Asynchronous: OnTerrainReady fires.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cesium")
	bool RequestCesiumTerrain(ACesium3DTileset* Tileset, double HalfSizeMeters = 30000.0, double SpacingMeters = 300.0);

	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Cesium")
	FOnSimTerrainReady OnTerrainReady;

	// ------------------------------------------------------------------ electromagnetic layer

	/**
	 * Start the electromagnetic-layer demo (simcore::em: radar, ESM, jamming, C2, knowledge) at the current sim
	 * origin and terrain. Overlays: console variable sam3d.em.Debug. Console: sam3d.em.Start / sam3d.em.Stop.
	 */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|EM")
	bool StartEmDemo(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "SAM3D|EM")
	void StopEmDemo();

	UFUNCTION(BlueprintPure, Category = "SAM3D|EM")
	bool IsEmDemoRunning() const;

	/** Simulated time of the EM layer (s). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|EM")
	double GetEmTimeSeconds() const;

	/** Nodes (C2 status, knowledge size) and radars (on/off, tracks), one per line. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|EM")
	FString GetEmStatusText() const;

	const FSimEmLayer* GetEmLayer() const { return EmLayer.Get(); }

	// ------------------------------------------------------------------ both modes

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

	/** Time compression: how fast sim time runs against real time, 0.25 to 32 (1 = real time). The same value is the
	 *  console variable sam3d.TimeScale. A cold start takes minutes of sim time (roadmap 6.2). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Sim")
	void SetTimeScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Sim")
	float GetTimeScale() const;

	/** Fired for every simcore World event in demo mode (launch, detonation, kill...). */
	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Sim")
	FOnSimEvent OnSimEvent;

	/** Absolute path of the data folder; empty = <repo>/data next to the unreal/ folder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Scenario")
	FString DataDirectory;

	/** Direct access for C++ gameplay code. */
	simcore::World* GetSimWorld() const;
	simcore::ScenarioRunner* GetScenarioRunner() const { return Runner.Get(); }

	/** ENU metres -> Unreal cm, and back (through the georeference when set). */
	FVector ToUnreal(const simcore::Vec3& Enu) const;
	simcore::Vec3 ToEnu(const FVector& Unreal) const;

	/** Flat mapping without a georeference: ENU (x=E, y=N, z=U) m <-> Unreal (X=N, Y=E, Z=U) cm. */
	static FVector EnuToUnreal(const simcore::Vec3& Enu);
	static simcore::Vec3 UnrealToEnu(const FVector& Unreal);

private:
	FString ResolveDataDirectory() const;
	void RunDemoFireControl();
	void BroadcastEvents();
	void BroadcastNewDebriefEntries();
	FSimEntityState MakeState(int32 Id, const FString& Name, bool bMissile, bool bAlive, const simcore::Vec3& Pos,
		const simcore::Vec3& Vel) const;

	const simcore::World* ActiveWorld() const;
	const simcore::LocalFrame* ActiveFrame() const;

	// Demo mode.
	TUniquePtr<simcore::World> SimWorld;
	double Accumulator = 0.0;
	double FixedStepSeconds = 0.01;
	bool bPaused = false;
	bool bDemoActive = false;
	bool bDemoLaunched = false;
	int32 DemoTargetId = -1;
	double DemoLaunchRangeM = 12000.0;
	simcore::Vec3 DemoSite;
	simcore::RadarParams DemoRadar;
	simcore::MissileParams DemoMissile;
	simcore::FlatTerrain DemoGround{0.0};

	// Scenario mode.
	TUniquePtr<simcore::ScenarioRunner> Runner;
	TUniquePtr<simcore::CabinLayout> Cabin;
	TUniquePtr<simcore::GridTerrain> SampledTerrain;
	double ScenarioStepSeconds = 0.05;
	size_t DebriefEntriesSent = 0;

	TWeakObjectPtr<ACesiumGeoreference> Georeference;
	int32 TerrainRequestSerial = 0;

	// Electromagnetic layer (runs on the active sim world's origin and terrain; stopped when they change).
	TUniquePtr<FSimEmLayer> EmLayer;
};
