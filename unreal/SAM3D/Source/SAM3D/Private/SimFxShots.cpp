#include "SimFxShots.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimFxAudio.h"
#include "SimFxImpl.h"
#include "SimFxSubsystem.h"
#include "SimSkyWeatherActor.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"

#include <utility>
#include <vector>

// The photographer of docs/fx/README.md 4.1: synthetic events in the feed (presentation only), each subject framed by
// one free camera, the simulation paused for each picture so what is aimed at is still there when it is taken.

namespace
{
	/** What a subject of the shot list is. */
	enum class ESimFxShotKind : uint8
	{
		AirburstNear,  // our missile's warhead (MissileEnd, Hit) 400 m off at 300 m
		AirburstFar,   // the same 4 km off (the impostor tier from the unit)
		Mk82,          // a WeaponImpact of one Mk 82 on the ground 1 km off
		Fab500,        // a WeaponImpact of one FAB-500 on the ground 3 km off
		SamLaunch,     // a neighbour's SamLaunch 300 m off toward a point 6 km off at 2 km
		BombStick,     // a WeaponRelease of six Mk 82 from 400 m, 2 km off
		GunPass,       // a ZSU-23-4's GunPass 500 m off toward a point 2 km up
		Contrail,      // an aircraft above 9 km and its contrails (no event)
		AircraftDown,  // the first live aircraft shot down: its wreck followed
		FlakHeavy,     // a KS-30's GunPass 3 km off at a point 3 km up (130 mm, TNT: black puffs, LOOK 4)
		FlakLight,     // an S-60's GunPass 1.5 km off at a point 1.2 km up (57 mm, A-IX-2: light grey, LOOK 4)
		Cluster,       // a WeaponRelease of two CBU-87 from 400 m, 2 km off: the carpet (LOOK 5)
		Cruise,        // a CruiseImpact of a BGM-109C on the ground 2 km off (LOOK 7)
		Arm,           // an AGM-45's ArmBurst over a point 1.5 km off, diving at 600 m/s (LOOK 8)
		RocketPass,    // a WeaponRelease of eight S-8 from a 20 deg dive, 1.5 km off: smoke that ends in mid-air (LOOK 10)
		AfterburnerF15,    // the photographer's own F-15C at 420 m/s, 600 m up: the F100's flame (LOOK 13)
		AfterburnerMig23,  // its own MiG-23ML the same: a Soviet engine's flame
		ManpadsLaunch, // a SamLaunch of a 9M39 150 m off: the corkscrew (LOOK 9.2)
		OwnLaunch,     // our own 9M33 off the unit's launcher at 27 deg: the launch cloud and the trail (LOOK 9.3)
	};

	/** Where the camera stands for one picture of a subject. */
	enum class ESimFxShotCam : uint8
	{
		Near,    // the subject's own pose, framing it
		Far,     // at the player's unit, a 60 deg lens
		Zoom,    // at the player's unit, a narrow lens
		Ground,  // at the player's unit looking up at the aircraft (contrails)
	};

	struct FSimFxShotVariant
	{
		ESimFxShotCam Cam = ESimFxShotCam::Near;
		const TCHAR* Suffix = TEXT("");
		float FovDeg = 60.0f;
	};

	struct FSimFxShotSubject
	{
		ESimFxShotKind Kind = ESimFxShotKind::AirburstNear;
		FString Name;
		TArray<double> Delays;  // simulation seconds after its events were pushed
		TArray<FSimFxShotVariant> Variants;
	};

	constexpr double SimFxShotRealLimitS = 900.0;   // the whole run, wall clock
	constexpr double SimFxShotWaitRunnerS = 240.0;  // for a scenario to run
	constexpr double SimFxShotWarmUpSimS = 4.0;     // of simulation time before the first subject
	constexpr double SimFxShotSettleS = 0.8;        // a camera placed before its events (the effects spawn only when seen)
	constexpr double SimFxShotAimS = 0.5;           // a picture's settle (temporal AA, auto exposure)
	constexpr double SimFxShotAfterS = 0.35;        // the screenshot written before the next move
	constexpr double SimFxShotGapS = 0.5;
	constexpr double SimFxShotContrailWaitS = 20.0;   // simulation seconds for an aircraft above 9 km to appear
	constexpr double SimFxShotAircraftWaitS = 90.0;   // for any aircraft in the air
	constexpr double SimFxShotGravity = 9.80665;
	constexpr double SimFxShotContrailAltM = 9000.0;     // contrails form only this high (the effects' rule)
	constexpr double SimFxShotContrailRangeM = 40000.0;  // an aircraft farther off is no picture
	constexpr double SimFxShotDownRangeM = 15000.0;      // nor is a wreck farther off (the far shot is from the unit)
	constexpr int32 SimFxShotOwnNumber = 91;             // our own missile's number in its picture (none of a scenario's)
	const TCHAR* const SimFxShotTag = TEXT("fxshots");

	simcore::Vec3 SimFxShotAz(double AzDeg)
	{
		const double A = FMath::DegreesToRadians(AzDeg);
		return simcore::Vec3(FMath::Sin(A), FMath::Cos(A), 0.0);
	}

	simcore::Vec3 SimFxShotFlat(const simcore::Vec3& V)
	{
		const simcore::Vec3 H(V.x, V.y, 0.0);
		return H.HorizontalLength() > 1e-6 ? H.Normalized() : simcore::Vec3(0.0, 1.0, 0.0);
	}

	/** The ground under P (ENU), raised by UpM. */
	simcore::Vec3 SimFxShotGround(const simcore::ScenarioRunner& Runner, const simcore::Vec3& P, double UpM)
	{
		simcore::Vec3 G = Runner.GroundEnu(P.x, P.y);
		G.z += UpM;
		return G;
	}

	/** P, but at least MinM above the ground under it. */
	simcore::Vec3 SimFxShotAbove(const simcore::ScenarioRunner& Runner, const simcore::Vec3& P, double MinM)
	{
		const simcore::Vec3 G = Runner.GroundEnu(P.x, P.y);
		simcore::Vec3 Out = P;
		Out.z = FMath::Max(Out.z, G.z + MinM);
		return Out;
	}

	bool SimFxShotIsAircraft(const FSimEntityState& S)
	{
		return S.bAlive && !S.bIsMissile && !S.bIsGround && S.Id >= 0 && S.Id < SimFxIds::MissileBase;
	}

	simcore::fx::Event SimFxShotEvent(simcore::fx::EventKind Kind, double Now)
	{
		simcore::fx::Event E;
		E.kind = Kind;
		E.t_s = Now;
		E.end_t_s = Now;
		return E;
	}
}

struct FSimFxShots::FState
{
	enum class EPhase : uint8
	{
		WaitRunner,  // no scenario yet, or its first seconds
		Setup,       // the next subject: find it, place the camera
		Settle,      // the camera placed, its events not yet pushed
		Running,     // the events pushed, waiting for the next shot's delay
		ShotAim,     // paused, a variant's camera placed
		ShotAfter,   // its screenshot asked for
		Gap,         // between subjects
		Finished,
	};

	EPhase Phase = EPhase::WaitRunner;
	double Clock = 0.0;     // wall seconds since the photographer began
	double PhaseAt = 0.0;   // the wall time the phase's wait ends
	double ReadyAtSimT = -1.0;
	TArray<FSimFxShotSubject> Subjects;
	int32 Subject = 0;
	int32 Shot = 0;
	int32 Variant = 0;
	double T0 = 0.0;        // the runner's time its events were pushed
	double WaitFromT = -1.0;  // the runner's time a subject began waiting for its aircraft (-1: not waiting)
	const void* RunnerSeen = nullptr;

