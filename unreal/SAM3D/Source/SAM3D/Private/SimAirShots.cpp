#include "SimAirShots.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimFxImpl.h"
#include "SimFxSubsystem.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"

#include <vector>

// The air battle's photographer (roadmap B13): it only watches. Each subject is the first time the simulation does it
// (an fx event of its kind, or an entity in that state), photographed at its delays from a camera close to it.

namespace
{
	enum class ESimAirShotKind : uint8
	{
		Banked,      // an aircraft banked 30 deg or more (its attitude from simcore's bank, BuildEntityList)
		Formation,   // an aircraft with another within 1.5 km: a flight's members in their slots
		OurFighter,  // one of our fighters in the air (drawn with our model, never the enemy's)
		Flares,      // a FlareDrop: the flares falling burning, their smoke
		Chaff,       // a ChaffDrop: the cloud blooming behind it
		Jettison,    // a WeaponRelease "jettison": stores or tanks tumbling down
		AamLaunch,   // an AamLaunch: the missile off its rail, its motor and smoke
		AamEnd,      // an AamEnd that met its target or passed it: the burst
		AirGuns,     // an AirGunBurst: tracers from the shooter at its target
	};

	struct FSimAirShotSubject
	{
		ESimAirShotKind Kind = ESimAirShotKind::Banked;
		FString Name;
		TArray<double> Delays;  // render seconds after it happened
		bool bTaken = false;
	};

	constexpr double SimAirShotRealLimitS = 1800.0;  // wall clock, the whole run
	constexpr double SimAirShotWaitRunnerS = 240.0;
	constexpr double SimAirShotAimS = 0.5;           // a picture's settle (temporal AA, auto exposure)
	constexpr double SimAirShotAfterS = 0.35;
	constexpr double SimAirShotBankDeg = 30.0;
	constexpr double SimAirShotFormationCm = 150000.0;

	bool SimAirShotIsAircraft(const FSimEntityState& S)
	{
		return S.bAlive && !S.bIsMissile && !S.bIsGround && !S.bIsWeapon && S.Id >= 0 && S.Id < SimFxIds::MissileBase;
	}

	FVector SimAirShotFwd(const FVector& V)
	{
		const FVector F = V.GetSafeNormal();
		return F.IsNearlyZero() ? FVector::ForwardVector : F;
	}

	FVector SimAirShotAcross(const FVector& Fwd)
	{
		return FVector::CrossProduct(Fwd, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	}
}

struct FSimAirShots::FState
{
	enum class EPhase : uint8
	{
		WaitRunner,
		Idle,       // watching for the next subject
		Running,    // a subject happened; waiting for its next delay
		ShotAim,    // paused, the camera placed
		ShotAfter,  // its screenshot asked for
		Finished,
	};

	EPhase Phase = EPhase::WaitRunner;
	double Clock = 0.0;
	double PhaseAt = 0.0;
	double MaxSimS = 900.0;
	TArray<FSimAirShotSubject> Subjects;
	const void* RunnerSeen = nullptr;
	uint64 Cursor = 0;

	// The subject being photographed.
	int32 Subject = -1;
	int32 Shot = 0;
	double T0 = 0.0;
	int32 EntityId = -1;
	int32 TargetId = -1;
	FString Source;
	int32 Number = 0;
	simcore::Vec3 EventPos;
	simcore::Vec3 EventVel;
	simcore::Vec3 EventAim;
	FVector LastAtU = FVector::ZeroVector;
	FVector LastFwdU = FVector::ForwardVector;
	bool bPausedByUs = false;
	int32 Taken = 0;
};

namespace
{
	using FSimAirShotState = FSimAirShots::FState;

	void SimAirShotList(FSimAirShotState& S)
	{
		auto Add = [&S](ESimAirShotKind Kind, const TCHAR* Name, TArray<double> Delays)
		{
			FSimAirShotSubject& Sub = S.Subjects.AddDefaulted_GetRef();
			Sub.Kind = Kind;
			Sub.Name = Name;
			Sub.Delays = MoveTemp(Delays);
		};
		Add(ESimAirShotKind::Banked, TEXT("banked"), {0.0});
		Add(ESimAirShotKind::Formation, TEXT("formation"), {0.0});
		Add(ESimAirShotKind::OurFighter, TEXT("our_fighter"), {0.0});
		Add(ESimAirShotKind::Flares, TEXT("flares"), {0.5, 2.0});
		Add(ESimAirShotKind::Chaff, TEXT("chaff"), {0.6, 2.5});
		Add(ESimAirShotKind::Jettison, TEXT("jettison"), {0.4, 1.5});
		Add(ESimAirShotKind::AamLaunch, TEXT("aam_launch"), {0.3, 1.2});
		Add(ESimAirShotKind::AamEnd, TEXT("aam_end"), {0.05, 0.8});
		Add(ESimAirShotKind::AirGuns, TEXT("air_guns"), {0.2, 0.8});
	}

