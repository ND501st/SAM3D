#include "SimWorldSubsystem.h"

#include "Misc/FileHelper.h"

#include <algorithm>

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumSampleHeightResult.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimCrewSeatComponent.h"
#include "SimKnowledgeComponent.h"

namespace
{
	constexpr double MetersToCm = 100.0;
	constexpr int32 MissileIdBase = 100000;  // display ids for engagement-model missiles
	constexpr int32 GroundIdBase = 200000;   // display ids for the battery's vehicles and dummy positions

	ESimEventType ToUnrealEventType(simcore::EventType Type)
	{
		switch (Type)
		{
		case simcore::EventType::MissileLaunched: return ESimEventType::MissileLaunched;
		case simcore::EventType::MissileDetonated: return ESimEventType::MissileDetonated;
		case simcore::EventType::MissileMissed: return ESimEventType::MissileMissed;
		case simcore::EventType::MissileSelfDestructed: return ESimEventType::MissileSelfDestructed;
		case simcore::EventType::MissileHitGround: return ESimEventType::MissileHitGround;
		case simcore::EventType::AircraftDestroyed: return ESimEventType::AircraftDestroyed;
		}
		return ESimEventType::MissileLaunched;
	}

	ESimSubsystemState ToUnrealState(simcore::SubsystemState State)
	{
		switch (State)
		{
		case simcore::SubsystemState::Off: return ESimSubsystemState::Off;
		case simcore::SubsystemState::Starting: return ESimSubsystemState::Starting;
		case simcore::SubsystemState::Ready: return ESimSubsystemState::Ready;
		case simcore::SubsystemState::Stopping: return ESimSubsystemState::Stopping;
		case simcore::SubsystemState::Fault: return ESimSubsystemState::Fault;
		}
		return ESimSubsystemState::Off;
	}

	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

// ---------------------------------------------------------------------------------------------- coordinates

FVector USimWorldSubsystem::EnuToUnreal(const simcore::Vec3& Enu)
{
	// ENU (x=E, y=N, z=U) metres -> Unreal (X=N, Y=E, Z=U) centimetres.
	return FVector(Enu.y * MetersToCm, Enu.x * MetersToCm, Enu.z * MetersToCm);
}

simcore::Vec3 USimWorldSubsystem::UnrealToEnu(const FVector& Unreal)
{
	return simcore::Vec3(Unreal.Y / MetersToCm, Unreal.X / MetersToCm, Unreal.Z / MetersToCm);
}

FVector USimWorldSubsystem::ToUnreal(const simcore::Vec3& Enu) const
{
	const simcore::LocalFrame* Frame = ActiveFrame();
	ACesiumGeoreference* Geo = Georeference.Get();
	if (Frame == nullptr || Geo == nullptr)
	{
		return EnuToUnreal(Enu);
	}
	const simcore::GeoPoint G = Frame->EnuToGeodetic(Enu);
	return Geo->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(G.LonDeg(), G.LatDeg(), G.alt_m));
}

simcore::Vec3 USimWorldSubsystem::ToEnu(const FVector& Unreal) const
{
	const simcore::LocalFrame* Frame = ActiveFrame();
	ACesiumGeoreference* Geo = Georeference.Get();
	if (Frame == nullptr || Geo == nullptr)
	{
		return UnrealToEnu(Unreal);
	}
	const FVector Llh = Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Unreal);
	return Frame->GeodeticToEnu(simcore::GeoPoint::FromDegrees(Llh.Y, Llh.X, Llh.Z));
}

void USimWorldSubsystem::SetGeoreference(ACesiumGeoreference* InGeoreference)
{
	Georeference = InGeoreference;
}

const simcore::World* USimWorldSubsystem::ActiveWorld() const
{
	if (Runner)
	{
		return &Runner->GetWorld();
	}
	return SimWorld.Get();
}

simcore::World* USimWorldSubsystem::GetSimWorld() const
{
	if (Runner)
	{
		return &Runner->GetWorld();
	}
	return SimWorld.Get();
}

const simcore::LocalFrame* USimWorldSubsystem::ActiveFrame() const
{
	const simcore::World* W = ActiveWorld();
	return W ? &W->Frame() : nullptr;
}

// ---------------------------------------------------------------------------------------------- lifecycle

void USimWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetSimulation(45.0, 25.0, 0.0);
}

void USimWorldSubsystem::Deinitialize()
{
	EmLayer.Reset();
	ClearScenario();
	SampledTerrain.Reset();
	SimWorld.Reset();
	Super::Deinitialize();
}

bool USimWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USimWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimWorldSubsystem, STATGROUP_Tickables);
}

namespace
{
TAutoConsoleVariable<float> CVarSimTimeScale(TEXT("sam3d.TimeScale"), 1.0f,
	TEXT("Time compression: how fast sim time runs against real time, 0.25 to 32 (1 = real time)."), ECVF_Default);

constexpr float MinTimeScale = 0.25f;
constexpr float MaxTimeScale = 32.0f;
}

float USimWorldSubsystem::GetTimeScale() const
{
	return FMath::Clamp(CVarSimTimeScale.GetValueOnGameThread(), MinTimeScale, MaxTimeScale);
}

void USimWorldSubsystem::SetTimeScale(float Scale)
{
	CVarSimTimeScale->Set(FMath::Clamp(Scale, MinTimeScale, MaxTimeScale), ECVF_SetByCode);
}

void USimWorldSubsystem::Tick(float DeltaTime)
{
	// After the sim has stepped (whichever way this function returns), the server sends players their knowledge.
	struct FPumpAfterStep
	{
		USimWorldSubsystem* Self;
		~FPumpAfterStep()
		{
			Self->PumpKnowledge();
			Self->PumpConsole();
		}
	} PumpAfterStep{this};

	const double Scale = GetTimeScale();
	if (EmLayer && EmLayer->IsRunning())
	{
		if (!bPaused)
		{
			if (!IsMirrorOnly())
			{
				EmLayer->Advance(DeltaTime * Scale);
			}
		}
		EmLayer->Draw(GetWorld(), [this](const simcore::Vec3& Enu) { return ToUnreal(Enu); });
	}
	if (bPaused || IsMirrorOnly())
	{
		return;  // a network client mirrors the server's simulation; it never steps its own
	}
	// Fixed-step simulation, decoupled from frame rate and sped up by the time scale. Clamp the frame time to avoid a
	// spiral of death after hitches; the step caps grow with the scale so compression isn't lost to them.
	Accumulator += FMath::Min(static_cast<double>(DeltaTime), 0.25) * Scale;
	const int32 ScaleSteps = FMath::CeilToInt32(FMath::Max(1.0, Scale));

	if (Runner)
	{
		int32 Steps = 0;
		while (Accumulator >= ScenarioStepSeconds && Steps < 20 * ScaleSteps && !(Link ? Link->Finished() : Runner->Finished()))
		{
			if (Link)
			{
				Link->Step(ScenarioStepSeconds);  // both vehicles of the battery
			}
			else
			{
				Runner->Step(ScenarioStepSeconds);
			}
			Accumulator -= ScenarioStepSeconds;
			++Steps;
		}
		BroadcastNewDebriefEntries();
		return;
	}

	if (!SimWorld)
	{
		return;
	}
	int32 Steps = 0;
	while (Accumulator >= FixedStepSeconds && Steps < 100 * ScaleSteps)
	{
		if (bDemoActive)
		{
			RunDemoFireControl();
		}
		SimWorld->Step(FixedStepSeconds);
		Accumulator -= FixedStepSeconds;
		++Steps;
	}
	BroadcastEvents();
}

// ---------------------------------------------------------------------------------------------- demo mode

void USimWorldSubsystem::ResetSimulation(double OriginLatitudeDeg, double OriginLongitudeDeg, double OriginAltitudeM)
{
	StopEmDemo();  // it runs on the old world's frame and terrain
	ClearScenario();
	SampledTerrain.Reset();
	// Flat ground at the origin's altitude.
	DemoGround = simcore::FlatTerrain(OriginAltitudeM);
	SimWorld = MakeUnique<simcore::World>(
		simcore::GeoPoint::FromDegrees(OriginLatitudeDeg, OriginLongitudeDeg, OriginAltitudeM), &DemoGround);
	Accumulator = 0.0;
	bDemoActive = false;
	bDemoLaunched = false;
	DemoTargetId = -1;
	UE_LOG(LogSAM3D, Log, TEXT("Sim reset at origin %.6f, %.6f, %.1f m"), OriginLatitudeDeg, OriginLongitudeDeg, OriginAltitudeM);
}

