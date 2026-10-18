#include "SimViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "SAM3D.h"
#include "SimOsaVehicle.h"
#include "SimWorldSubsystem.h"

namespace
{
TAutoConsoleVariable<int32> CVarSimViews(TEXT("sam3d.Views"), 2,
	TEXT("The camera views allowed (F1-F11): 2 every view, 1 our own side (the cabin, our vehicles, our missiles, the map), ")
	TEXT("0 the cabin and our own vehicle only."),
	ECVF_Default);

constexpr double MinStandOffCm = 300.0;

FVector Up() { return FVector::UpVector; }
}

// ---------------------------------------------------------------------------------------------- the camera

ASimViewCamera::ASimViewCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->SetFieldOfView(70.0f);
	Camera->bConstrainAspectRatio = false;
}

// ---------------------------------------------------------------------------------------------- the views

USimViewComponent::USimViewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;  // the vehicle calls UpdateViews
}

FString USimViewComponent::KeyOf(ESimView InView)
{
	switch (InView)
	{
	case ESimView::Cabin: return TEXT("F1");
	case ESimView::Vehicle: return TEXT("F2");
	case ESimView::FlyBy: return TEXT("F3");
	case ESimView::Chase: return TEXT("F4");
	case ESimView::NearestAir: return TEXT("F5");
	case ESimView::Weapon: return TEXT("F6");
	case ESimView::Ground: return TEXT("F7");
	case ESimView::Air: return TEXT("F8");
	case ESimView::Map: return TEXT("F10");
	case ESimView::Free: return TEXT("F11");
	}
	return FString();
}

FString USimViewComponent::NameOf(ESimView InView)
{
	switch (InView)
	{
	case ESimView::Cabin: return TEXT("Cabin");
	case ESimView::Vehicle: return TEXT("Your vehicle");
	case ESimView::FlyBy: return TEXT("Fly-by");
	case ESimView::Chase: return TEXT("Chase");
	case ESimView::NearestAir: return TEXT("Nearest aircraft from your vehicle");
	case ESimView::Weapon: return TEXT("Your missile");
	case ESimView::Ground: return TEXT("Ground units");
	case ESimView::Air: return TEXT("Aircraft");
	case ESimView::Map: return TEXT("Map");
	case ESimView::Free: return TEXT("Free camera");
	}
	return FString();
}

ASimOsaVehicle* USimViewComponent::Vehicle() const
{
	return Cast<ASimOsaVehicle>(GetOwner());
}

USimWorldSubsystem* USimViewComponent::Sim() const
{
	return GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
}

bool USimViewComponent::IsAllowed(ESimView InView) const
{
	const int32 Level = CVarSimViews.GetValueOnGameThread();
	if (Level >= 2)
	{
		return true;
	}
	if (InView == ESimView::Cabin || InView == ESimView::Vehicle)
	{
		return true;
	}
	return Level == 1 && (InView == ESimView::FlyBy || InView == ESimView::Chase || InView == ESimView::Weapon ||
							 InView == ESimView::Ground || InView == ESimView::Map);
}

bool USimViewComponent::StateOf(const FSubject& S, FSubjectState& Out) const
{
	const ASimOsaVehicle* V = Vehicle();
	if (S.bOwn)
	{
		if (V == nullptr)
		{
			return false;
		}
		// The middle of the vehicle, a little above the hull.
		Out.Location = V->GetActorLocation() + V->GetActorUpVector() * 250.0;
		Out.Velocity = V->GetActorForwardVector() * V->GetSpeedMps() * 100.0;
		Out.Rotation = V->GetActorRotation();
		Out.Name = V->IsCommandPost() ? TEXT("your PU-12M") : TEXT("your Osa");
		Out.bAlive = true;
		Out.SizeM = 9.0;
		return true;
	}
	const USimWorldSubsystem* W = Sim();
	FSimEntityState E;
	if (W == nullptr || !W->GetEntityState(S.EntityId, E))
	{
		return false;
	}
	Out.Location = E.Location;
	Out.Velocity = E.Velocity;
	Out.Rotation = E.Rotation;
	Out.Name = E.Name;
	Out.bAlive = E.bAlive;
	// The basic models are built to their real size (docs/scenery/): stand off by roughly that.
	const FString Model = E.Model.ToLower();
	Out.SizeM = E.bIsMissile ? 3.0 : E.bIsGround ? 9.0 : (Model.Contains(TEXT("drone")) || Model.Contains(TEXT("bqm"))) ? 5.0 : 16.0;
	return true;
}

