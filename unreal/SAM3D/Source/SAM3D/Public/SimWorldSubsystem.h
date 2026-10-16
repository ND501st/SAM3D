#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

#include <map>
#include "SimEmLayer.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWorldSubsystem.generated.h"

class ACesium3DTileset;
class USimKnowledgeComponent;
class USimCrewSeatComponent;
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

	/** A ground vehicle of the player's battery, or a dummy position (roadmap 7.7). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	bool bIsGround = false;

	/** launcher, reload, command_post, maintenance, dummy (ground entities only). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Sim")
	FString Kind;
};

/** One mark on the PU-12 plotting board (samsim_osa p20 notation). */
USTRUCT(BlueprintType)
struct SAM3D_API FSimBoardMark
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Board")
	int32 Number = 0;

	/** The plot (minute) it was written at: 1, 2, ... */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Board")
	int32 Stamp = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Board")
	int32 HeightHm = 0;

	/** Type and formation digits (e.g. 71: practice target, one aircraft). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Board")
	int32 Code = 0;

	/** Metres east and north of the battery. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Board")
	FVector2D EastNorthMeters = FVector2D::ZeroVector;
};

/** A radio or intercom message the crew heard. */
USTRUCT(BlueprintType)
struct SAM3D_API FSimRadioMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Radio")
	double TimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Radio")
	FString Net;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Radio")
	FString From;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Radio")
	FString Text;
};

/** A seat's best next move from the drill master (docs/crew/06_REFERENCE_PERFORMER.md): the assistance overlay's data. */
USTRUCT(BlueprintType)
struct SAM3D_API FSimDrillAdvice
{
	GENERATED_BODY()

	/** False when the seat has nothing to do toward the task (or there is no crew data). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Station;

	/** The control to move (highlight it in the cabin) and the position to set. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Control;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	int32 Position = 0;

	/** The drill it belongs to ("" for a single switch). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Procedure;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Description;

	/** When the ideal crew does it, seconds from now (about the reaction time: now). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	double DueInSeconds = 0.0;

	/** Why, nearest reason first, up to the task. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	TArray<FString> Why;
};

/** One line of the crew menu: an order (the commander's seat) or a report (any other seat), from the crew data. */
USTRUCT(BlueprintType)
struct SAM3D_API FSimCrewMenuEntry
{
	GENERATED_BODY()

	/** The callout id (GiveCrewOrder for an order, PlayerSay for a report). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString Ru;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	FString En;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Crew")
	bool bOrder = false;
};

/** One of the battery's other launchers in the fight (battery_fire.hpp). */
USTRUCT(BlueprintType)
struct SAM3D_API FSimBatteryShooter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Battery")
	FString Callsign;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Battery")
	int32 Missiles = 0;

	/** Its target ("" for none), and what it is doing: idle, acquiring, tracking, preparing, in flight. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Battery")
	FString Target;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Battery")
	FString Phase;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Battery")
	bool bDeployed = false;
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
	Invalid,
	/** Multiplayer crew, network client: sent to the server; the console shows the result when the next snapshot comes. */
	Sent
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

	/** Turn R-123M set 0 (left) or 1 (right) to preset 1-4. Takes a few seconds in the sim. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Command")
	bool TuneC2Radio(int32 Set, int32 Preset);

	/** Take the headset to set 0 (left) or 1 (right): the crew then hears the source on that set's preset. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Command")
	bool ListenToC2Radio(int32 Set);

	/** The preset (1-4) a set is on, 0 if there is no command network. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Command")
	int32 GetC2RadioPreset(int32 Set) const;

	/** The source id on a preset (1-4), "" if free. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Command")
	FString GetC2PresetSource(int32 Preset) const;

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

	/** Give a whole order of the crew data as the commander (the order menu, docs/crew/07_COMMANDER_MODE.md), e.g.
	 *  "order_k_boyu": its drills go to their seats and the words are spoken. False if there is no such order. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	bool GiveCrewOrder(const FString& OrderId);

	/** The drill master's best next move for a seat toward a task of the crew data (e.g. "ready_halted"): the
	 *  training overlay's highlight. Runs the ideal crew on a copy of the console: call it when the state changes or
	 *  the player asks for help, not every frame. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	FSimDrillAdvice GetDrillAdvice(const FString& Station, const FString& TaskId) const;

	/** Whether a task of the crew data holds now (e.g. "ready_halted"). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	bool IsCrewTaskDone(const FString& TaskId) const;

	/** The crew menu for a seat: the orders of the crew data at the commander's seat, else the seat's own reports. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	TArray<FSimCrewMenuEntry> GetCrewMenu(const FString& Station) const;

	/** A player in a seat speaks a report (filled from his own instruments: a contact from his scope). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	bool PlayerSay(const FString& Station, const FString& CalloutId);

	/** The battery commander's fire distribution: give one of the battery's other launchers a target (the battery net). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Battery")
	bool AssignBatteryTarget(const FString& Callsign, const FString& Target);

	/** The battery's other launchers: missiles, target, phase. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Battery")
	TArray<FSimBatteryShooter> GetBatteryShooters() const;

	/** Who holds a seat ("" for the NPC). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetCrewSeatHolder(const FString& Station) const;

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

	// ------------------------------------------------------------------ multiplayer knowledge (roadmap E10)

	/** Server: a player's knowledge component starts or stops receiving its node's view (USimKnowledgeComponent). */
	void RegisterKnowledgeClient(USimKnowledgeComponent* Client);
	void UnregisterKnowledgeClient(USimKnowledgeComponent* Client);
	/** Server: the player holds view `Seq`; the next delta is taken from it. */
	void AckKnowledge(USimKnowledgeComponent* Client, uint32 Seq);
	/** Server: send each registered player its view when its period is up (called after the sim steps). */
	void PumpKnowledge();