void USimWorldSubsystem::StartDemoScenario()
{
	if (!SimWorld)
	{
		return;
	}

	// Generic placeholder parameters (same as simcore/examples/engagement_demo.cpp).
	DemoRadar = simcore::RadarParams();
	DemoRadar.peak_power_w = 100e3;
	DemoRadar.antenna_gain_db = 35.0;
	DemoRadar.wavelength_m = 0.03;
	DemoRadar.bandwidth_hz = 1e6;
	DemoRadar.noise_figure_db = 5.0;
	DemoRadar.system_losses_db = 6.0;
	DemoRadar.detection_threshold_db = 13.0;
	DemoRadar.max_instrumented_range_m = 45000.0;

	DemoMissile = simcore::MissileParams();
	DemoMissile.launch_mass_kg = 150.0;
	DemoMissile.burnout_mass_kg = 90.0;
	DemoMissile.boost_thrust_n = 30000.0;
	DemoMissile.boost_time_s = 2.5;
	DemoMissile.sustain_thrust_n = 4000.0;
	DemoMissile.sustain_time_s = 10.0;
	DemoMissile.reference_area_m2 = 0.025;
	DemoMissile.drag_coefficient = 0.35;
	DemoMissile.max_lateral_accel_mps2 = 25.0 * simcore::kStandardGravity;
	DemoMissile.navigation_constant = 4.0;
	DemoMissile.proximity_fuze_radius_m = 15.0;
	DemoMissile.lethal_radius_m = 10.0;
	DemoMissile.max_flight_time_s = 40.0;
	DemoMissile.min_guided_speed_mps = 250.0;

	DemoSite = simcore::Vec3(0.0, 0.0, 5.0);

	simcore::Aircraft Target;
	Target.name = "Bandit 1";
	Target.position = simcore::Vec3(-6000.0, 40000.0, 3000.0);
	Target.speed_mps = 250.0;
	Target.rcs_m2 = 5.0;
	Target.max_turn_rate_rad_s = simcore::DegToRad(4.0);
	Target.waypoints = {simcore::Vec3(-1500.0, 15000.0, 2500.0), simcore::Vec3(2000.0, -30000.0, 2500.0)};
	DemoTargetId = SimWorld->AddAircraft(Target);

	bDemoActive = true;
	bDemoLaunched = false;
	UE_LOG(LogSAM3D, Log, TEXT("Demo scenario started: target id %d"), DemoTargetId);
}

void USimWorldSubsystem::RunDemoFireControl()
{
	const simcore::Aircraft* Target = SimWorld->FindAircraft(DemoTargetId);
	if (Target == nullptr || !Target->alive || bDemoLaunched)
	{
		return;
	}
	const simcore::DetectionResult Det = simcore::EvaluateDetection(
		DemoRadar, SimWorld->Frame(), SimWorld->GetTerrain(), DemoSite, simcore::Vec3(), Target->position, Target->rcs_m2);
	if (Det.detected && Det.range_m <= DemoLaunchRangeM)
	{
		SimWorld->LaunchMissile(DemoMissile, DemoSite, Det.azimuth_rad, Det.elevation_rad + simcore::DegToRad(10.0), DemoTargetId);
		bDemoLaunched = true;
	}
}

void USimWorldSubsystem::BroadcastEvents()
{
	for (const simcore::SimEvent& E : SimWorld->DrainEvents())
	{
		FSimEvent Out;
		Out.Type = ToUnrealEventType(E.type);
		Out.TimeSeconds = E.time_s;
		Out.MissileId = E.missile_id;
		Out.AircraftId = E.aircraft_id;
		Out.MissDistanceMeters = E.miss_distance_m;
		UE_LOG(LogSAM3D, Log, TEXT("t=%.2fs event %d missile %d aircraft %d miss %.1f m"),
			Out.TimeSeconds, static_cast<int32>(Out.Type), Out.MissileId, Out.AircraftId, Out.MissDistanceMeters);
		OnSimEvent.Broadcast(Out);
	}
}

// ---------------------------------------------------------------------------------------------- scenario mode

FString USimWorldSubsystem::ResolveDataDirectory() const
{
	if (!DataDirectory.IsEmpty())
	{
		return DataDirectory;
	}
	// <repo>/unreal/SAM3D/SAM3D.uproject -> <repo>/data
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("data")));
}

void USimWorldSubsystem::ClearScenario()
{
	Runner = nullptr;
	Link.Reset();
	OwnedRunner.Reset();
}

bool USimWorldSubsystem::LoadScenario(const FString& ScenarioFile, FString& OutError)
{
	const FString DataDir = ResolveDataDirectory();
	const FString Path = FPaths::Combine(DataDir, ScenarioFile);
	std::string Error;
	// A battery link (a file with "link_id": two scenarios, the Osa's and the post's) or one scenario.
	FString Text;
	const bool bLink = FFileHelper::LoadFileToString(Text, *Path) && Text.Contains(TEXT("\"link_id\""));
	TUniquePtr<simcore::BatteryLink> NewLink;
	TUniquePtr<simcore::ScenarioRunner> NewRunner;
	if (bLink)
	{
		std::unique_ptr<simcore::BatteryLink> Loaded = simcore::LoadBatteryLinkFile(ToStd(Path), ToStd(DataDir), &Error);
		if (Loaded)
		{
			NewLink = TUniquePtr<simcore::BatteryLink>(Loaded.release());
		}
	}
	else
	{
		simcore::ScenarioDef Def;
		simcore::ConsoleData Console;
		if (simcore::LoadScenarioFile(ToStd(Path), ToStd(DataDir), Def, Console, &Error))
		{
			NewRunner = MakeUnique<simcore::ScenarioRunner>(std::move(Def), std::move(Console));
		}
	}
	if (!NewLink && !NewRunner)
	{
		OutError = ToF(Error);
		UE_LOG(LogSAM3D, Error, TEXT("Scenario failed to load: %s"), *OutError);
		return false;
	}
	StopEmDemo();  // it runs on the old world's frame and terrain
	SimWorld.Reset();
	bDemoActive = false;
	SampledTerrain.Reset();
	Cabin.Reset();
	ClearScenario();
	if (NewLink)
	{
		Link = MoveTemp(NewLink);
		// The player starts in the Osa; -SimVehicle=post puts him in the PU-12M.
		FString Vehicle;
		FParse::Value(FCommandLine::Get(), TEXT("SimVehicle="), Vehicle);
		Runner = Vehicle == TEXT("post") ? &Link->Post() : &Link->Osa();
	}
	else
	{
		OwnedRunner = MoveTemp(NewRunner);
		Runner = OwnedRunner.Get();
	}
	Accumulator = 0.0;
	DebriefEntriesSent = 0;
	UE_LOG(LogSAM3D, Log, TEXT("Scenario loaded: %s (%s)%s"), *ToF(Runner->Def().name), *Path,
		Link ? *FString::Printf(TEXT(", a battery link: the player in %s"), Runner == &Link->Post() ? TEXT("the PU-12M") : TEXT("the Osa")) : TEXT(""));
	LoadCrewPresentation(DataDir);
	BroadcastNewDebriefEntries();
	return true;
}

bool USimWorldSubsystem::SwitchLinkedVehicle()
{
	// Networked, each player chooses his vehicle when he joins (-SimVehicle=post) and his seat says which; a mid-game
	// move would need his streams and his seat moved with him, so it is single player only.
	if (!Link || IsMirrorOnly() || (GetWorld() != nullptr && GetWorld()->GetNetMode() != NM_Standalone))
	{
		return false;
	}
	Runner = Runner == &Link->Osa() ? &Link->Post() : &Link->Osa();
	const FString DataDir = ResolveDataDirectory();
	const FString CabinFile = GetDefaultCabinFile();
	FString Error;
	if (CabinFile.IsEmpty() || !LoadCabin(CabinFile, Error))
	{
		Cabin.Reset();
	}
	LoadCrewPresentation(DataDir);
	DebriefEntriesSent = Runner->Log().Entries().size();  // its log from here on (not replayed)
	UE_LOG(LogSAM3D, Log, TEXT("Battery link: the player moves to %s"), Runner == &Link->Post() ? TEXT("the PU-12M") : TEXT("the Osa"));
	return true;
}

FString USimWorldSubsystem::GetOtherLinkedVehicle() const
{
	if (!Link)
	{
		return FString();
	}
	return Runner == &Link->Post() ? FString::Printf(TEXT("the Osa %s"), *ToF(Link->Callsign())) : FString(TEXT("the PU-12M"));
}