TArray<USimViewComponent::FSubject> USimViewComponent::CandidatesFor(ESimView InView) const
{
	TArray<FSubject> Out;
	const USimWorldSubsystem* W = Sim();
	const ASimOsaVehicle* V = Vehicle();
	if (W == nullptr || V == nullptr)
	{
		return Out;
	}
	if (InView == ESimView::Ground)
	{
		Out.Add(FSubject());  // our own first
	}
	TArray<FSimEntityState> States = W->GetEntityStates();
	States.Sort([](const FSimEntityState& A, const FSimEntityState& B) { return A.Id < B.Id; });
	const FVector From = V->GetActorLocation();
	if (InView == ESimView::NearestAir)
	{
		States.Sort([&From](const FSimEntityState& A, const FSimEntityState& B)
			{ return FVector::DistSquared(A.Location, From) < FVector::DistSquared(B.Location, From); });
	}
	for (const FSimEntityState& E : States)
	{
		if (!E.bAlive)
		{
			continue;
		}
		const bool bAir = !E.bIsGround && !E.bIsMissile;
		const bool bWant = (InView == ESimView::Ground && E.bIsGround) || (InView == ESimView::Weapon && E.bIsMissile) ||
						   ((InView == ESimView::Air || InView == ESimView::NearestAir) && bAir);
		if (bWant)
		{
			FSubject S;
			S.bOwn = false;
			S.EntityId = E.Id;
			Out.Add(S);
		}
	}
	return Out;
}

void USimViewComponent::Note(const FString& Text)
{
	Message = Text;
	MessageSeconds = 3.0f;
}

void USimViewComponent::SetView(ESimView NewView)
{
	if (View != NewView)
	{
		Press(NewView);
	}
}

void USimViewComponent::Press(ESimView InView)
{
	ASimOsaVehicle* V = Vehicle();
	if (V == nullptr)
	{
		return;
	}
	if (!IsAllowed(InView))
	{
		Note(FString::Printf(TEXT("%s: not allowed in this game (sam3d.Views)"), *NameOf(InView)));
		return;
	}
	const bool bRepeat = View == InView;
	if (InView == ESimView::Cabin)
	{
		// F1 again: the next seat, as Tab.
		if (V->IsInCabin())
		{
			V->NextStation();
		}
		else if (V->SetInCabin(true))
		{
			View = ESimView::Cabin;
		}
		else
		{
			Note(TEXT("This vehicle has no cabin loaded"));
		}
		return;
	}
	// The views that look for something of their own kind: none, and the view stays as it was.
	if (InView == ESimView::NearestAir || InView == ESimView::Weapon || InView == ESimView::Ground || InView == ESimView::Air)
	{
		const TArray<FSubject> List = CandidatesFor(InView);
		if (List.Num() == 0)
		{
			Note(InView == ESimView::Weapon ? TEXT("No missile of yours in flight") : InView == ESimView::Ground ? TEXT("No ground units")
																			   : TEXT("No aircraft in the sky yet"));
			return;
		}
		const int32 At = bRepeat ? List.IndexOfByKey(Subject) : INDEX_NONE;
		// First press: the nearest (F5), the newest missile (F6), the first of the list; again: the next.
		const int32 Pick = At != INDEX_NONE ? (At + 1) % List.Num() : (InView == ESimView::Weapon ? List.Num() - 1 : 0);
		Subject = List[Pick];
		if (!bRepeat)
		{
			OrbitScale = InView == ESimView::Weapon ? 4.0 : 3.0;
			PadlockFill = 1.0;
		}
	}
	else if (InView == ESimView::Vehicle)
	{
		if (bRepeat)
		{
			OrbitScale = OrbitScale > 2.0 ? 1.5 : 3.0;  // closer, further
		}
		else
		{
			OrbitScale = 3.0;
		}
		Subject = FSubject();
	}
	else if (InView == ESimView::Chase && bRepeat)
	{
		bChaseAhead = !bChaseAhead;
	}
	else if (InView == ESimView::Map && bRepeat)
	{
		MapPan = FVector::ZeroVector;
		MapHeightM = 1500.0;
	}
	if (V->IsInCabin())
	{
		V->SetInCabin(false);
	}
	// What the camera follows is lost: back to our own vehicle for the views that follow the current one.
	FSubjectState S;
	if ((InView == ESimView::FlyBy || InView == ESimView::Chase || InView == ESimView::Map) && (!StateOf(Subject, S) || !S.bAlive))
	{
		Subject = FSubject();
	}
	if (InView == ESimView::Chase && !bRepeat)
	{
		bChaseAhead = false;
	}
	if (InView == ESimView::Free && !bRepeat)
	{
		bFreePlaced = false;
	}
	View = InView;
	bFlyByPlaced = false;
	bSubjectGone = false;
	bSnap = true;
}