	// The subject's geometry (ENU m), found at its setup.
	simcore::Vec3 Unit;      // the player's unit, on the ground
	simcore::Vec3 At;        // a burst, a site, a release point
	simcore::Vec3 Aim;       // where it goes: the missile's point, the bombs' aim, the guns' aim
	simcore::Vec3 CamFrom;   // the near camera
	simcore::Vec3 LookAt;    // what the near camera looks at
	simcore::Vec3 Dir;       // its way (unit)
	simcore::Vec3 Side;      // a horizontal across it (unit)
	FVector OffsetU = FVector::ZeroVector;  // a followed subject's camera offset (Unreal cm)
	int32 EntityId = -1;
	int32 ContrailId = -1;   // the aircraft the contrail view took (the shoot-down prefers another)
	FVector LastSubjectU = FVector::ZeroVector;

	// Events to push later (a neighbour's missile's end), at a runner time.
	std::vector<std::pair<double, simcore::fx::Event>> Pending;

	bool bPausedByUs = false;
	bool bNightApplied = false;
	bool bInjectedContrail = false;  // the photographer's own aircraft put into this runner's scenario
	bool bInjectedDown = false;
	bool bInjectedAbF15 = false;
	bool bInjectedAbMig23 = false;
	int32 Taken = 0;
	int32 Skipped = 0;

	// The cost while a subject's effects run (not paused): the effects' game-thread time and the whole frame's, ms.
	double SumFxMs = 0.0;
	double PeakFxMs = 0.0;
	double SumFrameMs = 0.0;
	double PeakFrameMs = 0.0;
	double SumAudioMs = 0.0;   // the sound's game-thread time (USimFxAudioSubsystem)
	double PeakAudioMs = 0.0;
	int32 PeakSprites = 0;
	int32 PeakRibbons = 0;
	int32 PeakLights = 0;
	int32 CostFrames = 0;
};

namespace
{
	using FSimFxShotState = FSimFxShots::FState;

	/** The shot list (docs/fx/README.md 4.1; the coordinator's brief). */
	void SimFxShotList(FSimFxShotState& S)
	{
		const FSimFxShotVariant Near{ESimFxShotCam::Near, TEXT(""), 60.0f};
		const FSimFxShotVariant Far{ESimFxShotCam::Far, TEXT("_far"), 60.0f};
		auto Add = [&S](ESimFxShotKind Kind, const TCHAR* Name, TArray<double> Delays, TArray<FSimFxShotVariant> Variants)
		{
			FSimFxShotSubject& Sub = S.Subjects.AddDefaulted_GetRef();
			Sub.Kind = Kind;
			Sub.Name = Name;
			Sub.Delays = MoveTemp(Delays);
			Sub.Variants = MoveTemp(Variants);
		};
		Add(ESimFxShotKind::AirburstNear, TEXT("airburst15kg_400m"), {0.1, 0.5, 2.0}, {Near, Far});
		Add(ESimFxShotKind::AirburstFar, TEXT("airburst15kg_4km"), {0.1, 0.5, 2.0},
			{FSimFxShotVariant{ESimFxShotCam::Far, TEXT(""), 60.0f}, FSimFxShotVariant{ESimFxShotCam::Zoom, TEXT("_zoom"), 6.0f}});
		Add(ESimFxShotKind::Mk82, TEXT("mk82_ground_1km"), {0.1, 0.6, 4.0}, {Near, Far});
		Add(ESimFxShotKind::Fab500, TEXT("fab500_ground_3km"), {0.3, 5.0}, {Near, Far});
		Add(ESimFxShotKind::SamLaunch, TEXT("sam_launch_neighbour"), {1.0, 3.0, 6.0}, {Near, Far});
		Add(ESimFxShotKind::BombStick, TEXT("mk82x6_release"), {2.0, 5.0, 9.4, 12.0}, {Near, Far});
		Add(ESimFxShotKind::GunPass, TEXT("zsu234_gunpass"), {1.0, 2.5}, {FSimFxShotVariant{ESimFxShotCam::Near, TEXT(""), 90.0f}, Far});
		Add(ESimFxShotKind::Contrail, TEXT("contrail"), {3.0, 10.0},
			{Near, FSimFxShotVariant{ESimFxShotCam::Ground, TEXT("_ground"), 30.0f}});
		Add(ESimFxShotKind::AircraftDown, TEXT("aircraft_down"), {2.0, 8.0}, {Near, Far});
		Add(ESimFxShotKind::FlakHeavy, TEXT("ks30_flak"), {8.0, 14.0, 40.0}, {Near, Far});
		Add(ESimFxShotKind::FlakLight, TEXT("s60_flak"), {3.0, 6.0, 25.0}, {Near, Far});
		Add(ESimFxShotKind::Cluster, TEXT("cbu87_carpet"), {9.0, 10.0, 12.0, 40.0}, {Near, Far});
		Add(ESimFxShotKind::Cruise, TEXT("bgm109c_impact"), {0.1, 1.0, 4.0, 20.0}, {Near, Far});
		Add(ESimFxShotKind::Arm, TEXT("agm45_burst"), {0.1, 1.0, 4.0, 20.0}, {Near, Far});
		Add(ESimFxShotKind::RocketPass, TEXT("s8_rocket_pass"), {0.5, 1.5, 5.0}, {Near, Far});
		Add(ESimFxShotKind::AfterburnerF15, TEXT("afterburner_f15"), {1.0, 2.0}, {FSimFxShotVariant{ESimFxShotCam::Near, TEXT(""), 35.0f}, Far});
		Add(ESimFxShotKind::AfterburnerMig23, TEXT("afterburner_mig23"), {1.0, 2.0}, {FSimFxShotVariant{ESimFxShotCam::Near, TEXT(""), 35.0f}, Far});
		Add(ESimFxShotKind::ManpadsLaunch, TEXT("manpads_launch"), {0.7, 2.0, 4.0}, {Near, Far});
		Add(ESimFxShotKind::OwnLaunch, TEXT("own_launch"), {0.5, 2.0, 6.0, 15.0}, {Near, FSimFxShotVariant{ESimFxShotCam::Ground, TEXT("_from_vehicle"), 60.0f}});
	}

	/** The live aircraft of this name (the photographer's own put into the scenario). */
	const FSimEntityState* SimFxShotNamed(const USimWorldSubsystem& Sim, const TCHAR* Name)
	{
		for (const FSimEntityState& E : Sim.GetEntityStatesCached())
		{
			if (SimFxShotIsAircraft(E) && E.Name == Name)
			{
				return &E;
			}
		}
		return nullptr;
	}

	const TCHAR* SimFxShotAbName(ESimFxShotKind Kind)
	{
		return Kind == ESimFxShotKind::AfterburnerF15 ? TEXT("fxshots_ab_f15") : TEXT("fxshots_ab_mig23");
	}

	/** The player's unit: the first launcher of the snapshot, else the first ground entity, else the ENU origin. */
	simcore::Vec3 SimFxShotUnit(const USimWorldSubsystem& Sim, const simcore::ScenarioRunner& Runner)
	{
		const FSimEntityState* Best = nullptr;
		for (const FSimEntityState& E : Sim.GetEntityStatesCached())
		{
			if (!E.bIsGround)
			{
				continue;
			}
			if (E.Kind == TEXT("launcher"))
			{
				Best = &E;
				break;
			}
			if (Best == nullptr)
			{
				Best = &E;
			}
		}
		const simcore::Vec3 P = Best != nullptr ? Sim.ToEnu(Sim.GetRenderLocation(*Best)) : simcore::Vec3();
		return SimFxShotGround(Runner, P, 0.0);
	}