void USimWorldSubsystem::LoadCrewPresentation(const FString& DataDir)
{
	std::string Error;
	// The crew's bodies and voices (roadmap 8.10): presentation data, so a scenario runs without them.
	{
		TUniquePtr<simcore::BodyData> Bodies = MakeUnique<simcore::BodyData>();
		if (simcore::LoadBodyDataFile(ToStd(FPaths::Combine(DataDir, TEXT("crew/body.json"))), *Bodies, &Error))
		{
			CrewBodies = MoveTemp(Bodies);
		}
		else
		{
			CrewBodies.Reset();
			UE_LOG(LogSAM3D, Warning, TEXT("Crew bodies: %s"), *ToF(Error));
		}
		TUniquePtr<simcore::VoiceData> Voices = MakeUnique<simcore::VoiceData>();
		const simcore::CrewData* Crew = Runner->Def().has_crew_data ? &Runner->Def().crew_data : nullptr;
		if (simcore::LoadVoiceDataFile(ToStd(FPaths::Combine(DataDir, TEXT("crew/voices.json"))), Crew, *Voices, &Error))
		{
			CrewVoices = MoveTemp(Voices);
		}
		else
		{
			CrewVoices.Reset();
			UE_LOG(LogSAM3D, Warning, TEXT("Crew voices: %s"), *ToF(Error));
		}
	}
}

bool USimWorldSubsystem::RequestC2Source(const FString& SourceId, const FString& Why)
{
	simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	if (ActiveRunner == nullptr)
	{
		return false;
	}
	return ActiveRunner->RequestC2Source(std::string(TCHAR_TO_UTF8(*SourceId)), "Player", std::string(TCHAR_TO_UTF8(*Why)));
}

bool USimWorldSubsystem::TuneC2Radio(int32 Set, int32 Preset)
{
	simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	return ActiveRunner != nullptr && ActiveRunner->TuneC2Radio(Set, Preset, "Player");
}

bool USimWorldSubsystem::ListenToC2Radio(int32 Set)
{
	simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	return ActiveRunner != nullptr && ActiveRunner->ListenToC2Radio(Set, "Player");
}

int32 USimWorldSubsystem::GetC2RadioPreset(int32 Set) const
{
	const simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	if (ActiveRunner == nullptr || ActiveRunner->C2() == nullptr || Set < 0 || Set > 1)
	{
		return 0;
	}
	return ActiveRunner->C2()->Radios()[static_cast<size_t>(Set)].preset;
}

FString USimWorldSubsystem::GetC2PresetSource(int32 Preset) const
{
	const simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	if (ActiveRunner == nullptr || ActiveRunner->C2() == nullptr)
	{
		return FString();
	}
	return FString(UTF8_TO_TCHAR(ActiveRunner->C2()->PresetSource(Preset).c_str()));
}

FString USimWorldSubsystem::GetActiveC2Source() const
{
	const simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	if (ActiveRunner == nullptr || ActiveRunner->C2() == nullptr)
	{
		return FString();
	}
	return FString(UTF8_TO_TCHAR(ActiveRunner->C2()->ActiveId().c_str()));
}

TArray<FString> USimWorldSubsystem::GetC2SourceIds() const
{
	TArray<FString> Ids;
	const simcore::ScenarioRunner* ActiveRunner = GetScenarioRunner();
	if (ActiveRunner != nullptr && ActiveRunner->C2() != nullptr)
	{
		for (size_t i = 0; i < ActiveRunner->C2()->Size(); ++i)
		{
			Ids.Add(FString(UTF8_TO_TCHAR(ActiveRunner->C2()->Def(i).id.c_str())));
		}
	}
	return Ids;
}

bool USimWorldSubsystem::LoadCabin(const FString& CabinFile, FString& OutError)
{
	if (!Runner)
	{
		OutError = TEXT("load a scenario first");
		return false;
	}
	const FString Path = FPaths::Combine(ResolveDataDirectory(), CabinFile);
	TUniquePtr<simcore::CabinLayout> Loaded = MakeUnique<simcore::CabinLayout>();
	std::string Error;
	if (!simcore::LoadCabinFile(ToStd(Path), Runner->Console(), *Loaded, &Error))
	{
		OutError = ToF(Error);
		UE_LOG(LogSAM3D, Error, TEXT("Cabin failed to load: %s"), *OutError);
		return false;
	}
	for (const std::string& Id : Loaded->UnplacedControls(Runner->Console().panel))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Cabin %s doesn't place control %s"), *CabinFile, *ToF(Id));
	}
	Cabin = MoveTemp(Loaded);
	UE_LOG(LogSAM3D, Log, TEXT("Cabin loaded: %s"), *Path);
	return true;
}

static ESimPlayerActionResult ToActionResult(simcore::StepResult Result)
{
	switch (Result)
	{
	case simcore::StepResult::Accepted: return ESimPlayerActionResult::Accepted;
	case simcore::StepResult::AcceptedEarly: return ESimPlayerActionResult::AcceptedEarly;
	case simcore::StepResult::NotNextStep: return ESimPlayerActionResult::OutOfOrder;
	case simcore::StepResult::WaitRequired: return ESimPlayerActionResult::NotYet;
	case simcore::StepResult::Blocked: return ESimPlayerActionResult::Blocked;
	case simcore::StepResult::ProcedureComplete: return ESimPlayerActionResult::Moved;
	case simcore::StepResult::UnknownControl:
	case simcore::StepResult::InvalidPosition: break;
	}
	return ESimPlayerActionResult::Invalid;
}

ESimPlayerActionResult USimWorldSubsystem::PlayerSetControl(const FString& ControlId, int32 Position)
{
	// Multiplayer crew: a seated player's hands go through the seat (to the server, or straight in on the server).
	if (USimCrewSeatComponent* Seat = LocalCrewSeat.Get())
	{
		if (!Seat->SeatRole.IsEmpty())
		{
			if (IsMirrorOnly())
			{
				Seat->SetControl(ControlId, Position);
				return ESimPlayerActionResult::Sent;
			}
			if (!Runner)
			{
				return ESimPlayerActionResult::Invalid;
			}
			std::string Why;
			const simcore::StepResult R = Runner->SeatSet(ToStd(Seat->SeatRole), ToStd(ControlId), Position, &Why);
			BroadcastNewDebriefEntries();
			return ToActionResult(R);
		}
	}
	if (!Runner || IsMirrorOnly())
	{
		return ESimPlayerActionResult::Invalid;
	}
	// A single player who took the control's seat (T in the cabin, docs/crew/08): his move goes through the seat, so the
	// orders he was given there (and the NPC commander watching them) see it.
	if (const simcore::ControlDef* Def = Runner->Console().panel.Find(ToStd(ControlId)))
	{
		if (!Def->station.empty() && !Runner->SeatHolder(Def->station).empty())
		{
			std::string Why;
			const simcore::StepResult R = Runner->SeatSet(Def->station, ToStd(ControlId), Position, &Why);
			BroadcastNewDebriefEntries();
			return ToActionResult(R);
		}
	}
	const simcore::StepResult Result = Runner->PlayerSet(ToStd(ControlId), Position);
	BroadcastNewDebriefEntries();
	switch (Result)
	{
	case simcore::StepResult::Accepted: return ESimPlayerActionResult::Accepted;
	case simcore::StepResult::AcceptedEarly: return ESimPlayerActionResult::AcceptedEarly;
	case simcore::StepResult::NotNextStep: return ESimPlayerActionResult::OutOfOrder;
	case simcore::StepResult::WaitRequired: return ESimPlayerActionResult::NotYet;
	case simcore::StepResult::Blocked: return ESimPlayerActionResult::Blocked;
	case simcore::StepResult::ProcedureComplete: return ESimPlayerActionResult::Moved;
	case simcore::StepResult::UnknownControl:
	case simcore::StepResult::InvalidPosition: break;
	}
	return ESimPlayerActionResult::Invalid;
}

bool USimWorldSubsystem::PlayerTurnWheel(const FString& ControlId, double Value)
{
	if (USimCrewSeatComponent* Seat = LocalCrewSeat.Get())
	{
		if (!Seat->SeatRole.IsEmpty())
		{
			if (IsMirrorOnly())
			{
				Seat->TurnWheel(ControlId, Value);
				return true;
			}
			std::string Why;
			return Runner && Runner->SeatTurn(ToStd(Seat->SeatRole), ToStd(ControlId), Value, &Why);
		}
	}
	if (Runner && !IsMirrorOnly())
	{
		if (const simcore::ControlDef* Def = Runner->Console().panel.Find(ToStd(ControlId)))
		{
			if (!Def->station.empty() && !Runner->SeatHolder(Def->station).empty())
			{
				std::string Why;
				return Runner->SeatTurn(Def->station, ToStd(ControlId), Value, &Why);
			}
		}
	}
	return Runner && !IsMirrorOnly() && Runner->PlayerTurn(ToStd(ControlId), Value);
}