void USimViewComponent::EnsureCamera()
{
	if (ViewCamera != nullptr || GetWorld() == nullptr)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ViewCamera = GetWorld()->SpawnActor<ASimViewCamera>(ASimViewCamera::StaticClass(), FTransform::Identity, Params);
}

void USimViewComponent::Aim(const FVector& Location, const FVector& Target, float FovDeg)
{
	if (ViewCamera == nullptr)
	{
		return;
	}
	const FVector Look = Target - Location;
	ViewCamera->SetActorLocationAndRotation(Location, Look.IsNearlyZero() ? ViewCamera->GetActorRotation() : Look.Rotation());
	ViewCamera->GetCamera()->SetFieldOfView(FovDeg);
}

void USimViewComponent::UpdateViews(APlayerController* Pc, float DeltaSeconds)
{
	ASimOsaVehicle* V = Vehicle();
	if (Pc == nullptr || V == nullptr || !Pc->IsLocalController())
	{
		return;
	}
	MessageSeconds = FMath::Max(0.0f, MessageSeconds - DeltaSeconds);
	if (LastReportedView.IsEmpty())
	{
		OrbitYawDeg = V->GetActorRotation().Yaw + 155.0;  // first look: from behind and to the side
	}
	// C (the vehicle's own key) or the menu's seat may have moved the camera into the cabin or out of it.
	if (V->IsInCabin() && View != ESimView::Cabin)
	{
		View = ESimView::Cabin;
	}
	else if (!V->IsInCabin() && View == ESimView::Cabin)
	{
		View = ESimView::Vehicle;
		Subject = FSubject();
		bSnap = true;
	}
	static const struct
	{
		FKey Key;
		ESimView Mode;
	} Keys[] = {{EKeys::F1, ESimView::Cabin}, {EKeys::F2, ESimView::Vehicle}, {EKeys::F3, ESimView::FlyBy}, {EKeys::F4, ESimView::Chase},
		{EKeys::F5, ESimView::NearestAir}, {EKeys::F6, ESimView::Weapon}, {EKeys::F7, ESimView::Ground}, {EKeys::F8, ESimView::Air},
		{EKeys::F10, ESimView::Map}, {EKeys::F11, ESimView::Free}};
	for (const auto& K : Keys)
	{
		if (Pc->WasInputKeyJustPressed(K.Key))
		{
			Press(K.Mode);
		}
	}
	RunViewShots(DeltaSeconds);
	HandleTimeKeys(Pc);
	PlaceCamera(Pc, DeltaSeconds);
	ReportToTutorial();
}

void USimViewComponent::RunViewShots(float DeltaSeconds)
{
	if (!bShotsAsked)
	{
		bShotsAsked = true;
		if (FParse::Value(FCommandLine::Get(), TEXT("SimViewShots="), ShotFolder) && !ShotFolder.IsEmpty())
		{
			float Delay = 15.0f;
			FParse::Value(FCommandLine::Get(), TEXT("SimShotDelay="), Delay);
			ShotStep = 0;
			ShotClock = -Delay;
			UE_LOG(LogSAM3D, Log, TEXT("View shots: into %s after %.0f s"), *ShotFolder, Delay);
		}
	}
	if (ShotStep == INDEX_NONE)
	{
		return;
	}
	// Each view as a player meets it: the vehicle, a fly-by of it, the aircraft and its chase, the nearest from the
	// vehicle, the ground units and the next, the map, the free camera, the cabin and its next seat.
	static const ESimView Sequence[] = {ESimView::Vehicle, ESimView::FlyBy, ESimView::Air, ESimView::Chase, ESimView::NearestAir,
		ESimView::Ground, ESimView::Ground, ESimView::Map, ESimView::Free, ESimView::Cabin, ESimView::Cabin};
	constexpr int32 Count = UE_ARRAY_COUNT(Sequence);
	const float Before = ShotClock;
	ShotClock += DeltaSeconds;
	if (Before < 0.0f && ShotClock >= 0.0f && ShotStep < Count)
	{
		Press(Sequence[ShotStep]);
	}
	if (Before < 2.5f && ShotClock >= 2.5f && ShotStep < Count)
	{
		const FString File = FPaths::Combine(ShotFolder, FString::Printf(TEXT("%02d_%s.png"), ShotStep + 1, *KeyOf(View)));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogSAM3D, Log, TEXT("View shot: %s  (%s)"), *File, *GetViewLabel());
	}
	if (ShotClock >= 3.5f)
	{
		++ShotStep;
		ShotClock = 0.0f;
		if (ShotStep >= Count)
		{
			UE_LOG(LogSAM3D, Log, TEXT("View shots: done"));
			FGenericPlatformMisc::RequestExit(false);
			ShotStep = INDEX_NONE;
			return;
		}
		Press(Sequence[ShotStep]);
	}
}