	/** The local player's knowledge stream (its scope block feeds the cabin's scopes on a network client). */
	void SetLocalKnowledge(USimKnowledgeComponent* Knowledge);
	void ClearLocalKnowledge(USimKnowledgeComponent* Knowledge);
	/** Network client: the scopes as the server's last view gave them (null: none yet). */
	const simcore::em::ViewScope* GetMirroredScope() const;

	/** The EM scene players' knowledge comes from: the scenario's EM layer, else the EM demo (null: none). */
	const simcore::em::EmScene* GetActiveEmScene() const;

	// ------------------------------------------------------------------ multiplayer crew

	/** Server: players' seat components (console snapshots go to each). */
	void RegisterCrewSeat(USimCrewSeatComponent* Seat);
	void UnregisterCrewSeat(USimCrewSeatComponent* Seat);
	/** The local player's seat (cabin clicks go through it when it holds a seat). */
	void SetLocalCrewSeat(USimCrewSeatComponent* Seat);
	void ClearLocalCrewSeat(USimCrewSeatComponent* Seat);
	/** Server: a player takes or leaves a crew seat (a console station id). */
	bool TakeCrewSeat(const FString& Role, const FString& Player);
	void LeaveCrewSeat(const FString& Role);
	/** Server: a seated player's control move / wheel turn. True if the control moved; otherwise Why says why not. */
	bool SeatSetControl(const FString& Role, const FString& ControlId, int32 Position, FString& Why);
	bool SeatTurnWheel(const FString& Role, const FString& ControlId, double Value, FString& Why);
	/** Network client: overwrite the local console with the server's snapshot if it is newer than Seq. */
	bool ApplyConsoleSnapshot(const TArray<uint8>& Bytes, uint32& Seq);
	/** Server: console snapshots and seat procedures to seated players (called after the sim steps). */
	void PumpConsole();

	// ------------------------------------------------------------------ battery, board and radio (roadmap 7.7)

	/** The PU-12 plotting board's marks of the last five plots (on a network client: from the knowledge stream). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Board")
	TArray<FSimBoardMark> GetBoardMarks() const;

	/** The board's range rings (m) and its current plot number. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Board")
	TArray<float> GetBoardRangeRings() const;

	/** The last radio and intercom messages, oldest first. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Radio")
	TArray<FSimRadioMessage> GetRadioMessages(int32 MaxCount = 8) const;

	/** Who holds a crew seat ("" the NPC). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetSeatHolder(const FString& Role) const;

	/** True on a network client: the simulation runs on the server, and this machine only mirrors it. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	bool IsMirrorOnly() const;

	/** The node a knowledge component with NodeId 0 sees: the scenario unit's own node (0: none). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	int32 GetDefaultKnowledgeNode() const;

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
	/** The board, radio and battery extras: built from the runner here, or the mirrored view on a client. */
	const simcore::em::ViewExtras* GetViewExtras(simcore::em::ViewExtras& Scratch) const;
	/** Where the battery (the player's vehicle) is, in ENU. */
	simcore::Vec3 BatteryCentreEnu() const;
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

	// Multiplayer knowledge: one stream per registered player (simcore/em/replication.hpp).
	TUniquePtr<simcore::em::KnowledgeReplicator> Replicator;
	TArray<TWeakObjectPtr<USimKnowledgeComponent>> KnowledgeClients;
	double LastKnowledgeSimTime = -1.0;
	std::map<int32, simcore::EchoKeys> EchoKeysByClient;  // opaque echo keys per player
	TWeakObjectPtr<USimKnowledgeComponent> LocalKnowledge;

	// Multiplayer crew.
	TArray<TWeakObjectPtr<USimCrewSeatComponent>> CrewSeats;
	TWeakObjectPtr<USimCrewSeatComponent> LocalCrewSeat;
	double LastConsoleSimTime = -1.0;
};