bool USimWorldSubsystem::SetPlayerProcedure(const FString& ProcedureId)
{
	if (!Runner || !Runner->SetPlayerProcedure(ToStd(ProcedureId)))
	{
		return false;
	}
	BroadcastNewDebriefEntries();
	return true;
}

bool USimWorldSubsystem::IsLampLit(const FString& LampId) const
{
	if (!Runner || !Cabin)
	{
		return false;
	}
	const std::string Id = ToStd(LampId);
	for (const simcore::CabinLamp& Lamp : Cabin->lamps)
	{
		if (Lamp.id == Id)
		{
			return simcore::CabinLayout::LampLit(Lamp, Runner->Console().panel, Runner->Console().equipment);
		}
	}
	return false;
}

bool USimWorldSubsystem::IsScenarioFinished() const
{
	return Runner && Runner->Finished();
}

FVector USimWorldSubsystem::GetOriginLongitudeLatitudeHeight() const
{
	const simcore::LocalFrame* Frame = ActiveFrame();
	if (Frame == nullptr)
	{
		return FVector::ZeroVector;
	}
	const simcore::GeoPoint& O = Frame->Origin();
	return FVector(O.LonDeg(), O.LatDeg(), O.alt_m);
}

FString USimWorldSubsystem::GetDebriefText() const
{
	return Runner ? ToF(Runner->Debrief()) : FString();
}

void USimWorldSubsystem::BroadcastNewDebriefEntries()
{
	if (!Runner)
	{
		return;
	}
	const std::vector<simcore::DebriefEntry>& Entries = Runner->Log().Entries();
	for (; DebriefEntriesSent < Entries.size(); ++DebriefEntriesSent)
	{
		const simcore::DebriefEntry& E = Entries[DebriefEntriesSent];
		FSimDebriefEntry Out;
		Out.TimeSeconds = E.time_s;
		Out.Kind = ToF(simcore::ToString(E.kind));
		Out.Who = ToF(E.who);
		Out.What = ToF(E.what);
		Out.Detail = ToF(E.detail);
		UE_LOG(LogSAM3D, Log, TEXT("%8.1f s  %-16s %-10s %s  %s"), Out.TimeSeconds, *Out.Who, *Out.Kind, *Out.What, *Out.Detail);
		OnDebriefEntry.Broadcast(Out);
	}
}

void USimWorldSubsystem::SetUnitLocation(const FVector& UnrealLocation)
{
	if (Runner)
	{
		Runner->SetUnitPosition(ToEnu(UnrealLocation));
	}
}

FVector USimWorldSubsystem::GetUnitLocation() const
{
	return Runner ? ToUnreal(Runner->UnitPosition()) : FVector::ZeroVector;
}

void USimWorldSubsystem::SetUnitHeading(double HeadingDeg)
{
	if (Runner)
	{
		Runner->SetUnitHeading(HeadingDeg);
	}
}

double USimWorldSubsystem::YawToHeadingDeg(const FVector& Location, double YawDeg) const
{
	// The direction 100 m ahead, in ENU: its bearing is the heading.
	const FVector Ahead = Location + FRotator(0.0, YawDeg, 0.0).Vector() * 10000.0;
	const simcore::Vec3 A = ToEnu(Location);
	const simcore::Vec3 B = ToEnu(Ahead);
	return FMath::RadiansToDegrees(FMath::Atan2(B.x - A.x, B.y - A.y));
}

double USimWorldSubsystem::HeadingToYawDeg(const FVector& Location, double HeadingDeg) const
{
	const double H = FMath::DegreesToRadians(HeadingDeg);
	const simcore::Vec3 A = ToEnu(Location);
	const FVector From = ToUnreal(A);
	const FVector To = ToUnreal(A + simcore::Vec3{100.0 * FMath::Sin(H), 100.0 * FMath::Cos(H), 0.0});
	return (To - From).Rotation().Yaw;
}

double USimWorldSubsystem::GetUnitHeading() const
{
	return Runner ? Runner->Engagement().UnitHeadingDeg() : 0.0;
}

double USimWorldSubsystem::IndicatedToTrueAzimuth(double IndicatedDeg) const
{
	return Runner ? Runner->Engagement().IndicatedToTrueDeg(IndicatedDeg) : IndicatedDeg;
}

// ---------------------------------------------------------------------------------------------- console

int32 USimWorldSubsystem::GetControlPosition(const FString& ControlId) const
{
	return Runner ? Runner->Console().panel.Get(ToStd(ControlId)) : -1;
}

bool USimWorldSubsystem::SetControlPosition(const FString& ControlId, int32 Position)
{
	return Runner && Runner->Console().panel.Set(ToStd(ControlId), Position);
}

double USimWorldSubsystem::GetWheelValue(const FString& ControlId) const
{
	return Runner ? Runner->Console().panel.GetValue(ToStd(ControlId)) : 0.0;
}

bool USimWorldSubsystem::SetWheelValue(const FString& ControlId, double Value)
{
	return Runner && Runner->Console().panel.SetValue(ToStd(ControlId), Value);
}

ESimSubsystemState USimWorldSubsystem::GetSubsystemState(const FString& SubsystemId) const
{
	return Runner ? ToUnrealState(Runner->Console().equipment.State(ToStd(SubsystemId))) : ESimSubsystemState::Off;
}

double USimWorldSubsystem::GetSubsystemProgress(const FString& SubsystemId) const
{
	return Runner ? Runner->Console().equipment.Progress(ToStd(SubsystemId)) : 0.0;
}

bool USimWorldSubsystem::GetSimInput(const FString& InputId) const
{
	return Runner && Runner->Console().equipment.GetInput(ToStd(InputId));
}

bool USimWorldSubsystem::SetSimInput(const FString& InputId, bool bValue)
{
	return Runner && Runner->Console().equipment.SetInput(ToStd(InputId), bValue);
}

bool USimWorldSubsystem::OrderCrew(const FString& CrewName, const FString& ProcedureId, bool bQueue)
{
	if (!Runner)
	{
		return false;
	}
	simcore::CrewMember* Crew = Runner->Crew(ToStd(CrewName));
	const simcore::Procedure* Proc = Runner->Console().FindProcedure(ToStd(ProcedureId));
	if (Crew == nullptr || Proc == nullptr)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("OrderCrew: unknown crew '%s' or procedure '%s'"), *CrewName, *ProcedureId);
		return false;
	}
	// Through the runner, so the R-124 intercom decides whether the order arrives at once or is shouted.
	Runner->GiveOrder(*Crew, *Proc, bQueue);
	return true;
}

bool USimWorldSubsystem::GiveCrewOrder(const FString& OrderId)
{
	return Runner && Runner->GiveCrewOrder(ToStd(OrderId));
}

FSimDrillAdvice USimWorldSubsystem::GetAssistance(const FString& Station) const
{
	FSimDrillAdvice Out;
	if (!Runner)
	{
		return Out;
	}
	const simcore::AssistView V = Runner->Assistance(ToStd(Station));
	Out.Station = Station;
	Out.Task = ToF(V.task);
	if (!V.show)
	{
		return Out;
	}
	Out.bValid = true;
	Out.Control = ToF(V.control);
	Out.Position = V.position;
	Out.Description = ToF(V.description);
	Out.DueInSeconds = V.due_in_s;
	Out.bDemonstrating = V.demonstrating;
	for (const std::string& W : V.why)
	{
		Out.Why.Add(ToF(W));
	}
	return Out;
}

bool USimWorldSubsystem::SetAssistLevel(const FString& Level)
{
	simcore::AssistLevel L = simcore::AssistLevel::Guided;
	if (!Runner || !simcore::ParseAssistLevel(ToStd(Level), L))
	{
		return false;
	}
	Runner->SetAssistLevel(L);
	return true;
}

FString USimWorldSubsystem::GetAssistLevel() const
{
	return Runner ? ToF(simcore::ToString(Runner->GetAssistLevel())) : FString();
}