	/** The subject an fx event is, or -1 (taken already, or nothing of the air battle). */
	int32 SimAirShotForEvent(const FSimAirShotState& S, const simcore::fx::Event& E)
	{
		using simcore::fx::EventKind;
		ESimAirShotKind Kind = ESimAirShotKind::Banked;
		switch (E.kind)
		{
		case EventKind::FlareDrop: Kind = ESimAirShotKind::Flares; break;
		case EventKind::ChaffDrop: Kind = ESimAirShotKind::Chaff; break;
		case EventKind::AamLaunch: Kind = ESimAirShotKind::AamLaunch; break;
		case EventKind::AirGunBurst: Kind = ESimAirShotKind::AirGuns; break;
		case EventKind::AamEnd:
			if (E.end == simcore::fx::BurstEnd::Early)
			{
				return -1;
			}
			Kind = ESimAirShotKind::AamEnd;
			break;
		case EventKind::WeaponRelease:
			if (E.delivery != "jettison")
			{
				return -1;
			}
			Kind = ESimAirShotKind::Jettison;
			break;
		default:
			return -1;
		}
		for (int32 I = 0; I < S.Subjects.Num(); ++I)
		{
			if (S.Subjects[I].Kind == Kind && !S.Subjects[I].bTaken)
			{
				return I;
			}
		}
		return -1;
	}

	/** An aircraft in the state a polled subject wants, or -1. */
	int32 SimAirShotFindEntity(const USimWorldSubsystem& Sim, ESimAirShotKind Kind)
	{
		const TArray<FSimEntityState>& All = Sim.GetEntityStatesCached();
		for (const FSimEntityState& A : All)
		{
			if (!SimAirShotIsAircraft(A))
			{
				continue;
			}
			switch (Kind)
			{
			case ESimAirShotKind::Banked:
				if (FMath::Abs(A.Rotation.Roll) >= SimAirShotBankDeg)
				{
					return A.Id;
				}
				break;
			case ESimAirShotKind::OurFighter:
				if (A.bFriendly)
				{
					return A.Id;
				}
				break;
			case ESimAirShotKind::Formation:
				for (const FSimEntityState& B : All)
				{
					if (B.Id != A.Id && SimAirShotIsAircraft(B) && FVector::Dist(A.Location, B.Location) < SimAirShotFormationCm)
					{
						return A.Id;
					}
				}
				break;
			default:
				break;
			}
		}
		return -1;
	}