	/** The live aircraft nearest the unit, at least MinAltM up and within MaxRangeM of it (AvoidId only if no other). */
	const FSimEntityState* SimFxShotAircraft(const USimWorldSubsystem& Sim, const simcore::ScenarioRunner& Runner, const simcore::Vec3& Unit,
		double MinAltM, double MaxRangeM, int32 AvoidId)
	{
		const FSimEntityState* Best = nullptr;
		const FSimEntityState* Fallback = nullptr;
		double BestM = MaxRangeM;
		for (const FSimEntityState& E : Sim.GetEntityStatesCached())
		{
			if (!SimFxShotIsAircraft(E))
			{
				continue;
			}
			const simcore::Vec3 P = Sim.ToEnu(Sim.GetRenderLocation(E));
			if (MinAltM > 0.0 && Runner.GetWorld().Frame().AltitudeOf(P) < MinAltM)
			{
				continue;
			}
			const double RangeM = (P - Unit).Length();
			if (RangeM > MaxRangeM)
			{
				continue;
			}
			if (E.Id == AvoidId)
			{
				Fallback = &E;
				continue;
			}
			if (RangeM <= BestM)
			{
				BestM = RangeM;
				Best = &E;
			}
		}
		return Best != nullptr ? Best : Fallback;
	}

	/**
	 * An aircraft of the photographer's own put into the running scenario when it has none to shoot (a friend, so the
	 * crew leaves it alone): StartAzDeg / RangeM from the unit at UpM above it, flying HeadingDeg. False if the runner
	 * would not take it.
	 */
	bool SimFxShotInject(simcore::ScenarioRunner& Runner, const simcore::Vec3& Unit, const char* Id, const char* Model, double StartAzDeg,
		double RangeM, double UpM, double HeadingDeg, double SpeedMps)
	{
		simcore::ScenarioTarget T;
		T.id = Id;
		T.name = Id;
		T.model = Model;
		T.friendly = true;
		T.speed_mps = SpeedMps;
		T.start = Unit + SimFxShotAz(StartAzDeg) * RangeM + simcore::Vec3(0.0, 0.0, UpM);
		T.waypoints.push_back(T.start + SimFxShotAz(HeadingDeg) * 80000.0);
		std::vector<simcore::ScenarioTarget> Ts;
		Ts.push_back(T);
		std::string Why;
		const bool bOk = Runner.AddTargets(std::move(Ts), "the effects' photographer", &Why);
		UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s %s"), UTF8_TO_TCHAR(Id), bOk ? TEXT("put into the scenario") : *FString::Printf(TEXT("not taken: %s"), UTF8_TO_TCHAR(Why.c_str())));
		return bOk;
	}