FSimMoveVerdict USimWorldSubsystem::GetLastMoveVerdict(const FString& Station) const
{
	FSimMoveVerdict Out;
	const simcore::MoveVerdict* V = Runner ? Runner->LastVerdict(ToStd(Station)) : nullptr;
	if (V == nullptr)
	{
		return Out;
	}
	Out.bValid = true;
	Out.TimeSeconds = V->time_s;
	Out.Control = ToF(V->control);
	Out.Position = V->position;
	Out.bBest = V->best;
	Out.BestControl = ToF(V->best_control);
	Out.BestDescription = ToF(V->best_description);
	Out.CostSeconds = V->cost_s;
	return Out;
}

bool USimWorldSubsystem::TryCrewOrder(const FString& OrderId, FString& OutReason)
{
	if (!Runner)
	{
		OutReason = TEXT("no scenario");
		return false;
	}
	simcore::ScenarioRunner::OrderCheck Why;
	const bool bOk = Runner->TryCrewOrder(ToStd(OrderId), &Why);
	OutReason = bOk ? FString() : FString::Printf(TEXT("%s / %s"), *ToF(Why.reason_ru), *ToF(Why.reason_en));
	return bOk;
}

FSimDrillAdvice USimWorldSubsystem::GetDrillAdvice(const FString& Station, const FString& TaskId) const
{
	FSimDrillAdvice Out;
	if (!Runner || Runner->Drill() == nullptr || !Runner->Def().has_crew_data)
	{
		return Out;
	}
	const simcore::CrewTask* Task = Runner->Def().crew_data.FindTask(ToStd(TaskId));
	if (Task == nullptr)
	{
		return Out;
	}
	const simcore::DrillMaster& Drill = *Runner->Drill();
	const simcore::DrillAdvice A = Drill.NextFor(ToStd(Station), Drill.GoalFor(*Task), Runner->Console());
	Out.Station = Station;
	if (A.control.empty())
	{
		return Out;
	}
	Out.bValid = true;
	Out.Control = ToF(A.control);
	Out.Position = A.position;
	Out.Procedure = ToF(A.procedure);
	Out.Description = ToF(A.description);
	Out.DueInSeconds = A.due_in_s;
	for (const std::string& W : A.why)
	{
		Out.Why.Add(ToF(W));
	}
	return Out;
}

bool USimWorldSubsystem::IsCrewTaskDone(const FString& TaskId) const
{
	if (!Runner || !Runner->Def().has_crew_data)
	{
		return false;
	}
	const simcore::CrewTask* Task = Runner->Def().crew_data.FindTask(ToStd(TaskId));
	const simcore::ConsoleData& C = Runner->Console();
	return Task != nullptr && C.equipment.AllHold(Task->conditions, C.panel);
}

TArray<FSimCrewMenuEntry> USimWorldSubsystem::GetCrewMenu(const FString& Station) const
{
	TArray<FSimCrewMenuEntry> Out;
	if (!Runner || !Runner->Def().has_crew_data)
	{
		return Out;
	}
	const simcore::CrewData& Crew = Runner->Def().crew_data;
	const std::string Seat = ToStd(Station);
	if (Seat == Crew.commander)
	{
		for (const simcore::OrderDef& O : Crew.orders)
		{
			if (const simcore::CalloutDef* C = Crew.FindCallout(O.callout))
			{
				FSimCrewMenuEntry E{ToF(C->id), ToF(C->ru), ToF(C->en), true};
				const simcore::ScenarioRunner::OrderCheck Check = Runner->CheckOrder(O.callout);
				E.bAvailable = Check.can;
				E.Reason = Check.can ? FString() : FString::Printf(TEXT("%s / %s"), *ToF(Check.reason_ru), *ToF(Check.reason_en));
				Out.Add(E);
			}
		}
		return Out;
	}
	for (const simcore::CalloutDef& C : Crew.callouts)
	{
		if (C.kind == simcore::CalloutKind::Report && (C.from == Seat || C.from == "any"))
		{
			Out.Add({ToF(C.id), ToF(C.ru), ToF(C.en), false});
		}
	}
	return Out;
}

bool USimWorldSubsystem::AssignBatteryTarget(const FString& Callsign, const FString& Target)
{
	return Runner && Runner->AssignBatteryTarget(ToStd(Callsign), ToStd(Target));
}

TArray<FSimBatteryShooter> USimWorldSubsystem::GetBatteryShooters() const
{
	TArray<FSimBatteryShooter> Out;
	const simcore::BatteryFire* Fire = Runner ? Runner->GetBatteryFire() : nullptr;
	if (Fire == nullptr)
	{
		return Out;
	}
	for (const simcore::BatteryFire::Shooter& S : Fire->Shooters())
	{
		FSimBatteryShooter O;
		O.Callsign = ToF(S.callsign);
		O.Missiles = S.missiles;
		O.Target = ToF(S.target);
		O.bDeployed = S.deployed;
		switch (S.phase)
		{
		case simcore::BatteryFire::Phase::Idle: O.Phase = TEXT("idle"); break;
		case simcore::BatteryFire::Phase::Acquiring: O.Phase = TEXT("acquiring"); break;
		case simcore::BatteryFire::Phase::Tracking: O.Phase = TEXT("tracking"); break;
		case simcore::BatteryFire::Phase::Preparing: O.Phase = TEXT("preparing"); break;
		case simcore::BatteryFire::Phase::InFlight: O.Phase = TEXT("missile in flight"); break;
		}
		Out.Add(O);
	}
	return Out;
}

bool USimWorldSubsystem::PlayerSay(const FString& Station, const FString& CalloutId)
{
	return Runner && Runner->PlayerSay(ToStd(Station), ToStd(CalloutId));
}

FString USimWorldSubsystem::GetCrewSeatHolder(const FString& Station) const
{
	return Runner ? ToF(Runner->SeatHolder(ToStd(Station))) : FString();
}

FString USimWorldSubsystem::GetTrackedTarget() const
{
	return (Runner && Runner->EngagementOn()) ? ToF(Runner->Engagement().TrackedTarget()) : FString();
}

// ---------------------------------------------------------------------------------------------- terrain

bool USimWorldSubsystem::RequestCesiumTerrain(ACesium3DTileset* Tileset, double HalfSizeMeters, double SpacingMeters)
{
	const simcore::LocalFrame* Frame = ActiveFrame();
	if (Tileset == nullptr || Frame == nullptr || !Runner || HalfSizeMeters <= 0.0 || SpacingMeters <= 0.0)
	{
		return false;
	}
	const int32 Count = FMath::FloorToInt32(2.0 * HalfSizeMeters / SpacingMeters) + 1;
	TArray<FVector> Positions;
	Positions.Reserve(Count * Count);
	for (int32 Row = 0; Row < Count; ++Row)  // row 0 = southmost, as simcore::GridTerrain expects
	{
		for (int32 Col = 0; Col < Count; ++Col)
		{
			const simcore::Vec3 Enu(-HalfSizeMeters + Col * SpacingMeters, -HalfSizeMeters + Row * SpacingMeters, 0.0);
			const simcore::GeoPoint G = Frame->EnuToGeodetic(Enu);
			Positions.Add(FVector(G.LonDeg(), G.LatDeg(), 0.0));
		}
	}
	const int32 Serial = ++TerrainRequestSerial;
	const double Fallback = Frame->Origin().alt_m;
	UE_LOG(LogSAM3D, Log, TEXT("Sampling Cesium terrain: %d x %d points, %.0f m spacing"), Count, Count, SpacingMeters);
	Tileset->SampleHeightMostDetailed(Positions, FCesiumSampleHeightMostDetailedCallback::CreateWeakLambda(this,
		[this, Serial, Count, HalfSizeMeters, SpacingMeters, Fallback](
			ACesium3DTileset*, const TArray<FCesiumSampleHeightResult>& Results, const TArray<FString>& Warnings)
		{
			if (Serial != TerrainRequestSerial || !Runner)
			{
				return;  // superseded, or the scenario ended
			}
			std::vector<double> Heights;
			Heights.reserve(Results.Num());
			int32 Failed = 0;
			for (const FCesiumSampleHeightResult& R : Results)
			{
				Heights.push_back(R.SampleSuccess ? R.LongitudeLatitudeHeight.Z : Fallback);
				Failed += R.SampleSuccess ? 0 : 1;
			}
			for (const FString& W : Warnings)
			{
				UE_LOG(LogSAM3D, Warning, TEXT("Cesium height sampling: %s"), *W);
			}
			const bool bOk = Results.Num() == Count * Count && Failed < Results.Num();
			if (bOk)
			{
				TUniquePtr<simcore::GridTerrain> Grid = MakeUnique<simcore::GridTerrain>(
					-HalfSizeMeters, -HalfSizeMeters, SpacingMeters, Count, Count, std::move(Heights));
				Runner->SetTerrain(Grid.Get());  // point the sim at the new grid before the old one goes away
				if (EmLayer && EmLayer->IsRunning())
				{
					UE_LOG(LogSAM3D, Log, TEXT("Terrain replaced: EM layer stopped (sam3d.em.Start to restart on it)"));
					EmLayer->Stop();
				}
				SampledTerrain = MoveTemp(Grid);
			}
			UE_LOG(LogSAM3D, Log, TEXT("Cesium terrain %s: %d samples, %d failed"), bOk ? TEXT("ready") : TEXT("failed"),
				Results.Num(), Failed);
			OnTerrainReady.Broadcast(bOk);
		}));
	return true;
}