	/** Where the subject is now, its way, and the camera's distance from it (Unreal cm). */
	void SimAirShotPose(FSimAirShotState& S, const FSimAirShotSubject& Sub, const USimWorldSubsystem& Sim, const USimFxSubsystem& Fx,
		FVector& OutFrom, FVector& OutAt)
	{
		const FSimFxWorld* W = Fx.GetFxWorld();
		FVector P = S.LastAtU;
		FVector Fwd = S.LastFwdU;
		if (const FSimEntityState* A = Sim.FindEntityStateCached(S.EntityId))
		{
			if (A->bAlive)
			{
				P = Sim.GetRenderLocation(*A);
				Fwd = SimAirShotFwd(A->Velocity);
			}
		}
		if (P.IsZero())
		{
			P = Sim.ToUnreal(S.EventPos);
			Fwd = SimAirShotFwd(Sim.ToUnreal(S.EventPos + S.EventVel) - P);
		}
		const FVector Up = FVector::UpVector;
		FVector Across = SimAirShotAcross(Fwd);
		FVector At = P;
		double SideCm = 2500.0, BackCm = 6000.0, UpCm = 800.0;
		switch (Sub.Kind)
		{
		case ESimAirShotKind::Banked:
			break;
		case ESimAirShotKind::OurFighter:
			SideCm = 4000.0;
			BackCm = 1500.0;
			UpCm = 500.0;
			break;
		case ESimAirShotKind::Formation:
		{
			// The flight round it: its middle, from far enough back and above to have them all.
			FVector Sum = P;
			double Spread = 0.0;
			int32 N = 1;
			for (const FSimEntityState& B : Sim.GetEntityStatesCached())
			{
				if (B.Id != S.EntityId && SimAirShotIsAircraft(B) && FVector::Dist(B.Location, P) < SimAirShotFormationCm)
				{
					const FVector BP = Sim.GetRenderLocation(B);
					Sum += BP;
					Spread = FMath::Max(Spread, FVector::Dist(BP, P));
					++N;
				}
			}
			At = Sum / N;
			const double D = FMath::Max(Spread * 1.3, 15000.0);
			BackCm = D;
			SideCm = 0.4 * D;
			UpCm = 0.35 * D;
			break;
		}
		case ESimAirShotKind::Flares:
		{
			// The salvo's flares (their mean), from the side.
			simcore::Vec3 Sum;
			int32 N = 0;
			if (W != nullptr)
			{
				for (const FSimFxFlare& F : W->Flares)
				{
					if (F.bLive && F.SourceId == S.EntityId)
					{
						Sum += F.Pos;
						++N;
					}
				}
			}
			if (N > 0)
			{
				At = Sim.ToUnreal(Sum / static_cast<double>(N));
			}
			SideCm = 12000.0;
			BackCm = 3000.0;
			UpCm = 1500.0;
			break;
		}
		case ESimAirShotKind::Chaff:
			// The cloud stays where it was dropped, drifting.
			At = Sim.ToUnreal(S.EventPos);
			Fwd = SimAirShotFwd(Sim.ToUnreal(S.EventPos + S.EventVel) - At);
			Across = SimAirShotAcross(Fwd);
			SideCm = 15000.0;
			BackCm = 5000.0;
			UpCm = 2000.0;
			break;
		case ESimAirShotKind::Jettison:
		{
			// The thrown stores (their mean) tumbling under it.
			simcore::Vec3 Sum;
			int32 N = 0;
			if (W != nullptr)
			{
				for (const FSimFxBomb& B : W->Bombs)
				{
					if (B.bInert && B.Source == S.Source && B.bReleased && !B.bDone && !B.Path.Done(W->Now))
					{
						Sum += B.Path.Position(W->Now);
						++N;
					}
				}
			}
			if (N > 0)
			{
				At = Sim.ToUnreal(Sum / static_cast<double>(N));
			}
			SideCm = 6000.0;
			BackCm = 1500.0;
			UpCm = 600.0;
			break;
		}
		case ESimAirShotKind::AamLaunch:
		{
			// The missile itself, from behind and beside it.
			if (W != nullptr)
			{
				for (const FSimFxMissile& M : W->Missiles)
				{
					if (M.bAam && !M.bDone && M.Number == S.Number && M.Source == S.Source && !M.PosU.IsZero())
					{
						At = M.PosU;
						Fwd = SimAirShotFwd(Sim.ToUnreal(M.Pos + M.Vel) - Sim.ToUnreal(M.Pos));
						Across = SimAirShotAcross(Fwd);
						break;
					}
				}
			}
			SideCm = 1500.0;
			BackCm = 2500.0;
			UpCm = 500.0;
			break;
		}
		case ESimAirShotKind::AamEnd:
			// At its target, or where it ended.
			At = P;
			if (const FSimEntityState* T = Sim.FindEntityStateCached(S.TargetId))
			{
				At = Sim.GetRenderLocation(*T);
				Fwd = SimAirShotFwd(T->Velocity);
				Across = SimAirShotAcross(Fwd);
			}
			else if (S.EventAim.LengthSquared() > 0.0)
			{
				At = Sim.ToUnreal(S.EventAim);
			}
			SideCm = 15000.0;
			BackCm = 3000.0;
			UpCm = 3000.0;
			break;
		case ESimAirShotKind::AirGuns:
			// Behind the shooter, looking down its line of fire.
			if (const FSimEntityState* T = Sim.FindEntityStateCached(S.TargetId))
			{
				At = FMath::Lerp(P, Sim.GetRenderLocation(*T), 0.5);
			}
			else
			{
				At = P + Fwd * 30000.0;
			}
			SideCm = 1200.0;
			BackCm = 3000.0;
			UpCm = 800.0;
			OutFrom = P - Fwd * BackCm + Across * SideCm + Up * UpCm;
			OutAt = At;
			S.LastAtU = P;
			S.LastFwdU = Fwd;
			return;
		}
		S.LastAtU = Sub.Kind == ESimAirShotKind::AamEnd || Sub.Kind == ESimAirShotKind::Chaff ? S.LastAtU : P;
		S.LastFwdU = Fwd;
		OutFrom = At - Fwd * BackCm + Across * SideCm + Up * UpCm;
		OutAt = At;
	}
}

FSimAirShots::FSimAirShots(UWorld* InWorld, const FString& InFolder)
	: World(InWorld)
	, Folder(FPaths::ConvertRelativePathToFull(InFolder))
	, State(MakeUnique<FState>())
{
	IFileManager::Get().MakeDirectory(*Folder, true);
	SimAirShotList(*State);
	double MaxS = 0.0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-SimAirShotsMaxS="), MaxS) && MaxS > 0.0)
	{
		State->MaxSimS = MaxS;
	}
	UE_LOG(LogSAM3D, Log, TEXT("SimAirShots: %d subjects into %s within %.0f s of the scenario"), State->Subjects.Num(), *Folder, State->MaxSimS);
}

FSimAirShots::~FSimAirShots() = default;

bool FSimAirShots::IsDone() const
{
	return State->Phase == FState::EPhase::Finished;
}

void FSimAirShots::Tick(USimFxSubsystem& Fx, float RealDt)
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
		FString Missing;
		for (const FSimAirShotSubject& Sub : S.Subjects)
		{
			if (!Sub.bTaken)
			{
				Missing += (Missing.IsEmpty() ? TEXT("") : TEXT(", ")) + Sub.Name;
			}
		}
		UE_LOG(LogSAM3D, Log, TEXT("SimAirShots: done (%s): %d pictures; not seen: %s"), Why, S.Taken, Missing.IsEmpty() ? TEXT("none") : *Missing);
		if (S.bPausedByUs && Sim != nullptr)
		{
			Sim->SetSimPaused(false);
			S.bPausedByUs = false;
		}
		S.Phase = FState::EPhase::Finished;
		FGenericPlatformMisc::RequestExit(false);
	};

	if (S.Clock > SimAirShotRealLimitS)
	{
		Finish(TEXT("the time limit"));
		return;
	}
	simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetFxRunner() : nullptr;
	if (Runner == nullptr)
	{
		if (S.Phase != FState::EPhase::WaitRunner)
		{
			Finish(TEXT("the scenario ended"));
		}
		else if (S.Clock > SimAirShotWaitRunnerS)
		{
			Finish(TEXT("no scenario ran: start one with -SimScenario=<file>"));
		}
		return;
	}
	if (S.RunnerSeen != Runner)
	{
		S.RunnerSeen = Runner;
		S.Cursor = Runner->Fx().Last();
		if (S.Phase != FState::EPhase::WaitRunner && S.Phase != FState::EPhase::Idle)
		{
			UE_LOG(LogSAM3D, Log, TEXT("SimAirShots: a new scenario runner; the subject in hand dropped"));
			if (S.bPausedByUs)
			{
				Sim->SetSimPaused(false);
				S.bPausedByUs = false;
			}
		}
		S.Phase = FState::EPhase::Idle;
		S.Subject = -1;
	}
	const double RunnerT = Runner->Time();
	const double RenderT = Sim->GetRenderTimeSeconds();

	auto Aim = [this, W](const FVector& From, const FVector& At)
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
		Cam->GetCameraComponent()->SetFieldOfView(50.0f);
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (PC->GetViewTarget() != Cam)
			{
				PC->SetViewTargetWithBlend(Cam, 0.0f);
			}
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
	auto AimSubject = [&]() -> double
	{
		FVector From, At;
		SimAirShotPose(S, S.Subjects[S.Subject], *Sim, Fx, From, At);
		Aim(From, At);
		return FVector::Dist(From, At) / 100.0;
	};

	// The feed since last frame: the first of each subject's kind starts it (only while nothing else is in hand).
	const simcore::fx::Feed& Feed = Runner->Fx();
	const std::vector<simcore::fx::Event> New = Feed.Since(S.Cursor);
	S.Cursor = Feed.Last();

	switch (S.Phase)
	{
	case FState::EPhase::WaitRunner:
		break;

	case FState::EPhase::Idle:
	{
		bool bAllTaken = true;
		for (const FSimAirShotSubject& Sub : S.Subjects)
		{
			bAllTaken = bAllTaken && Sub.bTaken;
		}
		if (bAllTaken)
		{
			Finish(TEXT("every subject"));
			return;
		}
		if (RunnerT > S.MaxSimS)
		{
			Finish(TEXT("the scenario's time for it"));
			return;
		}
		int32 Found = -1;
		for (const simcore::fx::Event& E : New)
		{
			const int32 I = SimAirShotForEvent(S, E);
			if (I < 0)
			{
				continue;
			}
			Found = I;
			S.T0 = E.t_s;
			S.EntityId = E.world_id;
			S.TargetId = E.target_world_id;
			S.Source = UTF8_TO_TCHAR(E.source.c_str());
			S.Number = E.number;
			S.EventPos = E.position;
			S.EventVel = E.velocity;
			S.EventAim = E.aim;
			break;
		}
		if (Found < 0)
		{
			for (int32 I = 0; I < S.Subjects.Num() && Found < 0; ++I)
			{
				const FSimAirShotSubject& Sub = S.Subjects[I];
				if (Sub.bTaken || (Sub.Kind != ESimAirShotKind::Banked && Sub.Kind != ESimAirShotKind::Formation && Sub.Kind != ESimAirShotKind::OurFighter))
				{
					continue;
				}
				const int32 Id = SimAirShotFindEntity(*Sim, Sub.Kind);
				if (Id >= 0)
				{
					Found = I;
					S.T0 = RenderT;
					S.EntityId = Id;
					S.TargetId = -1;
					S.Source.Reset();
					S.Number = 0;
					S.EventPos = S.EventVel = S.EventAim = simcore::Vec3();
				}
			}
		}
		if (Found >= 0)
		{
			S.Subject = Found;
			S.Shot = 0;
			S.LastAtU = FVector::ZeroVector;
			UE_LOG(LogSAM3D, Log, TEXT("SimAirShots: %s at %.2f s (entity %d, %s)"), *S.Subjects[Found].Name, S.T0, S.EntityId,
				S.Source.IsEmpty() ? TEXT("-") : *S.Source);
			AimSubject();
			S.Phase = FState::EPhase::Running;
		}
		break;
	}

	case FState::EPhase::Running:
	{
		AimSubject();
		const FSimAirShotSubject& Sub = S.Subjects[S.Subject];
		if (RenderT >= S.T0 + Sub.Delays[S.Shot])
		{
			if (!Sim->IsSimPaused())
			{
				Sim->SetSimPaused(true);
				S.bPausedByUs = true;
			}
			AimSubject();
			S.Phase = FState::EPhase::ShotAim;
			S.PhaseAt = S.Clock + SimAirShotAimS;
		}
		break;
	}

	case FState::EPhase::ShotAim:
	{
		const double DistM = AimSubject();
		if (S.Clock >= S.PhaseAt)
		{
			const FSimAirShotSubject& Sub = S.Subjects[S.Subject];
			const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("shot_air_%s_%.2fs.png"), *Sub.Name, Sub.Delays[S.Shot]));
			FScreenshotRequest::RequestScreenshot(File, false, false);
			++S.Taken;
			FString Who = TEXT("-");
			if (const FSimEntityState* A = Sim->FindEntityStateCached(S.EntityId))
			{
				Who = FString::Printf(TEXT("%s, model %s, %s, bank %.0f deg"), *A->Name, *A->Model, A->bFriendly ? TEXT("ours") : TEXT("theirs"),
					A->Rotation.Roll);
			}
			UE_LOG(LogSAM3D, Log, TEXT("SimAirShots: %s (%.2f s after, camera %.0f m off; %s)"), *FPaths::GetCleanFilename(File), RenderT - S.T0, DistM, *Who);
			S.Phase = FState::EPhase::ShotAfter;
			S.PhaseAt = S.Clock + SimAirShotAfterS;
		}
		break;
	}

	case FState::EPhase::ShotAfter:
	{
		if (S.Clock < S.PhaseAt)
		{
			break;
		}
		if (S.bPausedByUs)
		{
			Sim->SetSimPaused(false);
			S.bPausedByUs = false;
		}
		FSimAirShotSubject& Sub = S.Subjects[S.Subject];
		++S.Shot;
		if (S.Shot < Sub.Delays.Num())
		{
			S.Phase = FState::EPhase::Running;
		}
		else
		{
			Sub.bTaken = true;
			S.Subject = -1;
			S.Phase = FState::EPhase::Idle;
		}
		break;
	}

	case FState::EPhase::Finished:
		break;
	}
}