	/** Find the subject and its geometry; false (with the reason) when it is not there to be photographed. */
	bool SimFxShotSetup(FSimFxShotState& S, const FSimFxShotSubject& Sub, const USimWorldSubsystem& Sim, const simcore::ScenarioRunner& Runner,
		FString& OutWhy)
	{
		using simcore::Vec3;
		S.Unit = SimFxShotUnit(Sim, Runner);
		S.EntityId = -1;
		const Vec3 Up(0.0, 0.0, 1.0);
		switch (Sub.Kind)
		{
		case ESimFxShotKind::AirburstNear:
		case ESimFxShotKind::AirburstFar:
		{
			const bool bNear = Sub.Kind == ESimFxShotKind::AirburstNear;
			const Vec3 H = SimFxShotAz(bNear ? 45.0 : 100.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * (bNear ? 400.0 : 4000.0), 300.0);
			S.Dir = (S.At - (S.Unit + Up * 3.0)).Normalized();
			S.CamFrom = SimFxShotAbove(Runner, S.At - H * 90.0 - Up * 12.0, 5.0);
			S.LookAt = S.At;
			return true;
		}
		case ESimFxShotKind::Mk82:
		case ESimFxShotKind::Fab500:
		{
			const bool bMk82 = Sub.Kind == ESimFxShotKind::Mk82;
			const Vec3 H = SimFxShotAz(bMk82 ? 140.0 : 200.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * (bMk82 ? 1000.0 : 3000.0), 0.0);
			S.CamFrom = SimFxShotGround(Runner, S.At - H * (bMk82 ? 250.0 : 400.0), bMk82 ? 8.0 : 10.0);
			S.LookAt = S.At + Up * (bMk82 ? 20.0 : 40.0);
			return true;
		}
		case ESimFxShotKind::SamLaunch:
		{
			S.At = SimFxShotGround(Runner, S.Unit + SimFxShotAz(270.0) * 300.0, 2.0);
			S.Aim = SimFxShotGround(Runner, S.At + SimFxShotAz(330.0) * 6000.0, 2000.0);
			S.Dir = (S.Aim - S.At).Normalized();
			const Vec3 DirH = SimFxShotFlat(S.Dir);
			S.Side = simcore::Cross(DirH, Up).Normalized();
			S.CamFrom = SimFxShotAbove(Runner, S.At + S.Side * 60.0 - DirH * 40.0 + Up * 4.0, 2.0);
			S.LookAt = S.At + S.Dir * 200.0;
			return true;
		}
		case ESimFxShotKind::BombStick:
		{
			const Vec3 H = SimFxShotAz(0.0);
			const Vec3 Fly = SimFxShotAz(90.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * 2000.0, 400.0);  // the release point
			S.Aim = SimFxShotGround(Runner, S.At + Fly * 1000.0, 0.0);
			S.Dir = Fly;
			S.Side = H * -1.0;  // the camera stands on the unit's side of the stick
			S.CamFrom = SimFxShotAbove(Runner, S.At + S.Side * 250.0, 5.0);
			S.LookAt = S.At;
			return true;
		}
		case ESimFxShotKind::GunPass:
		{
			const Vec3 H = SimFxShotAz(160.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * 500.0, 1.5);
			S.Aim = SimFxShotGround(Runner, S.At + H * 1200.0, 0.0) + Up * 2000.0;
			S.Dir = (S.Aim - S.At).Normalized();
			const Vec3 DirH = SimFxShotFlat(S.Dir);
			S.Side = simcore::Cross(DirH, Up).Normalized();
			// Well to the side (a 90 deg lens): the guns and their stream across the picture, up to about 600 m out.
			S.CamFrom = SimFxShotAbove(Runner, S.At + S.Side * 450.0 - DirH * 100.0 + Up * 2.0, 2.0);
			S.LookAt = S.At + S.Dir * 250.0;
			return true;
		}
		case ESimFxShotKind::Contrail:
		{
			const FSimEntityState* A = SimFxShotAircraft(Sim, Runner, S.Unit, SimFxShotContrailAltM, SimFxShotContrailRangeM, -1);
			if (A == nullptr)
			{
				OutWhy = TEXT("no aircraft above 9 km within 40 km");
				return false;
			}
			S.EntityId = A->Id;
			S.ContrailId = A->Id;
			return true;
		}
		case ESimFxShotKind::AircraftDown:
		{
			const FSimEntityState* A = SimFxShotAircraft(Sim, Runner, S.Unit, 0.0, SimFxShotDownRangeM, S.ContrailId);
			if (A == nullptr)
			{
				OutWhy = TEXT("no live aircraft within 15 km");
				return false;
			}
			S.EntityId = A->Id;
			const FVector P = Sim.GetRenderLocation(*A);
			S.At = Sim.ToEnu(P);
			FVector ToUnit = Sim.ToUnreal(S.Unit) - P;
			ToUnit.Z = 0.0;
			ToUnit = ToUnit.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			S.OffsetU = ToUnit * 25000.0 + FVector(0.0, 0.0, 4000.0);
			return true;
		}
		case ESimFxShotKind::FlakHeavy:
		case ESimFxShotKind::FlakLight:
		{
			const bool bHeavy = Sub.Kind == ESimFxShotKind::FlakHeavy;
			const Vec3 H = SimFxShotAz(bHeavy ? 200.0 : 120.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * (bHeavy ? 3000.0 : 1500.0), 1.5);  // the guns
			S.Aim = SimFxShotGround(Runner, S.At + H * (bHeavy ? 1500.0 : 800.0), bHeavy ? 3000.0 : 1200.0);
			S.Dir = (S.Aim - S.At).Normalized();
			S.Side = simcore::Cross(SimFxShotFlat(S.Dir), Up).Normalized();
			// Beside the bursts and a little below them, across the line of fire.
			S.CamFrom = S.Aim + S.Side * (bHeavy ? 700.0 : 300.0) - Up * (bHeavy ? 150.0 : 60.0);
			S.LookAt = S.Aim;
			return true;
		}
		case ESimFxShotKind::Cluster:
		{
			const Vec3 H = SimFxShotAz(330.0);
			const Vec3 Fly = SimFxShotAz(60.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * 2000.0, 400.0);  // the release point
			S.Aim = SimFxShotGround(Runner, S.At + Fly * 1000.0, 0.0);
			S.Dir = Fly;
			S.Side = H * -1.0;
			S.CamFrom = SimFxShotGround(Runner, S.Aim + S.Side * 600.0, 90.0);  // above the fields' banks, looking down on the pattern
			S.LookAt = S.Aim + Up * 10.0;
			return true;
		}
		case ESimFxShotKind::Cruise:
		{
			const Vec3 H = SimFxShotAz(250.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * 2000.0, 0.0);
			S.Dir = (SimFxShotAz(20.0) + Up * -0.15).Normalized();  // low and fast, a shallow dive at the end
			S.CamFrom = SimFxShotGround(Runner, S.At - H * 450.0, 10.0);
			S.LookAt = S.At + Up * 40.0;
			return true;
		}
		case ESimFxShotKind::Arm:
		{
			const Vec3 H = SimFxShotAz(300.0);
			S.At = SimFxShotGround(Runner, S.Unit + H * 1500.0, 0.0);  // the antenna it homed on
			S.Dir = (SimFxShotAz(80.0) * FMath::Cos(FMath::DegreesToRadians(30.0)) - Up * FMath::Sin(FMath::DegreesToRadians(30.0))).Normalized();
			S.CamFrom = SimFxShotGround(Runner, S.At - H * 300.0, 5.0);
			S.LookAt = S.At + Up * 8.0;
			return true;
		}
		case ESimFxShotKind::RocketPass:
		{
			// A 20 deg dive at 200 m/s from 300 m, firing at a point 1.2 km on; the camera off to the side of the track.
			const Vec3 H = SimFxShotAz(20.0);
			const Vec3 Fly = SimFxShotAz(110.0);
			S.Aim = SimFxShotGround(Runner, S.Unit + H * 1500.0, 0.0);
			const double Dive = FMath::DegreesToRadians(20.0);
			S.Dir = (Fly * FMath::Cos(Dive) - Up * FMath::Sin(Dive)).Normalized();
			S.At = S.Aim - S.Dir * (300.0 / FMath::Sin(Dive));  // the release point, 300 m over the aim's ground
			S.Side = simcore::Cross(Fly, Up).Normalized();
			S.CamFrom = SimFxShotAbove(Runner, (S.At + S.Aim) * 0.5 - S.Side * 450.0, 20.0);
			S.LookAt = S.At + S.Dir * 350.0;
			return true;
		}
		case ESimFxShotKind::AfterburnerF15:
		case ESimFxShotKind::AfterburnerMig23:
		{
			const FSimEntityState* A = SimFxShotNamed(Sim, SimFxShotAbName(Sub.Kind));
			if (A == nullptr)
			{
				OutWhy = TEXT("its own aircraft not in the air");
				return false;
			}
			S.EntityId = A->Id;
			return true;
		}
		case ESimFxShotKind::ManpadsLaunch:
		{
			S.At = SimFxShotGround(Runner, S.Unit + SimFxShotAz(200.0) * 150.0, 1.5);
			S.Aim = SimFxShotGround(Runner, S.At + SimFxShotAz(250.0) * 2500.0, 1200.0);
			S.Dir = (S.Aim - S.At).Normalized();
			const Vec3 DirH = SimFxShotFlat(S.Dir);
			S.Side = simcore::Cross(DirH, Up).Normalized();
			S.CamFrom = SimFxShotAbove(Runner, S.At + S.Side * 30.0 - DirH * 20.0 + Up * 2.0, 1.7);
			S.LookAt = S.At + S.Dir * 100.0;
			return true;
		}
		case ESimFxShotKind::OwnLaunch:
		{
			// Off the launcher's rails 3.5 m up at a constant 27 deg (LOOK 9.2); the camera a man standing 25 m off.
			const double El = FMath::DegreesToRadians(27.0);
			const Vec3 H = SimFxShotAz(40.0);
			S.At = S.Unit + Up * 3.5;
			S.Dir = (H * FMath::Cos(El) + Up * FMath::Sin(El)).Normalized();
			S.Aim = S.At + S.Dir * 9000.0;
			S.Side = simcore::Cross(H, Up).Normalized();
			S.CamFrom = SimFxShotAbove(Runner, S.Unit + S.Side * 22.0 - H * 12.0, 1.7);
			S.LookAt = S.At + S.Dir * 150.0;
			return true;
		}
		}
		return false;
	}