void USimViewComponent::HandleTimeKeys(APlayerController* Pc)
{
	USimWorldSubsystem* W = Sim();
	if (W == nullptr || !Pc->WasInputKeyJustPressed(EKeys::Z) || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}
	const bool bCtrl = Pc->IsInputKeyDown(EKeys::LeftControl) || Pc->IsInputKeyDown(EKeys::RightControl);
	const bool bAlt = Pc->IsInputKeyDown(EKeys::LeftAlt) || Pc->IsInputKeyDown(EKeys::RightAlt);
	const bool bShift = Pc->IsInputKeyDown(EKeys::LeftShift) || Pc->IsInputKeyDown(EKeys::RightShift);
	const float Was = W->GetTimeScale();
	if (bCtrl)
	{
		W->SetTimeScale(Was * 2.0f);
	}
	else if (bAlt)
	{
		W->SetTimeScale(Was * 0.5f);
	}
	else if (bShift)
	{
		W->SetTimeScale(1.0f);
	}
	else
	{
		return;
	}
	const float Now = W->GetTimeScale();
	Note(Now == 1.0f ? FString(TEXT("Time: normal")) : FString::Printf(TEXT("Time: x%g"), Now));
	if (Now > 1.0f)
	{
		W->NoteTutorialEvent(TEXT("time_faster"));
	}
	else if (Now == 1.0f && Was != 1.0f)
	{
		W->NoteTutorialEvent(TEXT("time_normal"));
	}
	UE_LOG(LogSAM3D, Log, TEXT("Time scale x%g"), Now);
}

