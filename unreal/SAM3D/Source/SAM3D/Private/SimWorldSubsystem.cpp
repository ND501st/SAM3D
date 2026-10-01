#include "SimWorldSubsystem.h"

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumSampleHeightResult.h"
#include "Misc/Paths.h"
#include "SAM3D.h"

namespace
{
	constexpr double MetersToCm = 100.0;
	constexpr int32 MissileIdBase = 100000;  // display ids for engagement-model missiles

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
	Runner.Reset();
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

void USimWorldSubsystem::Tick(float DeltaTime)
{
	if (bPaused)
	{
		return;
	}
	// Fixed-step simulation, decoupled from frame rate. Clamp to avoid a spiral of death after hitches.
	Accumulator += FMath::Min(static_cast<double>(DeltaTime), 0.25);

	if (Runner)
	{
		int32 Steps = 0;
		while (Accumulator >= ScenarioStepSeconds && Steps < 20 && !Runner->Finished())
		{
			Runner->Step(ScenarioStepSeconds);
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
	while (Accumulator >= FixedStepSeconds && Steps < 100)
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
	Runner.Reset();
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

bool USimWorldSubsystem::LoadScenario(const FString& ScenarioFile, FString& OutError)
{
	const FString DataDir = ResolveDataDirectory();
	const FString Path = FPaths::Combine(DataDir, ScenarioFile);
	simcore::ScenarioDef Def;
	simcore::ConsoleData Console;
	std::string Error;
	if (!simcore::LoadScenarioFile(ToStd(Path), ToStd(DataDir), Def, Console, &Error))
	{
		OutError = ToF(Error);
		UE_LOG(LogSAM3D, Error, TEXT("Scenario failed to load: %s"), *OutError);
		return false;
	}
	SimWorld.Reset();
	bDemoActive = false;
	SampledTerrain.Reset();
	Cabin.Reset();
	Runner = MakeUnique<simcore::ScenarioRunner>(std::move(Def), std::move(Console));
	Accumulator = 0.0;
	DebriefEntriesSent = 0;
	UE_LOG(LogSAM3D, Log, TEXT("Scenario loaded: %s (%s)"), *ToF(Runner->Def().name), *Path);
	BroadcastNewDebriefEntries();
	return true;
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

ESimPlayerActionResult USimWorldSubsystem::PlayerSetControl(const FString& ControlId, int32 Position)
{
	if (!Runner)
	{
		return ESimPlayerActionResult::Invalid;
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
	return Runner && Runner->PlayerTurn(ToStd(ControlId), Value);
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
	if (bQueue)
	{
		Crew->Queue(*Proc);
	}
	else
	{
		Crew->Order(*Proc);
	}
	return true;
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
				SampledTerrain = MoveTemp(Grid);
			}
			UE_LOG(LogSAM3D, Log, TEXT("Cesium terrain %s: %d samples, %d failed"), bOk ? TEXT("ready") : TEXT("failed"),
				Results.Num(), Failed);
			OnTerrainReady.Broadcast(bOk);
		}));
	return true;
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
	if (Runner)
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