	/** Push the subject's events (t_s: the runner's time now). */
	void SimFxShotPush(FSimFxShotState& S, const FSimFxShotSubject& Sub, const USimWorldSubsystem& Sim, simcore::ScenarioRunner& Runner,
		double Now)
	{
		using simcore::Vec3;
		using simcore::fx::EventKind;
		simcore::fx::Feed& Feed = Runner.Fx();
		const std::string Tag = TCHAR_TO_UTF8(SimFxShotTag);
		switch (Sub.Kind)
		{
		case ESimFxShotKind::AirburstNear:
		case ESimFxShotKind::AirburstFar:
		{
			// Our missile's warhead on its target: the 9M33's 15 kg warhead (the effects take the charge from the munition).
			simcore::fx::Event E = SimFxShotEvent(EventKind::MissileEnd, Now);
			E.position = S.At - S.Dir * 25.0;
			E.velocity = S.Dir * 550.0;
			E.aim = S.At;
			E.end = simcore::fx::BurstEnd::Hit;
			E.number = Sub.Kind == ESimFxShotKind::AirburstNear ? 97 : 98;
			E.weapon = "9m33m2";
			E.charge_kg = 15.0;
			E.source = "own";
			E.target = Tag;
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::Mk82:
		case ESimFxShotKind::Fab500:
		{
			simcore::fx::Event E = SimFxShotEvent(EventKind::WeaponImpact, Now);
			E.position = S.At;
			E.aim = S.At;
			E.weapon = Sub.Kind == ESimFxShotKind::Mk82 ? "mk82" : "fab500";
			E.number = 1;
			E.source = Tag + "_" + E.weapon;
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::SamLaunch:
		{
			simcore::fx::Event E = SimFxShotEvent(EventKind::SamLaunch, Now);
			E.position = S.At;
			E.velocity = S.Dir * 30.0;
			E.aim = S.Aim;
			E.end_t_s = Now + 12.0;
			E.source = Tag + "_neighbour";
			E.weapon = "osa";
			Feed.Push(E);
			// Its end, later: it misses and destroys itself at the point.
			simcore::fx::Event End = SimFxShotEvent(EventKind::SamEnd, Now + 12.0);
			End.position = S.At;
			End.aim = S.Aim;
			End.end = simcore::fx::BurstEnd::SelfDestruct;
			End.source = E.source;
			End.weapon = E.weapon;
			S.Pending.emplace_back(Now + 12.0, End);
			break;
		}
		case ESimFxShotKind::BombStick:
		{
			// Level release at 115 m/s: about 1 km on over the fall from 400 m (the fall time without drag).
			const double Height = S.At.z - Runner.GroundEnu(S.Aim.x, S.Aim.y).z;
			const double FallS = FMath::Sqrt(2.0 * FMath::Max(Height, 10.0) / SimFxShotGravity);
			simcore::fx::Event E = SimFxShotEvent(EventKind::WeaponRelease, Now);
			E.position = S.At;
			E.velocity = S.Dir * 115.0;
			E.aim = S.Aim;
			E.end_t_s = Now + FallS;
			E.world_id = -1;  // the event's own position and velocity: the stick lands where it is framed
			E.source = Tag + "_bomber";
			E.weapon = "mk82";
			E.number = 6;
			E.delivery = "level";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::GunPass:
		{
			simcore::fx::Event E = SimFxShotEvent(EventKind::GunPass, Now);
			E.position = S.At;
			E.aim = S.Aim;
			E.end_t_s = Now + 3.0;
			E.world_id = -1;
			E.number = 200;
			E.source = Tag + "_zsu";
			E.weapon = "zsu234";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::Contrail:
			break;
		case ESimFxShotKind::AircraftDown:
		{
			const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId);
			if (A == nullptr)
			{
				break;
			}
			simcore::fx::Event E = SimFxShotEvent(EventKind::AircraftDown, Now);
			const FVector Loc = Sim.GetRenderLocation(*A);
			E.position = Sim.ToEnu(Loc);
			// Through the georeference too: under Cesium the level's X is east and Y south, not the flat mapping's north and east.
			E.velocity = Sim.ToEnu(Loc + A->Velocity) - E.position;
			E.world_id = A->Id;
			E.target = TCHAR_TO_UTF8(*A->Name);
			E.source = E.target;
			E.fire = true;
			E.falls = true;
			E.model = TCHAR_TO_UTF8(*A->Model);
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::FlakHeavy:
		case ESimFxShotKind::FlakLight:
		{
			const bool bHeavy = Sub.Kind == ESimFxShotKind::FlakHeavy;
			simcore::fx::Event E = SimFxShotEvent(EventKind::GunPass, Now);
			E.position = S.At;
			E.aim = S.Aim;
			// A KS-30 fires about 10 rounds a minute; an S-60 2 a second in bursts of one or two clips with pauses
			// between (effects.json's guns), so a few bursts' worth, not the 4-8 rounds of a 4 s pass.
			E.end_t_s = Now + (bHeavy ? 12.0 : 10.0);
			E.world_id = -1;
			E.number = bHeavy ? 8 : 30;
			E.source = Tag + (bHeavy ? "_ks30" : "_s60");
			E.weapon = bHeavy ? "ks30" : "s60";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::Cluster:
		{
			const double Height = S.At.z - Runner.GroundEnu(S.Aim.x, S.Aim.y).z;
			const double FallS = FMath::Sqrt(2.0 * FMath::Max(Height, 10.0) / SimFxShotGravity);
			simcore::fx::Event E = SimFxShotEvent(EventKind::WeaponRelease, Now);
			E.position = S.At;
			E.velocity = S.Dir * 115.0;
			E.aim = S.Aim;
			E.end_t_s = Now + FallS;
			E.world_id = -1;
			E.source = Tag + "_cluster";
			E.weapon = "cbu87";
			E.number = 2;
			E.delivery = "level";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::Cruise:
		{
			simcore::fx::Event E = SimFxShotEvent(EventKind::CruiseImpact, Now);
			E.position = S.At;
			E.aim = S.At;
			E.velocity = S.Dir * 240.0;
			E.weapon = "bgm109c";
			E.source = Tag + "_cruise";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::Arm:
		{
			// No missile flown before it: the burst takes the event's own place and velocity.
			simcore::fx::Event E = SimFxShotEvent(EventKind::ArmBurst, Now);
			E.position = S.At;
			E.aim = S.At;
			E.velocity = S.Dir * 600.0;
			E.weapon = "agm45";
			E.source = Tag + "_arm";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::RocketPass:
		{
			const double Dist = (S.Aim - S.At).Length();
			simcore::fx::Event E = SimFxShotEvent(EventKind::WeaponRelease, Now);
			E.position = S.At;
			E.velocity = S.Dir * 200.0;
			E.aim = S.Aim;
			E.end_t_s = Now + Dist / 600.0;
			E.world_id = -1;
			E.source = Tag + "_rockets";
			E.weapon = "s8";
			E.number = 8;
			E.delivery = "dive";
			Feed.Push(E);
			break;
		}
		case ESimFxShotKind::AfterburnerF15:
		case ESimFxShotKind::AfterburnerMig23:
			break;
		case ESimFxShotKind::ManpadsLaunch:
		{
			simcore::fx::Event E = SimFxShotEvent(EventKind::SamLaunch, Now);
			E.position = S.At;
			E.velocity = S.Dir * 30.0;
			E.aim = S.Aim;
			E.end_t_s = Now + 7.0;
			E.source = Tag + "_manpads";
			E.weapon = "9m39";
			Feed.Push(E);
			simcore::fx::Event End = SimFxShotEvent(EventKind::SamEnd, Now + 7.0);
			End.position = S.At;
			End.aim = S.Aim;
			End.end = simcore::fx::BurstEnd::SelfDestruct;
			End.source = E.source;
			End.weapon = E.weapon;
			S.Pending.emplace_back(Now + 7.0, End);
			break;
		}
		case ESimFxShotKind::OwnLaunch:
		{
			// Our launch (the cloud at the launcher), then at once its flight on to where it destroys itself: with no
			// missile of the scenario's under this number the effects fly it themselves (fx/events.hpp, MissileEnd Wide).
			simcore::fx::Event E = SimFxShotEvent(EventKind::MissileLaunch, Now);
			E.position = S.At;
			E.velocity = S.Dir * 30.0;
			E.number = SimFxShotOwnNumber;
			E.weapon = "9m33m2";
			E.source = "own";
			E.end_t_s = Now + 26.0;
			Feed.Push(E);
			simcore::fx::Event End = SimFxShotEvent(EventKind::MissileEnd, Now);
			End.position = S.At;
			End.velocity = S.Dir * 500.0;
			End.aim = S.Aim;
			End.end = simcore::fx::BurstEnd::Wide;
			End.end_t_s = Now + 18.0;
			End.number = SimFxShotOwnNumber;
			End.weapon = "9m33m2";
			End.source = "own";
			Feed.Push(End);
			break;
		}
		}
	}

	/** The flown missile of a subject (a neighbour's, a MANPADS, our own), if the effects have it. */
	const FSimFxMissile* SimFxShotSamMissile(const FSimFxWorld& W, ESimFxShotKind Kind)
	{
		const FString Source = Kind == ESimFxShotKind::OwnLaunch ? FString(TEXT("own"))
			: FString(SimFxShotTag) + (Kind == ESimFxShotKind::ManpadsLaunch ? TEXT("_manpads") : TEXT("_neighbour"));
		const FSimFxMissile* Best = nullptr;
		for (const FSimFxMissile& M : W.Missiles)
		{
			if (Kind == ESimFxShotKind::OwnLaunch && M.Number != SimFxShotOwnNumber)
			{
				continue;
			}
			if (M.Source == Source && M.Mode == FSimFxMissile::EMode::Flown && (Best == nullptr || M.LaunchT > Best->LaunchT))
			{
				Best = &M;
			}
		}
		return Best;
	}

	/** Where the subject is now (Unreal cm): what a camera at the unit looks at. */
	FVector SimFxShotSubjectU(FSimFxShotState& S, const FSimFxShotSubject& Sub, const USimWorldSubsystem& Sim, const USimFxSubsystem& Fx,
		double RenderT)
	{
		const FSimFxWorld* W = Fx.GetFxWorld();
		switch (Sub.Kind)
		{
		case ESimFxShotKind::SamLaunch:
		case ESimFxShotKind::ManpadsLaunch:
		case ESimFxShotKind::OwnLaunch:
		{
			const FSimFxMissile* M = W != nullptr ? SimFxShotSamMissile(*W, Sub.Kind) : nullptr;
			if (M != nullptr && !M->bDone && !M->PosU.IsZero())
			{
				return M->PosU;
			}
			const double Along = FMath::Clamp((RenderT - S.T0) * 500.0, Sub.Kind == ESimFxShotKind::SamLaunch ? 200.0 : 60.0, 6000.0);
			return Sim.ToUnreal(S.At + S.Dir * Along);
		}
		case ESimFxShotKind::AfterburnerF15:
		case ESimFxShotKind::AfterburnerMig23:
			if (const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId))
			{
				S.LastSubjectU = Sim.GetRenderLocation(*A);
			}
			return S.LastSubjectU;
		case ESimFxShotKind::BombStick:
		{
			// The mean of the stick's bombs in flight; before the first, the release point; after the last, the aim.
			simcore::Vec3 Sum;
			int32 N = 0;
			const FString Source = FString(SimFxShotTag) + TEXT("_bomber");
			if (W != nullptr)
			{
				for (const FSimFxBomb& B : W->Bombs)
				{
					if (B.Source == Source && B.bReleased && !B.bDone && !B.Path.Done(W->Now))
					{
						Sum += B.Path.Position(W->Now);
						++N;
					}
				}
			}
			if (N > 0)
			{
				return Sim.ToUnreal(Sum / static_cast<double>(N));
			}
			return Sim.ToUnreal(RenderT - S.T0 < 1.0 ? S.At : S.Aim + simcore::Vec3(0.0, 0.0, 15.0));
		}
		case ESimFxShotKind::Contrail:
		case ESimFxShotKind::AircraftDown:
		{
			FVector Loc;
			FRotator Rot;
			bool bVisible = false;
			if (Sub.Kind == ESimFxShotKind::AircraftDown && Fx.GetWreckPose(S.EntityId, Loc, Rot, bVisible))
			{
				S.LastSubjectU = bVisible ? Loc : Loc + FVector(0.0, 0.0, 2000.0);
				return S.LastSubjectU;
			}
			if (const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId))
			{
				S.LastSubjectU = Sim.GetRenderLocation(*A);
				return S.LastSubjectU;
			}
			return S.LastSubjectU;
		}
		default:
			return Sim.ToUnreal(S.LookAt);
		}
	}

	/** The camera for one picture (Unreal cm). */
	void SimFxShotPose(FSimFxShotState& S, const FSimFxShotSubject& Sub, const FSimFxShotVariant& V, const USimWorldSubsystem& Sim,
		const USimFxSubsystem& Fx, const simcore::ScenarioRunner& Runner, double RenderT, FVector& OutFrom, FVector& OutAt)
	{
		const FVector Subject = SimFxShotSubjectU(S, Sub, Sim, Fx, RenderT);
		if (V.Cam != ESimFxShotCam::Near)
		{
			OutFrom = Sim.ToUnreal(S.Unit + simcore::Vec3(0.0, 0.0, 3.0));
			OutAt = Subject;
			return;
		}
		switch (Sub.Kind)
		{
		case ESimFxShotKind::SamLaunch:
		case ESimFxShotKind::ManpadsLaunch:
		case ESimFxShotKind::GunPass:
			// Beside the launch (the guns), looking along it at the missile (down the stream).
			OutFrom = Sim.ToUnreal(S.CamFrom);
			OutAt = Sub.Kind == ESimFxShotKind::GunPass ? Sim.ToUnreal(S.LookAt) : Subject;
			return;
		case ESimFxShotKind::OwnLaunch:
		{
			// A man beside the vehicle: the launch cloud round it and the trail climbing away, the missile kept in the
			// picture's upper part.
			OutFrom = Sim.ToUnreal(S.CamFrom);
			const FVector Vehicle = Sim.ToUnreal(S.At);
			OutAt = FMath::Lerp(Vehicle, Subject, 0.6);
			return;
		}
		case ESimFxShotKind::RocketPass:
			OutFrom = Sim.ToUnreal(S.CamFrom);
			OutAt = Sim.ToUnreal(S.LookAt);
			return;
		case ESimFxShotKind::AfterburnerF15:
		case ESimFxShotKind::AfterburnerMig23:
		{
			// Behind and beside it, close: the nozzles and the flame against the sky.
			const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId);
			FVector Fwd = A != nullptr ? A->Velocity.GetSafeNormal() : FVector::ForwardVector;
			if (Fwd.IsNearlyZero())
			{
				Fwd = FVector::ForwardVector;
			}
			const FVector Across = FVector::CrossProduct(Fwd, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
			OutFrom = Subject - Fwd * 4500.0 + Across * 1800.0 + FVector::UpVector * 300.0;
			OutAt = Subject - Fwd * 700.0;
			return;
		}
		case ESimFxShotKind::BombStick:
		{
			const simcore::Vec3 P = Sim.ToEnu(Subject);
			const bool bLanded = RenderT - S.T0 > 1.0 && FVector::Dist(Subject, Sim.ToUnreal(S.Aim + simcore::Vec3(0.0, 0.0, 15.0))) < 1.0;
			OutFrom = Sim.ToUnreal(SimFxShotAbove(Runner, P + S.Side * (bLanded ? 450.0 : 150.0), bLanded ? 10.0 : 5.0));
			OutAt = Subject;
			return;
		}
		case ESimFxShotKind::Contrail:
		{
			const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId);
			FVector Fwd = A != nullptr ? A->Velocity.GetSafeNormal() : FVector::ForwardVector;
			if (Fwd.IsNearlyZero())
			{
				Fwd = FVector::ForwardVector;
			}
			const FVector Across = FVector::CrossProduct(Fwd, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
			// 1.2 km to the side and a little below, looking at a point 400 m behind it: the aircraft about 19 deg off the
			// middle (in the 60 deg lens), its trails across the picture (from 600 m it was out of frame).
			OutFrom = Subject - Fwd * 30000.0 + Across * 120000.0 - FVector::UpVector * 15000.0;
			OutAt = Subject - Fwd * 40000.0;
			return;
		}
		case ESimFxShotKind::AircraftDown:
		{
			const simcore::Vec3 From = Sim.ToEnu(Subject + S.OffsetU);
			OutFrom = Sim.ToUnreal(SimFxShotAbove(Runner, From, 10.0));
			OutAt = Subject;
			return;
		}
		default:
			OutFrom = Sim.ToUnreal(S.CamFrom);
			OutAt = Subject;
			return;
		}
	}
}

FSimFxShots::FSimFxShots(UWorld* InWorld, const FString& InFolder, bool bInNight)
	: World(InWorld)
	, Folder(FPaths::ConvertRelativePathToFull(InFolder))
	, bNight(bInNight)
	, State(MakeUnique<FState>())
{
	IFileManager::Get().MakeDirectory(*Folder, true);
	SimFxShotList(*State);
	// -SimFxShotsOnly=<a+b>: only the subjects whose names hold one of these (a quick look at one effect).
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("-SimFxShotsOnly="), Only) && !Only.IsEmpty())
	{
		// Names split at ',' or '+' (FParse stops a value at a comma unless it is quoted, and a launcher may drop the quotes).
		TArray<FString> Keep;
		const TCHAR* Delims[] = {TEXT(","), TEXT("+")};
		Only.ParseIntoArray(Keep, Delims, UE_ARRAY_COUNT(Delims), true);
		State->Subjects.RemoveAll([&Keep](const FSimFxShotSubject& Sub)
		{
			return !Keep.ContainsByPredicate([&Sub](const FString& K) { return Sub.Name.Contains(K); });
		});
	}
	UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %d subjects into %s%s; waiting for a scenario (-SimScenario=<file>)"), State->Subjects.Num(), *Folder,
		bNight ? TEXT(" at night") : TEXT(""));
}

FSimFxShots::~FSimFxShots() = default;

bool FSimFxShots::IsDone() const
{
	return State->Phase == FState::EPhase::Finished;
}

void FSimFxShots::Tick(USimFxSubsystem& Fx, float RealDt)
{
	FState& S = *State;
	UWorld* W = World.Get();
	if (W == nullptr || S.Phase == FState::EPhase::Finished)
	{
		return;
	}
	S.Clock += FMath::Clamp(static_cast<double>(RealDt), 0.0, 1.0);
	USimWorldSubsystem* Sim = W->GetSubsystem<USimWorldSubsystem>();

	auto Finish = [&S, Sim](const TCHAR* Why)
	{
		UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: done (%s): %d pictures, %d subjects skipped"), Why, S.Taken, S.Skipped);
		if (S.bPausedByUs && Sim != nullptr)
		{
			Sim->SetSimPaused(false);
			S.bPausedByUs = false;
		}
		S.Phase = FState::EPhase::Finished;
		FGenericPlatformMisc::RequestExit(false);
	};

	if (S.Clock > SimFxShotRealLimitS)
	{
		Finish(TEXT("the time limit"));
		return;
	}

	// Night: the sky's clock at 23:00 (it moves on with the simulation's time from there).
	if (bNight && !S.bNightApplied)
	{
		for (TActorIterator<ASimSkyWeatherActor> It(W); It; ++It)
		{
			It->LocalTimeHours = 23.0f;
			S.bNightApplied = true;
		}
	}

	simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetFxRunner() : nullptr;
	if (Runner == nullptr)
	{
		if (S.Phase != FState::EPhase::WaitRunner)
		{
			Finish(TEXT("the scenario ended"));
		}
		else if (S.Clock > SimFxShotWaitRunnerS)
		{
			Finish(TEXT("no scenario ran: start one with -SimScenario=<file>"));
		}
		return;
	}
	if (S.RunnerSeen != Runner)
	{
		if (S.RunnerSeen != nullptr)
		{
			UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: a new scenario runner; the pending events dropped"));
		}
		S.RunnerSeen = Runner;
		S.Pending.clear();
		S.bInjectedContrail = false;
		S.bInjectedDown = false;
		S.bInjectedAbF15 = false;
		S.bInjectedAbMig23 = false;
	}

	const double RunnerT = Runner->Time();
	const double RenderT = Sim->GetRenderTimeSeconds();

	// Events due (a neighbour's missile's end).
	for (size_t I = 0; I < S.Pending.size();)
	{
		if (S.Pending[I].first <= RunnerT)
		{
			simcore::fx::Event E = S.Pending[I].second;
			E.t_s = RunnerT;
			Runner->Fx().Push(E);
			S.Pending.erase(S.Pending.begin() + static_cast<std::ptrdiff_t>(I));
		}
		else
		{
			++I;
		}
	}

	auto Aim = [this, W](const FVector& From, const FVector& At, float FovDeg)
	{
		ACameraActor* Cam = Camera.Get();
		if (Cam == nullptr)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
			if (Cam == nullptr)
			{
				return;
			}
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			Camera = Cam;
		}
		const FVector Look = (At - From).IsNearlyZero() ? FVector::ForwardVector : At - From;
		Cam->SetActorLocationAndRotation(From, Look.Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(FovDeg);
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (PC->GetViewTarget() != Cam)
			{
				PC->SetViewTargetWithBlend(Cam, 0.0f);
			}
			// Clean pictures: no menu, no HUD, no debug text over them.
			if (PC->MyHUD != nullptr)
			{
				PC->MyHUD->bShowHUD = false;
			}
		}
		if (GEngine != nullptr)
		{
			GEngine->bEnableOnScreenDebugMessagesDisplay = false;
		}
	};

	auto AimVariant = [&](int32 VariantIndex) -> float
	{
		const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
		const FSimFxShotVariant& V = Sub.Variants[FMath::Clamp(VariantIndex, 0, Sub.Variants.Num() - 1)];
		FVector From;
		FVector At;
		SimFxShotPose(S, Sub, V, *Sim, Fx, *Runner, RenderT, From, At);
		Aim(From, At, V.FovDeg);
		return static_cast<float>(FVector::Dist(From, At) / 100.0);
	};

	switch (S.Phase)
	{
	case FState::EPhase::WaitRunner:
		if (S.ReadyAtSimT < 0.0)
		{
			S.ReadyAtSimT = RunnerT + SimFxShotWarmUpSimS;
			UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: the scenario runs (t %.1f s); the first subject at %.1f s"), RunnerT, S.ReadyAtSimT);
		}
		if (RunnerT >= S.ReadyAtSimT && S.Clock > 3.0)
		{
			S.Phase = FState::EPhase::Setup;
			S.Subject = 0;
		}
		break;

	case FState::EPhase::Setup:
	{
		if (S.Subject >= S.Subjects.Num())
		{
			Finish(TEXT("the shot list"));
			return;
		}
		const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
		FString Why;
		if (!SimFxShotSetup(S, Sub, *Sim, *Runner, Why) || Sub.Delays.Num() == 0 || Sub.Variants.Num() == 0)
		{
			// An aircraft may yet come into the air: wait for one a while (simulation time) before giving up.
			const bool bAircraft = Sub.Kind == ESimFxShotKind::Contrail || Sub.Kind == ESimFxShotKind::AircraftDown ||
				Sub.Kind == ESimFxShotKind::AfterburnerF15 || Sub.Kind == ESimFxShotKind::AfterburnerMig23;
			const double WaitS = Sub.Kind == ESimFxShotKind::AircraftDown ? SimFxShotAircraftWaitS : SimFxShotContrailWaitS;
			if (bAircraft && !Why.IsEmpty())
			{
				// None in the scenario fit to shoot: the photographer's own, once a scenario run (a friend, across the
				// unit's view: one high enough for contrails, one low and near to go down).
				if (Sub.Kind == ESimFxShotKind::Contrail && !S.bInjectedContrail)
				{
					S.bInjectedContrail = true;
					SimFxShotInject(*Runner, S.Unit, "fxshots_contrail", "f4e", 300.0, 12000.0, 10500.0, 30.0, 230.0);
				}
				else if (Sub.Kind == ESimFxShotKind::AircraftDown && !S.bInjectedDown)
				{
					S.bInjectedDown = true;
					SimFxShotInject(*Runner, S.Unit, "fxshots_down", "mig21", 60.0, 5000.0, 1500.0, 150.0, 200.0);
				}
				else if (Sub.Kind == ESimFxShotKind::AfterburnerF15 && !S.bInjectedAbF15)
				{
					S.bInjectedAbF15 = true;
					SimFxShotInject(*Runner, S.Unit, "fxshots_ab_f15", "f15c", 120.0, 3000.0, 600.0, 330.0, 420.0);
				}
				else if (Sub.Kind == ESimFxShotKind::AfterburnerMig23 && !S.bInjectedAbMig23)
				{
					S.bInjectedAbMig23 = true;
					SimFxShotInject(*Runner, S.Unit, "fxshots_ab_mig23", "mig23ml", 240.0, 3000.0, 600.0, 30.0, 420.0);
				}
				if (S.WaitFromT < 0.0)
				{
					S.WaitFromT = RunnerT;
					UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s: %s yet; waiting up to %.0f s"), *Sub.Name, *Why, WaitS);
				}
				if (RunnerT - S.WaitFromT < WaitS)
				{
					break;
				}
			}
			UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s skipped: %s"), *Sub.Name, Why.IsEmpty() ? TEXT("nothing to shoot") : *Why);
			S.WaitFromT = -1.0;
			++S.Skipped;
			++S.Subject;
			break;
		}
		S.WaitFromT = -1.0;
		S.T0 = RunnerT;  // until pushed: the camera's guesses of where things go run from now
		AimVariant(0);
		S.Phase = FState::EPhase::Settle;
		S.PhaseAt = S.Clock + SimFxShotSettleS;
		break;
	}

	case FState::EPhase::Settle:
	{
		AimVariant(0);
		if (S.Clock >= S.PhaseAt)
		{
			const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
			S.T0 = RunnerT;
			// Each subject alone: what earlier subjects left in the air (a column, a trail) would stand in its pictures
			// and count in its cost.
			Fx.ClearEffects();
			SimFxShotPush(S, Sub, *Sim, *Runner, RunnerT);
			UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s: events at %.2f s"), *Sub.Name, RunnerT);
			S.Shot = 0;
			S.Phase = FState::EPhase::Running;
		}
		break;
	}

	case FState::EPhase::Running:
	{
		AimVariant(0);
		const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
		{
			const double FxMs = Fx.GetLastFrameMs();
			const double FrameMs = static_cast<double>(RealDt) * 1000.0;
			S.SumFxMs += FxMs;
			S.PeakFxMs = FMath::Max(S.PeakFxMs, FxMs);
			S.SumFrameMs += FrameMs;
			S.PeakFrameMs = FMath::Max(S.PeakFrameMs, FrameMs);
			const USimFxAudioSubsystem* Audio = W->GetSubsystem<USimFxAudioSubsystem>();
			const double AudioMs = Audio != nullptr ? static_cast<double>(Audio->GetLastFrameMs()) : 0.0;
			S.SumAudioMs += AudioMs;
			S.PeakAudioMs = FMath::Max(S.PeakAudioMs, AudioMs);
			S.PeakSprites = FMath::Max(S.PeakSprites, Fx.GetSpriteCount());
			S.PeakRibbons = FMath::Max(S.PeakRibbons, Fx.GetRibbonCount());
			S.PeakLights = FMath::Max(S.PeakLights, Fx.GetLightCount());
			++S.CostFrames;
		}
		if (RenderT >= S.T0 + Sub.Delays[S.Shot])
		{
			if (!Sim->IsSimPaused())
			{
				Sim->SetSimPaused(true);
				S.bPausedByUs = true;
			}
			S.Variant = 0;
			AimVariant(S.Variant);
			S.Phase = FState::EPhase::ShotAim;
			S.PhaseAt = S.Clock + SimFxShotAimS;
		}
		break;
	}

	case FState::EPhase::ShotAim:
	{
		const float DistM = AimVariant(S.Variant);
		if (S.Clock >= S.PhaseAt)
		{
			const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
			const FSimFxShotVariant& V = Sub.Variants[S.Variant];
			const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("shot_%s%s_%.1fs%s.png"), *Sub.Name, V.Suffix, Sub.Delays[S.Shot],
				bNight ? TEXT("_night") : TEXT("")));
			FScreenshotRequest::RequestScreenshot(File, false, false);
			++S.Taken;
			UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s (%.2f s after its events, camera %.0f m from it, fov %.0f; %d sprites, %d ribbons, %d lights; frame %.1f ms, effects %.2f ms)"),
				*FPaths::GetCleanFilename(File), RenderT - S.T0, DistM, V.FovDeg, Fx.GetSpriteCount(), Fx.GetRibbonCount(), Fx.GetLightCount(),
				RealDt * 1000.0f, Fx.GetLastFrameMs());
			S.Phase = FState::EPhase::ShotAfter;
			S.PhaseAt = S.Clock + SimFxShotAfterS;
		}
		break;
	}