void USimViewComponent::PlaceCamera(APlayerController* Pc, float DeltaSeconds)
{
	ASimOsaVehicle* V = Vehicle();
	if (View == ESimView::Cabin)
	{
		if (Pc->GetViewTarget() != V)
		{
			Pc->SetViewTarget(V);
		}
		return;
	}
	EnsureCamera();
	if (ViewCamera == nullptr)
	{
		return;
	}
	if (Pc->GetViewTarget() != ViewCamera)
	{
		if (View == ESimView::Free && !bFreePlaced && Pc->PlayerCameraManager != nullptr)
		{
			// The free camera starts where the last view was.
			FreeLocation = Pc->PlayerCameraManager->GetCameraLocation();
			FreeRotation = Pc->PlayerCameraManager->GetCameraRotation();
			bFreePlaced = true;
		}
		Pc->SetViewTarget(ViewCamera);
	}
	float Dx = 0.0f;
	float Dy = 0.0f;
	Pc->GetInputMouseDelta(Dx, Dy);
	const bool bLook = Pc->IsInputKeyDown(EKeys::RightMouseButton);
	const int32 Wheel = (Pc->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1 : 0) - (Pc->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	USimWorldSubsystem* W = Sim();
	if (W != nullptr && bLook && (Dx != 0.0f || Dy != 0.0f))
	{
		W->NoteTutorialEvent(TEXT("orbit"));
	}
	if (W != nullptr && Wheel != 0)
	{
		W->NoteTutorialEvent(TEXT("zoom"));
	}

	if (View == ESimView::Free)
	{
		if (!bFreePlaced)
		{
			FreeLocation = ViewCamera->GetActorLocation();
			FreeRotation = ViewCamera->GetActorRotation();
			bFreePlaced = true;
		}
		if (bLook)
		{
			FreeRotation.Yaw += Dx * OrbitDegPerMouseUnit;
			FreeRotation.Pitch = FMath::Clamp(FreeRotation.Pitch + Dy * OrbitDegPerMouseUnit, -89.0, 89.0);
		}
		if (Wheel != 0)
		{
			FreeSpeedMps = FMath::Clamp(FreeSpeedMps * (Wheel > 0 ? 1.4 : 1.0 / 1.4), 2.0, 5000.0);
			Note(FString::Printf(TEXT("Free camera: %.0f m/s"), FreeSpeedMps));
		}
		const double Fast = Pc->IsInputKeyDown(EKeys::LeftShift) || Pc->IsInputKeyDown(EKeys::RightShift) ? 5.0 : 1.0;
		const FRotationMatrix M(FreeRotation);
		FVector Move = FVector::ZeroVector;
		Move += M.GetUnitAxis(EAxis::X) * ((Pc->IsInputKeyDown(EKeys::W) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::S) ? 1.0 : 0.0));
		Move += M.GetUnitAxis(EAxis::Y) * ((Pc->IsInputKeyDown(EKeys::D) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::A) ? 1.0 : 0.0));
		Move += Up() * ((Pc->IsInputKeyDown(EKeys::E) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::Q) ? 1.0 : 0.0));
		FreeLocation += Move.GetClampedToMaxSize(1.0) * FreeSpeedMps * Fast * 100.0 * DeltaSeconds;
		ViewCamera->SetActorLocationAndRotation(FreeLocation, FreeRotation);
		ViewCamera->GetCamera()->SetFieldOfView(75.0f);
		return;
	}

	FSubjectState S;
	const bool bHave = StateOf(Subject, S);
	if (!bHave || !S.bAlive)
	{
		if (!bSubjectGone)
		{
			bSubjectGone = true;
			if (bHave)
			{
				LastSubjectLocation = S.Location;
			}
			Note(FString::Printf(TEXT("%s: gone"), bHave ? *S.Name : TEXT("It")));
		}
		// The camera stays where it was, still looking at where it last saw it.
		Aim(ViewCamera->GetActorLocation(), LastSubjectLocation, ViewCamera->GetCamera()->FieldOfView);
		return;
	}
	LastSubjectLocation = S.Location;
	const FVector Target = S.Location;
	const double SizeCm = S.SizeM * 100.0;
	const FVector Heading = S.Velocity.Size() > 100.0 ? S.Velocity.GetSafeNormal() : S.Rotation.Vector();
	auto AboveGround = [this](FVector At, double ClearanceCm)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SimViewGround), false, GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + Up() * 100000.0, At - Up() * 100000.0, ECC_Visibility, Params) &&
			At.Z < Hit.ImpactPoint.Z + ClearanceCm)
		{
			At.Z = Hit.ImpactPoint.Z + ClearanceCm;
		}
		return At;
	};
	switch (View)
	{
	case ESimView::Vehicle:
	case ESimView::Ground:
	case ESimView::Air:
	{
		if (bLook)
		{
			OrbitYawDeg += Dx * OrbitDegPerMouseUnit;
			OrbitPitchDeg = FMath::Clamp(OrbitPitchDeg + Dy * OrbitDegPerMouseUnit, -85.0, 40.0);
		}
		if (Wheel != 0)
		{
			OrbitScale = FMath::Clamp(OrbitScale * (Wheel > 0 ? 1.0 / 1.2 : 1.2), 0.6, 400.0);
		}
		const FVector Dir = FRotator(OrbitPitchDeg, OrbitYawDeg, 0.0).Vector();
		const FVector At = AboveGround(Target - Dir * FMath::Max(MinStandOffCm, SizeCm * OrbitScale), 150.0);
		Aim(At, Target, 70.0f);
		break;
	}
	case ESimView::Chase:
	case ESimView::Weapon:
	{
		if (Wheel != 0)
		{
			OrbitScale = FMath::Clamp(OrbitScale * (Wheel > 0 ? 1.0 / 1.2 : 1.2), 0.8, 100.0);
		}
		const double Dist = FMath::Max(MinStandOffCm, SizeCm * OrbitScale);
		// The offset from it is smoothed, not the place: a missile at 800 m/s leaves no lagging camera behind.
		const FVector Want = Heading * (bChaseAhead && View == ESimView::Chase ? Dist : -Dist) + Up() * Dist * 0.25;
		Smoothed = bSnap ? Want : FMath::Lerp(Smoothed, Want, 1.0 - FMath::Exp(-6.0 * DeltaSeconds));
		bSnap = false;
		Aim(AboveGround(Target + Smoothed, 150.0), Target, 70.0f);
		break;
	}
	case ESimView::FlyBy:
	{
		const double Speed = S.Velocity.Size();
		const FVector Side = FVector::CrossProduct(Heading, Up()).GetSafeNormal();
		const bool bPast = bFlyByPlaced && Speed > 100.0 && FVector::DotProduct(Target - FlyByPoint, Heading) > 0.0 &&
						   FVector::Dist(Target, FlyByPoint) > FMath::Max(80000.0, Speed * 4.0);
		if (!bFlyByPlaced || bPast)
		{
			// Ahead along its way, off to one side and a little above: it comes to the camera and goes past.
			const double Ahead = Speed > 100.0 ? FMath::Max(3000.0, Speed * 3.0) : 0.0;
			FlyByPoint = AboveGround(Target + Heading * Ahead + Side * FMath::Max(1500.0, SizeCm * 2.5) + Up() * 300.0, 200.0);
			bFlyByPlaced = true;
		}
		Aim(FlyByPoint, Target, 60.0f);
		break;
	}
	case ESimView::NearestAir:
	{
		if (Wheel != 0)
		{
			PadlockFill = FMath::Clamp(PadlockFill * (Wheel > 0 ? 1.3 : 1.0 / 1.3), 0.02, 20.0);
		}
		// From the top of your vehicle, the lens closed so the aircraft fills a part of the view.
		const FVector From = V->GetActorLocation() + V->GetActorUpVector() * 450.0;
		const double Dist = FMath::Max(1.0, FVector::Dist(From, Target));
		const float Fov = static_cast<float>(FMath::Clamp(FMath::RadiansToDegrees(2.0 * FMath::Atan(SizeCm * 4.0 / PadlockFill / Dist)), 0.5, 80.0));
		Aim(From, Target, Fov);
		break;
	}
	case ESimView::Map:
	{
		if (Wheel != 0)
		{
			MapHeightM = FMath::Clamp(MapHeightM * (Wheel > 0 ? 0.8 : 1.25), 150.0, 150000.0);
		}
		if (bLook)
		{
			// Drag the map: north up (X), east right (Y).
			MapPan += FVector(-Dy, -Dx, 0.0) * MapHeightM * 0.4;
		}
		ViewCamera->SetActorLocationAndRotation(Target + MapPan + Up() * MapHeightM * 100.0, FRotator(-89.9, 0.0, 0.0));
		ViewCamera->GetCamera()->SetFieldOfView(60.0f);
		break;
	}
	default:
		break;
	}
}