// ---------------------------------------------------------------------------------------------- electromagnetic layer

bool USimWorldSubsystem::StartEmDemo(FString& OutError)
{
	const simcore::World* W = ActiveWorld();
	if (W == nullptr)
	{
		OutError = TEXT("no sim world: load a scenario or reset the simulation first");
		return false;
	}
	if (!EmLayer)
	{
		EmLayer = MakeUnique<FSimEmLayer>();
	}
	return EmLayer->StartDemo(ResolveDataDirectory(), W->Frame().Origin(), &W->GetTerrain(), OutError);
}

void USimWorldSubsystem::StopEmDemo()
{
	if (EmLayer)
	{
		EmLayer->Stop();
	}
}

bool USimWorldSubsystem::IsEmDemoRunning() const
{
	return EmLayer && EmLayer->IsRunning();
}

double USimWorldSubsystem::GetEmTimeSeconds() const
{
	return EmLayer ? EmLayer->GetTimeSeconds() : 0.0;
}

FString USimWorldSubsystem::GetEmStatusText() const
{
	return EmLayer ? EmLayer->GetStatusText() : FString(TEXT("EM layer not running"));
}

// ---------------------------------------------------------------------------------------------- entities

FSimEntityState USimWorldSubsystem::MakeState(int32 Id, const FString& Name, bool bMissile, bool bAlive,
	const simcore::Vec3& Pos, const simcore::Vec3& Vel) const
{
	FSimEntityState S;
	S.Id = Id;
	S.Name = Name;
	S.bIsMissile = bMissile;
	S.bAlive = bAlive;
	S.Location = ToUnreal(Pos);
	S.Velocity = ToUnreal(Pos + Vel) - S.Location;  // per second, in the display frame
	S.Rotation = S.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : S.Velocity.Rotation();
	return S;
}

TArray<FSimEntityState> USimWorldSubsystem::GetEntityStates() const
{
	TArray<FSimEntityState> Out;
	const simcore::World* W = ActiveWorld();
	if (W == nullptr)
	{
		return Out;
	}
	for (const simcore::Aircraft& A : W->AircraftList())
	{
		Out.Add(MakeState(A.id, ToF(A.name), false, A.alive, A.position, A.velocity));
	}
	for (const simcore::Missile& M : W->MissileList())
	{
		Out.Add(MakeState(M.id, TEXT("Missile"), true, M.Active(), M.position, M.velocity));
	}
	// The battery's other vehicles and the dummy positions (roadmap 7.7).
	{
		simcore::em::ViewExtras Scratch;
		if (const simcore::em::ViewExtras* X = GetViewExtras(Scratch))
		{
			for (int32 I = 0; I < static_cast<int32>(X->battery.size()); ++I)
			{
				const simcore::em::ViewVehicle& V = X->battery[static_cast<size_t>(I)];
				FSimEntityState S = MakeState(GroundIdBase + I, UTF8_TO_TCHAR(V.id.c_str()), false, true, V.position, simcore::Vec3{});
				S.Rotation = FRotator(0.0, V.heading_deg, 0.0);
				S.bIsGround = true;
				S.Kind = UTF8_TO_TCHAR(V.kind.c_str());
				S.bDeployed = V.deployed;
				Out.Add(S);
			}
		}
	}
	if (Runner && !IsMirrorOnly())
	{
		for (const simcore::EngagementModel::MissileView& M : Runner->MissilesInFlight())
		{
			Out.Add(MakeState(MissileIdBase + M.number, FString::Printf(TEXT("Missile %d"), M.number), true, true,
				M.position, M.velocity));
		}
	}
	return Out;
}

bool USimWorldSubsystem::GetEntityState(int32 Id, FSimEntityState& OutState) const
{
	for (const FSimEntityState& S : GetEntityStates())
	{
		if (S.Id == Id)
		{
			OutState = S;
			return true;
		}
	}
	// A missile that has finished: report it as no longer alive so its actor hides.
	if (Id >= MissileIdBase && Runner)
	{
		OutState = FSimEntityState();
		OutState.Id = Id;
		OutState.bIsMissile = true;
		OutState.bAlive = false;
		return true;
	}
	return false;
}

double USimWorldSubsystem::GetSimTimeSeconds() const
{
	if (Runner)
	{
		return Runner->Time();
	}
	return SimWorld ? SimWorld->Time() : 0.0;
}

// ------------------------------------------------------------------ multiplayer knowledge (roadmap E10)

void USimWorldSubsystem::RegisterKnowledgeClient(USimKnowledgeComponent* Client)
{
	if (Client == nullptr || KnowledgeClients.Contains(Client))
	{
		return;
	}
	if (!Replicator)
	{
		Replicator = MakeUnique<simcore::em::KnowledgeReplicator>();
	}
	const int32 Node = Client->NodeId != 0 ? Client->NodeId : GetDefaultKnowledgeNode();
	Client->SetReplicatorHandle(Replicator->AddClient(Node));
	Client->SendAccumulator() = 1e9;  // the first view goes out at once
	KnowledgeClients.Add(Client);
}

void USimWorldSubsystem::UnregisterKnowledgeClient(USimKnowledgeComponent* Client)
{
	KnowledgeClients.Remove(Client);
}

void USimWorldSubsystem::AckKnowledge(USimKnowledgeComponent* Client, uint32 Seq)
{
	if (Replicator && Client != nullptr && Client->GetReplicatorHandle() != INDEX_NONE)
	{
		Replicator->Ack(Client->GetReplicatorHandle(), Seq);
	}
}

const simcore::em::EmScene* USimWorldSubsystem::GetActiveEmScene() const
{
	if (Runner && Runner->Em() != nullptr)
	{
		return &Runner->Em()->World().Scene();
	}
	if (EmLayer && EmLayer->IsRunning())
	{
		return EmLayer->GetScene();
	}
	return nullptr;
}

int32 USimWorldSubsystem::GetDefaultKnowledgeNode() const
{
	return Runner && Runner->Em() != nullptr ? Runner->Em()->UnitNodeId() : 0;
}

void USimWorldSubsystem::SetLocalKnowledge(USimKnowledgeComponent* Knowledge)
{
	LocalKnowledge = Knowledge;
}

void USimWorldSubsystem::ClearLocalKnowledge(USimKnowledgeComponent* Knowledge)
{
	if (LocalKnowledge.Get() == Knowledge)
	{
		LocalKnowledge.Reset();
	}
}

const simcore::em::ViewScope* USimWorldSubsystem::GetMirroredScope() const
{
	const USimKnowledgeComponent* K = LocalKnowledge.Get();
	return K != nullptr && K->GetView().scope.valid ? &K->GetView().scope : nullptr;
}