	case FState::EPhase::ShotAfter:
	{
		if (S.Clock < S.PhaseAt)
		{
			break;
		}
		const FSimFxShotSubject& Sub = S.Subjects[S.Subject];
		++S.Variant;
		if (S.Variant < Sub.Variants.Num())
		{
			AimVariant(S.Variant);
			S.Phase = FState::EPhase::ShotAim;
			S.PhaseAt = S.Clock + SimFxShotAimS;
			break;
		}
		if (S.bPausedByUs)
		{
			Sim->SetSimPaused(false);
			S.bPausedByUs = false;
		}
		++S.Shot;
		if (S.Shot < Sub.Delays.Num())
		{
			AimVariant(0);
			S.Phase = FState::EPhase::Running;
		}
		else
		{
			if (S.CostFrames > 0)
			{
				UE_LOG(LogSAM3D, Log, TEXT("SimFxShots: %s: running, %d frames: the effects %.2f ms a frame (peak %.2f; at most %d sprites, %d ribbons, %d lights), the sound %.2f ms (peak %.2f), the frame %.1f ms (peak %.1f)"),
					*Sub.Name, S.CostFrames, S.SumFxMs / S.CostFrames, S.PeakFxMs, S.PeakSprites, S.PeakRibbons, S.PeakLights,
					S.SumAudioMs / S.CostFrames, S.PeakAudioMs, S.SumFrameMs / S.CostFrames, S.PeakFrameMs);
			}
			S.SumFxMs = S.PeakFxMs = S.SumFrameMs = S.PeakFrameMs = S.SumAudioMs = S.PeakAudioMs = 0.0;
			S.PeakSprites = S.PeakRibbons = S.PeakLights = 0;
			S.CostFrames = 0;
			S.Phase = FState::EPhase::Gap;
			S.PhaseAt = S.Clock + SimFxShotGapS;
		}
		break;
	}

	case FState::EPhase::Gap:
		if (S.Clock >= S.PhaseAt)
		{
			++S.Subject;
			S.Phase = FState::EPhase::Setup;
		}
		break;

	case FState::EPhase::Finished:
		break;
	}
}