FString USimViewComponent::GetViewLabel() const
{
	if (View == ESimView::Cabin || View == ESimView::Free)
	{
		return FString::Printf(TEXT("%s  %s"), *KeyOf(View), *NameOf(View));
	}
	FSubjectState S;
	FString Name = StateOf(Subject, S) ? S.Name : FString(TEXT("-"));
	if (bSubjectGone)
	{
		Name += TEXT(" (gone)");
	}
	if (View == ESimView::Vehicle)
	{
		return FString::Printf(TEXT("%s  %s"), *KeyOf(View), *NameOf(View));
	}
	if (View == ESimView::Chase && bChaseAhead)
	{
		return FString::Printf(TEXT("%s  Chase, from ahead: %s"), *KeyOf(View), *Name);
	}
	if (View == ESimView::Map)
	{
		return FString::Printf(TEXT("%s  Map, above %s"), *KeyOf(View), *Name);
	}
	return FString::Printf(TEXT("%s  %s: %s"), *KeyOf(View), *NameOf(View), *Name);
}

void USimViewComponent::ReportToTutorial()
{
	USimWorldSubsystem* W = Sim();
	const ASimOsaVehicle* V = Vehicle();
	if (W == nullptr || V == nullptr || !W->HasTutorial())
	{
		LastReportedView = KeyOf(View);
		return;
	}
	const bool bIn = V->IsInCabin();
	W->SetTutorialView(KeyOf(View), bIn, bIn ? V->GetStationId() : FString(), V->IsHoldingSeat() ? V->GetStationId() : FString());
	LastReportedView = KeyOf(View);
}
