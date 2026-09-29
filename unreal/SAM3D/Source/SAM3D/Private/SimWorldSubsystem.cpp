#include "SimWorldSubsystem.h"

#include "SAM3D.h"

namespace
{
	constexpr double MetersToCm = 100.0;

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
}

FVector USimWorldSubsystem::EnuToUnreal(const simcore::Vec3& Enu)
{
	// ENU (x=E, y=N, z=U) metres -> Unreal (X=N, Y=E, Z=U) centimetres.
	return FVector(Enu.y * MetersToCm, Enu.x * MetersToCm, Enu.z * MetersToCm);
}

simcore::Vec3 USimWorldSubsystem::UnrealToEnu(const FVector& Unreal)
{
	return simcore::Vec3(Unreal.Y / MetersToCm, Unreal.X / MetersToCm, Unreal.Z / MetersToCm);
}

void USimWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetSimulation(45.0, 25.0, 0.0);
}

void USimWorldSubsystem::Deinitialize()
{
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

void USimWorldSubsystem::ResetSimulation(double OriginLatitudeDeg, double OriginLongitudeDeg, double OriginAltitudeM)
{
	// Flat ground at the origin's altitude until a Cesium-backed simcore::Terrain is implemented.
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

void USimWorldSubsystem::Tick(float DeltaTime)
{
	if (!SimWorld || bPaused)
	{
		return;
	}

	// Fixed-step simulation, decoupled from frame rate. Clamp to avoid a spiral of death after hitches.
	Accumulator += FMath::Min(static_cast<double>(DeltaTime), 0.25);
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

FSimEntityState USimWorldSubsystem::MakeAircraftState(const simcore::Aircraft& InAircraft) const
{
	FSimEntityState S;
	S.Id = InAircraft.id;
	S.Name = UTF8_TO_TCHAR(InAircraft.name.c_str());
	S.bIsMissile = false;
	S.bAlive = InAircraft.alive;
	S.Location = EnuToUnreal(InAircraft.position);
	S.Velocity = EnuToUnreal(InAircraft.velocity);
	S.Rotation = S.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : S.Velocity.Rotation();
	return S;
}

FSimEntityState USimWorldSubsystem::MakeMissileState(const simcore::Missile& InMissile) const
{
	FSimEntityState S;
	S.Id = InMissile.id;
	S.Name = TEXT("Missile");
	S.bIsMissile = true;
	S.bAlive = InMissile.Active();
	S.Location = EnuToUnreal(InMissile.position);
	S.Velocity = EnuToUnreal(InMissile.velocity);
	S.Rotation = S.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : S.Velocity.Rotation();
	return S;
}

TArray<FSimEntityState> USimWorldSubsystem::GetEntityStates() const
{
	TArray<FSimEntityState> Out;
	if (!SimWorld)
	{
		return Out;
	}
	for (const simcore::Aircraft& A : SimWorld->AircraftList())
	{
		Out.Add(MakeAircraftState(A));
	}
	for (const simcore::Missile& M : SimWorld->MissileList())
	{
		Out.Add(MakeMissileState(M));
	}
	return Out;
}

bool USimWorldSubsystem::GetEntityState(int32 Id, FSimEntityState& OutState) const
{
	if (!SimWorld)
	{
		return false;
	}
	for (const simcore::Aircraft& A : SimWorld->AircraftList())
	{
		if (A.id == Id)
		{
			OutState = MakeAircraftState(A);
			return true;
		}
	}
	if (const simcore::Missile* M = SimWorld->FindMissile(Id))
	{
		OutState = MakeMissileState(*M);
		return true;
	}
	return false;
}

double USimWorldSubsystem::GetSimTimeSeconds() const
{
	return SimWorld ? SimWorld->Time() : 0.0;
}