void USimWorldSubsystem::PumpKnowledge()
{
	if (!Replicator || KnowledgeClients.Num() == 0 || GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}
	const simcore::em::EmScene* Scene = GetActiveEmScene();
	if (Scene == nullptr && !Runner)
	{
		return;
	}
	// Sim time, not frame time: a paused or time-compressed sim sends by its own clock.
	const double Now = Runner ? Runner->Time() : Scene->time_s;
	double Dt = LastKnowledgeSimTime >= 0.0 ? Now - LastKnowledgeSimTime : 0.0;
	if (Dt < 0.0)
	{
		Dt = 0.0;  // a new scene started
	}
	LastKnowledgeSimTime = Now;
	KnowledgeClients.RemoveAll([](const TWeakObjectPtr<USimKnowledgeComponent>& C) { return !C.IsValid(); });
	for (const TWeakObjectPtr<USimKnowledgeComponent>& Weak : KnowledgeClients)
	{
		USimKnowledgeComponent* Client = Weak.Get();
		double& Acc = Client->SendAccumulator();
		Acc += Dt;
		if (Acc < Client->SendPeriodSeconds)
		{
			continue;
		}
		Acc = 0.0;
		const int32 Handle = Client->GetReplicatorHandle();
		// In a battery link, the player's own vehicle (his seat says which): its sensors' scene, its unit, its extras.
		const USimCrewSeatComponent* Seat = Client->GetOwner() != nullptr ? Client->GetOwner()->FindComponentByClass<USimCrewSeatComponent>() : nullptr;
		simcore::ScenarioRunner* R = Seat != nullptr ? RunnerFor(Seat->SeatVehicle) : Runner;
		const simcore::em::EmScene* ClientScene = R != Runner ? (R->Em() != nullptr ? &R->Em()->World().Scene() : nullptr) : Scene;
		const int32 Node = Client->NodeId != 0 ? Client->NodeId
			: R != Runner ? (R->Em() != nullptr ? R->Em()->UnitNodeId() : 0) : GetDefaultKnowledgeNode();
		if (Replicator->Node(Handle) != Node)
		{
			Replicator->SetNode(Handle, Node);
		}
		// The scopes as the crew sees them ride along (echoes keyed opaquely per player).
		simcore::EchoKeys& Keys = EchoKeysByClient[Handle];
		const std::vector<std::uint8_t> Bytes = Replicator->Produce(Handle, ClientScene, [R, &Keys, Now](simcore::em::KnowledgeView& V) {
			V.time_s = Now;
			if (R != nullptr && R->EngagementOn())
			{
				V.scope = simcore::ToViewScope(simcore::ScopeFeedOf(R->Engagement(), R->GetLauncher(), R->Time()), Keys);
			}
			if (R != nullptr)
			{
				V.extras = R->ViewExtrasNow(16);
			}
		});
		TArray<uint8> Packet;
		Packet.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
		Client->SendKnowledge(Packet);
	}
}

// ------------------------------------------------------------------ multiplayer crew

bool USimWorldSubsystem::IsMirrorOnly() const
{
	return GetWorld() != nullptr && GetWorld()->GetNetMode() == NM_Client;
}

void USimWorldSubsystem::RegisterCrewSeat(USimCrewSeatComponent* Seat)
{
	if (Seat != nullptr && !CrewSeats.Contains(Seat))
	{
		Seat->SendAccumulator() = 1e9;  // the first snapshot goes out at once
		CrewSeats.Add(Seat);
	}
}

void USimWorldSubsystem::UnregisterCrewSeat(USimCrewSeatComponent* Seat)
{
	CrewSeats.Remove(Seat);
}

void USimWorldSubsystem::SetLocalCrewSeat(USimCrewSeatComponent* Seat)
{
	LocalCrewSeat = Seat;
}

void USimWorldSubsystem::ClearLocalCrewSeat(USimCrewSeatComponent* Seat)
{
	if (LocalCrewSeat.Get() == Seat)
	{
		LocalCrewSeat.Reset();
	}
}

simcore::ScenarioRunner* USimWorldSubsystem::RunnerFor(const FString& Vehicle) const
{
	if (!Link || Vehicle.IsEmpty())
	{
		return Runner;
	}
	return Vehicle == TEXT("post") ? &Link->Post() : &Link->Osa();
}

FString USimWorldSubsystem::GetLocalVehicle() const
{
	if (!Link)
	{
		return FString();
	}
	return Runner == &Link->Post() ? FString(TEXT("post")) : FString(TEXT("osa"));
}

bool USimWorldSubsystem::TakeCrewSeat(const FString& Role, const FString& Player, const FString& Vehicle)
{
	simcore::ScenarioRunner* In = RunnerFor(Vehicle);
	const bool bOk = In != nullptr && In->TakeSeat(ToStd(Role), ToStd(Player));
	BroadcastNewDebriefEntries();
	return bOk;
}

void USimWorldSubsystem::LeaveCrewSeat(const FString& Role, const FString& Vehicle)
{
	if (simcore::ScenarioRunner* In = RunnerFor(Vehicle))
	{
		In->LeaveSeat(ToStd(Role));
		BroadcastNewDebriefEntries();
	}
}

bool USimWorldSubsystem::SeatSetControl(const FString& Role, const FString& ControlId, int32 Position, FString& Why, const FString& Vehicle)
{
	simcore::ScenarioRunner* In = RunnerFor(Vehicle);
	if (In == nullptr)
	{
		Why = TEXT("no scenario");
		return false;
	}
	std::string W;
	const simcore::StepResult R = In->SeatSet(ToStd(Role), ToStd(ControlId), Position, &W);
	BroadcastNewDebriefEntries();
	const ESimPlayerActionResult A = ToActionResult(R);
	const bool bMoved = A == ESimPlayerActionResult::Accepted || A == ESimPlayerActionResult::AcceptedEarly ||
		A == ESimPlayerActionResult::OutOfOrder || A == ESimPlayerActionResult::Moved;
	if (!bMoved)
	{
		Why = W.empty() ? FString(StaticEnum<ESimPlayerActionResult>()->GetNameStringByValue(static_cast<int64>(A))) : FString(UTF8_TO_TCHAR(W.c_str()));
	}
	return bMoved;
}

bool USimWorldSubsystem::SeatTurnWheel(const FString& Role, const FString& ControlId, double Value, FString& Why, const FString& Vehicle)
{
	std::string W;
	simcore::ScenarioRunner* In = RunnerFor(Vehicle);
	const bool bOk = In != nullptr && In->SeatTurn(ToStd(Role), ToStd(ControlId), Value, &W);
	Why = UTF8_TO_TCHAR(W.c_str());
	return bOk;
}

bool USimWorldSubsystem::ApplyConsoleSnapshot(const TArray<uint8>& Bytes, uint32& Seq)
{
	if (!Runner)
	{
		return false;  // this machine has not loaded the scenario yet
	}
	const std::vector<std::uint8_t> Packet(Bytes.GetData(), Bytes.GetData() + Bytes.Num());
	std::uint32_t S = Seq;
	if (!simcore::ApplyConsoleState(Packet, Runner->Console(), &S))
	{
		return false;
	}
	Seq = S;
	return true;
}

FString USimWorldSubsystem::GetSeatHolder(const FString& Role) const
{
	return Runner ? FString(UTF8_TO_TCHAR(Runner->SeatHolder(ToStd(Role)).c_str())) : FString();
}

void USimWorldSubsystem::PumpConsole()
{
	if (!Runner || CrewSeats.Num() == 0 || IsMirrorOnly())
	{
		return;
	}
	double Dt = LastConsoleSimTime >= 0.0 ? Runner->Time() - LastConsoleSimTime : 0.0;
	if (Dt < 0.0)
	{
		Dt = 0.0;  // a new scenario started
	}
	LastConsoleSimTime = Runner->Time();
	CrewSeats.RemoveAll([](const TWeakObjectPtr<USimCrewSeatComponent>& S) { return !S.IsValid(); });
	for (const TWeakObjectPtr<USimCrewSeatComponent>& Weak : CrewSeats)
	{
		USimCrewSeatComponent* Seat = Weak.Get();
		simcore::ScenarioRunner* In = RunnerFor(Seat->SeatVehicle);  // a battery link: the player's own vehicle
		// The seat's procedure for the player's UI (owner-only replicated properties).
		FString ProcId;
		int32 Step = 0;
		if (!Seat->SeatRole.IsEmpty())
		{
			if (const simcore::ProcedureTracker* T = In->SeatTracker(ToStd(Seat->SeatRole)))
			{
				ProcId = UTF8_TO_TCHAR(T->GetProcedure().id.c_str());
				Step = T->IsComplete() ? 0 : static_cast<int32>(T->NextIndex()) + 1;
			}
		}
		Seat->SeatProcedureId = ProcId;
		Seat->SeatProcedureStep = Step;
		double& Acc = Seat->SendAccumulator();
		Acc += Dt;
		if (Acc < Seat->ConsoleSendPeriodSeconds)
		{
			continue;
		}
		Acc = 0.0;
		const std::vector<std::uint8_t> Bytes = In->ConsoleSnapshot(Seat->NextSnapshotSeq());
		TArray<uint8> Packet;
		Packet.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
		Seat->SendConsole(Packet);
	}
}

// ------------------------------------------------------------------ battery, board and radio (roadmap 7.7)

const simcore::em::ViewExtras* USimWorldSubsystem::GetViewExtras(simcore::em::ViewExtras& Scratch) const
{
	if (IsMirrorOnly())
	{
		const USimKnowledgeComponent* K = LocalKnowledge.Get();
		return K != nullptr && K->GetView().extras.valid ? &K->GetView().extras : nullptr;
	}
	if (!Runner)
	{
		return nullptr;
	}
	Scratch = Runner->ViewExtrasNow(16);
	return &Scratch;
}

simcore::Vec3 USimWorldSubsystem::BatteryCentreEnu() const
{
	if (IsMirrorOnly())
	{
		if (const simcore::em::ViewScope* Sc = GetMirroredScope())
		{
			return Sc->unit_position;
		}
	}
	return Runner ? Runner->UnitPosition() : simcore::Vec3{};
}

TArray<FSimBoardMark> USimWorldSubsystem::GetBoardMarks() const
{
	TArray<FSimBoardMark> Out;
	simcore::em::ViewExtras Scratch;
	const simcore::em::ViewExtras* X = GetViewExtras(Scratch);
	if (X == nullptr)
	{
		return Out;
	}
	const simcore::Vec3 Centre = BatteryCentreEnu();
	for (const simcore::em::ViewBoardMark& M : X->board)
	{
		FSimBoardMark B;
		B.Number = M.number;
		B.Stamp = M.stamp;
		B.HeightHm = M.height_hm;
		B.Code = M.code;
		B.EastNorthMeters = FVector2D(M.position.x - Centre.x, M.position.y - Centre.y);
		Out.Add(B);
	}
	return Out;
}

TArray<float> USimWorldSubsystem::GetBoardRangeRings() const
{
	TArray<float> Out;
	const simcore::PlottingBoard* Board = Runner ? Runner->Board() : nullptr;
	if (Board != nullptr)
	{
		for (double R : Board->Def().range_rings_m)
		{
			Out.Add(static_cast<float>(R));
		}
	}
	if (Out.Num() == 0)
	{
		Out = {50000.f, 100000.f, 150000.f, 200000.f};
	}
	return Out;
}

float USimWorldSubsystem::GetPostMastExtension() const
{
	if (!Runner || Runner->Post() == nullptr || !Runner->Console().equipment.Has("mast_up"))
	{
		return -1.f;
	}
	return static_cast<float>(Runner->Console().equipment.Progress("mast_up"));
}

bool USimWorldSubsystem::IsPostGeneratorOut() const
{
	// (a network client reads the mirrored console: mast_up and ab1_placed's progress come in its snapshots)
	return Runner && Runner->Post() != nullptr && Runner->Console().equipment.Has("ab1_placed") &&
		Runner->Console().equipment.Progress("ab1_placed") >= 0.5;
}

bool USimWorldSubsystem::IsCommandPost() const
{
	return Runner && Runner->Post() != nullptr;
}

simcore::PostFeed USimWorldSubsystem::GetPostFeed() const
{
	if (IsMirrorOnly())
	{
		simcore::em::ViewExtras Scratch;
		const simcore::em::ViewExtras* X = GetViewExtras(Scratch);
		return X != nullptr ? simcore::FromViewExtras(*X) : simcore::PostFeed();
	}
	return Runner ? Runner->PostFeedNow() : simcore::PostFeed();
}

TArray<int32> USimWorldSubsystem::GetPostTrackNumbers() const
{
	TArray<int32> Out;
	std::vector<simcore::CommandPost::ScopeMark> Marks = GetPostFeed().scope;
	std::sort(Marks.begin(), Marks.end(), [](const simcore::CommandPost::ScopeMark& A, const simcore::CommandPost::ScopeMark& B) {
		return A.x * A.x + A.y * A.y < B.x * B.x + B.y * B.y;
	});
	for (const simcore::CommandPost::ScopeMark& M : Marks)
	{
		Out.Add(M.number);
	}
	return Out;
}

TArray<int32> USimWorldSubsystem::GetPostBoardNumbers() const
{
	TArray<int32> Out;
	const simcore::PostFeed Feed = GetPostFeed();
	const simcore::Vec3 At = Feed.position;
	std::vector<simcore::em::ViewBoardMark> Marks = simcore::LatestMarks(Feed);
	std::sort(Marks.begin(), Marks.end(), [&At](const simcore::em::ViewBoardMark& A, const simcore::em::ViewBoardMark& B) {
		return (A.position - At).HorizontalLength() < (B.position - At).HorizontalLength();
	});
	for (const simcore::em::ViewBoardMark& M : Marks)
	{
		Out.Add(M.number);
	}
	return Out;
}

TArray<FSimLauncherStatus> USimWorldSubsystem::GetLauncherStatus() const
{
	TArray<FSimLauncherStatus> Out;
	for (const simcore::LauncherStatus& L : GetPostFeed().launchers)
	{
		FSimLauncherStatus S;
		S.Callsign = ToF(L.callsign);
		S.State = ToF(simcore::LauncherStateName(L.state));
		S.Missiles = L.missiles;
		S.TargetNumber = L.target_number;
		Out.Add(S);
	}
	return Out;
}

bool USimWorldSubsystem::DesignateBatteryTarget(const FString& Callsign, int32 Number)
{
	// A seated player in multiplayer: through the seat (to the server; the answer comes back as an action result).
	if (USimCrewSeatComponent* Seat = LocalCrewSeat.Get())
	{
		if (!Seat->SeatRole.IsEmpty())
		{
			if (IsMirrorOnly())
			{
				Seat->Designate(Callsign, Number);
				return true;
			}
			FString Why;
			return SeatDesignate(Seat->SeatRole, Callsign, Number, Why, Seat->SeatVehicle);
		}
	}
	return Runner && !IsMirrorOnly() && Runner->DesignateBatteryTarget(ToStd(Callsign), Number);
}

bool USimWorldSubsystem::PlotPostTrack(int32 Number)
{
	if (USimCrewSeatComponent* Seat = LocalCrewSeat.Get())
	{
		if (!Seat->SeatRole.IsEmpty())
		{
			if (IsMirrorOnly())
			{
				Seat->PlotTrack(Number);
				return true;
			}
			FString Why;
			return SeatPlot(Seat->SeatRole, Number, Why, Seat->SeatVehicle);
		}
	}
	return Runner && !IsMirrorOnly() && Runner->PlayerPlot(Number);
}

bool USimWorldSubsystem::SeatDesignate(const FString& Role, const FString& Callsign, int32 Number, FString& Why, const FString& Vehicle)
{
	std::string W;
	simcore::ScenarioRunner* In = RunnerFor(Vehicle);
	const bool bOk = In != nullptr && In->SeatDesignate(ToStd(Role), ToStd(Callsign), Number, &W);
	Why = In != nullptr ? ToF(W) : FString(TEXT("no scenario"));
	BroadcastNewDebriefEntries();
	return bOk;
}

bool USimWorldSubsystem::SeatPlot(const FString& Role, int32 Number, FString& Why, const FString& Vehicle)
{
	std::string W;
	simcore::ScenarioRunner* In = RunnerFor(Vehicle);
	const bool bOk = In != nullptr && In->SeatPlot(ToStd(Role), Number, &W);
	Why = In != nullptr ? ToF(W) : FString(TEXT("no scenario"));
	return bOk;
}

FString USimWorldSubsystem::GetDefaultCabinFile() const
{
	if (!Runner || Runner->Console().system_id.empty())
	{
		return FString();
	}
	const FString File = FString::Printf(TEXT("cabins/%s.json"), *ToF(Runner->Console().system_id));
	return FPaths::FileExists(FPaths::Combine(ResolveDataDirectory(), File)) ? File : FString();
}

FString USimWorldSubsystem::GetCommanderStation() const
{
	return Runner && Runner->Def().has_crew_data ? ToF(Runner->Def().crew_data.commander) : FString();
}

FString USimWorldSubsystem::GetPlotterStation() const
{
	return Runner && Runner->Console().has_post ? ToF(Runner->Console().post_plotter) : FString();
}

TArray<FSimRadioMessage> USimWorldSubsystem::GetRadioMessages(int32 MaxCount) const
{
	TArray<FSimRadioMessage> Out;
	simcore::em::ViewExtras Scratch;
	const simcore::em::ViewExtras* X = GetViewExtras(Scratch);
	if (X == nullptr)
	{
		return Out;
	}
	const int32 N = static_cast<int32>(X->radio.size());
	for (int32 I = FMath::Max(0, N - MaxCount); I < N; ++I)
	{
		const simcore::em::ViewRadio& R = X->radio[static_cast<size_t>(I)];
		FSimRadioMessage M;
		M.TimeSeconds = R.time_s;
		M.Net = UTF8_TO_TCHAR(R.net.c_str());
		M.From = UTF8_TO_TCHAR(R.from.c_str());
		M.Text = UTF8_TO_TCHAR(R.text.c_str());
		Out.Add(M);
	}
	return Out;
}

