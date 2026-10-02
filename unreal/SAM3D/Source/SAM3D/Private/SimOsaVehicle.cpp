#include "SimOsaVehicle.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "SAM3D.h"
#include "SimCabinComponent.h"
#include "SimCabinHUD.h"
#include "SimWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Engine cube is 100 cm on a side, centred on its origin.
	UStaticMeshComponent* MakeBox(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Cube,
		const FVector& SizeM, const FVector& OffsetM)
	{
		UStaticMeshComponent* Box = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Box->SetupAttachment(Parent);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cube != nullptr)
		{
			Box->SetStaticMesh(Cube);
		}
		Box->SetRelativeScale3D(SizeM);
		Box->SetRelativeLocation(OffsetM * 100.0);
		return Box;
	}

	FSimCrewHotkey Hotkey(const FKey& Key, const TCHAR* Crew, const TCHAR* Procedure, bool bQueue = false)
	{
		FSimCrewHotkey H;
		H.Key = Key;
		H.Crew = Crew;
		H.Procedure = Procedure;
		H.bQueue = bQueue;
		return H;
	}
}

ASimOsaVehicle::ASimOsaVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Rough placeholder proportions: hull about 9.1 x 2.8 m (data/systems/osa.json lists the BAZ-5937 chassis).
	Hull = MakeBox(this, Root, TEXT("Hull"), Cube, FVector(9.1, 2.8, 1.6), FVector(0.0, 0.0, 1.4));

	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(Root);
	TurretPivot->SetRelativeLocation(FVector(-100.0, 0.0, 220.0));
	Turret = MakeBox(this, TurretPivot, TEXT("Turret"), Cube, FVector(2.4, 2.0, 1.2), FVector(0.0, 0.0, 0.6));

	AntennaHinge = CreateDefaultSubobject<USceneComponent>(TEXT("AntennaHinge"));
	AntennaHinge->SetupAttachment(TurretPivot);
	AntennaHinge->SetRelativeLocation(FVector(-120.0, 0.0, 120.0));
	SearchAntenna = MakeBox(this, AntennaHinge, TEXT("SearchAntenna"), Cube, FVector(0.2, 2.4, 1.4), FVector(0.0, 0.0, 0.7));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Root);
	CameraBoom->TargetArmLength = 2500.0f;
	CameraBoom->SetRelativeRotation(FRotator(-15.0, 0.0, 0.0));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);

	// The cabin's body frame has its origin at the root (ground under the hull centre), like the hull boxes.
	Cabin = CreateDefaultSubobject<USimCabinComponent>(TEXT("Cabin"));
	Cabin->SetupAttachment(Root);
	CabinCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CabinCamera"));
	CabinCamera->SetupAttachment(Root);
	CabinCamera->SetFieldOfView(80.0f);
	CabinCamera->bAutoActivate = false;
	CabinHUDClass = ASimCabinHUD::StaticClass();

	// Placeholder crew orders until the order menu exists (phase 8). Crew names and procedures are the
	// practice scenario's (data/scenarios/osa_practice.json, data/procedures/osa_ak.json). Each procedure
	// belongs to one seat, so a key that starts a drill gives each seat involved its part.
	CrewHotkeys = {
		Hotkey(EKeys::One, TEXT("Commander"), TEXT("power_up")),
		Hotkey(EKeys::Two, TEXT("Senior operator"), TEXT("search_on_march")),
		Hotkey(EKeys::Three, TEXT("Commander"), TEXT("deploy_at_halt")),
		Hotkey(EKeys::Three, TEXT("Driver"), TEXT("pto_on_line")),
		Hotkey(EKeys::Four, TEXT("Senior operator"), TEXT("acquire_with_soc")),
		Hotkey(EKeys::Four, TEXT("Deputy commander"), TEXT("ready_capture")),
		Hotkey(EKeys::Four, TEXT("Operator"), TEXT("capture_with_ssc")),
		Hotkey(EKeys::Five, TEXT("Deputy commander"), TEXT("prepare_firing"), true),
		Hotkey(EKeys::Six, TEXT("Deputy commander"), TEXT("launch"), true),
		Hotkey(EKeys::Seven, TEXT("Senior operator"), TEXT("emcon_tv_azimuth")),
		Hotkey(EKeys::Seven, TEXT("Deputy commander"), TEXT("emcon_tv_engagement")),
		Hotkey(EKeys::Eight, TEXT("Operator"), TEXT("stow_ssc")),
		Hotkey(EKeys::Eight, TEXT("Deputy commander"), TEXT("stow_launch_console")),
		Hotkey(EKeys::Eight, TEXT("Driver"), TEXT("pto_off_line")),
		Hotkey(EKeys::Eight, TEXT("Commander"), TEXT("stow_for_march")),
		Hotkey(EKeys::Nine, TEXT("Senior operator"), TEXT("stop_search")),
		Hotkey(EKeys::Nine, TEXT("Commander"), TEXT("stow_for_transport"), true),
	};
}

void ASimOsaVehicle::BeginPlay()
{
	Super::BeginPlay();
	HeadingDeg = GetActorRotation().Yaw;
	FollowGround();
}

void ASimOsaVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	APlayerController* Pc = Cast<APlayerController>(NewController);
	if (Pc != nullptr && CabinHUDClass)
	{
		Pc->ClientSetHUD(CabinHUDClass);
	}
}

void ASimOsaVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* Pc = Cast<APlayerController>(GetController());
	USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	Drive(DeltaSeconds, Pc);
	FollowGround();
	if (Sim != nullptr && Sim->IsScenarioRunning())
	{
		UpdateSim(Sim);
		HandleHotkeys(Pc, Sim);
		AnimateFromSim(Sim);
	}
	HandleCabin(DeltaSeconds, Pc);
}

void ASimOsaVehicle::Drive(float DeltaSeconds, APlayerController* Pc)
{
	const double Dt = DeltaSeconds;
	double Throttle = 0.0;
	double Steer = 0.0;
	bool bBrake = false;
	if (Pc != nullptr)
	{
		Throttle = (Pc->IsInputKeyDown(EKeys::W) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::S) ? 1.0 : 0.0);
		Steer = (Pc->IsInputKeyDown(EKeys::D) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::A) ? 1.0 : 0.0);
		bBrake = Pc->IsInputKeyDown(EKeys::SpaceBar);
	}
	if (bBrake || Throttle == 0.0)
	{
		const double Decel = (bBrake ? BrakingMps2 : 0.5 * BrakingMps2) * Dt;
		SpeedMps = FMath::Abs(SpeedMps) <= Decel ? 0.0 : SpeedMps - FMath::Sign(SpeedMps) * Decel;
	}
	else
	{
		SpeedMps = FMath::Clamp(SpeedMps + Throttle * AccelerationMps2 * Dt, -0.3 * MaxSpeedMps, MaxSpeedMps);
	}
	// Turn rate scales with speed (no turning on the spot), capped at the full-lock rate from 10 m/s.
	const double SpeedFactor = FMath::Clamp(SpeedMps / 10.0, -1.0, 1.0);
	HeadingDeg += Steer * TurnRateDegPerS * SpeedFactor * Dt;
	const FRotator Heading(0.0, HeadingDeg, 0.0);
	SetActorLocation(GetActorLocation() + Heading.Vector() * SpeedMps * 100.0 * Dt);
}

void ASimOsaVehicle::FollowGround()
{
	// Sit on whatever has collision below (Cesium terrain tiles have collision by default).
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const FVector Here = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SimOsaGround), false, this);
	// Start well above: the vehicle can begin under the real terrain (scenario heights are approximate and Cesium
	// heights are above the ellipsoid), and a trace that starts inside the ground never hits its surface.
	if (World->LineTraceSingleByChannel(Hit, Here + FVector(0, 0, 300000.0), Here - FVector(0, 0, 300000.0), ECC_Visibility, Params))
	{
		const FRotator Pitch = FRotationMatrix::MakeFromXZ(FRotator(0.0, HeadingDeg, 0.0).Vector(), Hit.ImpactNormal).Rotator();
		SetActorLocationAndRotation(Hit.ImpactPoint, Pitch);
	}
	else
	{
		SetActorRotation(FRotator(0.0, HeadingDeg, 0.0));
	}
}

void ASimOsaVehicle::UpdateSim(USimWorldSubsystem* Sim)
{
	// The sim unit sits about 3 m above the ground, where the antennas are.
	Sim->SetUnitLocation(GetActorLocation() + GetActorUpVector() * 300.0);
	Sim->SetSimInput(StoppedInput, FMath::Abs(SpeedMps) < StoppedBelowMps);
}

void ASimOsaVehicle::AnimateFromSim(const USimWorldSubsystem* Sim)
{
	// The sim treats the hull as facing north (engagement.hpp): turret yaw is the wheel's azimuth from north,
	// minus the hull's heading, so it points the right way whichever way the hull faces.
	const double AzimuthFromNorth = Sim->GetWheelValue(TurretAzimuthWheel);
	TurretPivot->SetRelativeRotation(FRotator(0.0, AzimuthFromNorth - HeadingDeg, 0.0));
	// 1 = raised (upright), 0 = folded back 90 degrees for transport.
	const double Raised = Sim->GetSubsystemProgress(SearchAntennaSubsystem);
	AntennaHinge->SetRelativeRotation(FRotator(-90.0 * (1.0 - Raised), 0.0, 0.0));
}

void ASimOsaVehicle::HandleHotkeys(APlayerController* Pc, USimWorldSubsystem* Sim)
{
	if (Pc == nullptr)
	{
		return;
	}
	for (const FSimCrewHotkey& H : CrewHotkeys)
	{
		if (!Pc->WasInputKeyJustPressed(H.Key))
		{
			continue;
		}
		const bool bOk = Sim->OrderCrew(H.Crew, H.Procedure, H.bQueue);
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, bOk ? FColor::White : FColor::Red,
				FString::Printf(TEXT("Order: %s -> %s%s%s"), *H.Crew, *H.Procedure, H.bQueue ? TEXT(" (queued)") : TEXT(""),
					bOk ? TEXT("") : TEXT("  FAILED")));
		}
	}
}

// ---------------------------------------------------------------------------------------------- cabin

bool ASimOsaVehicle::SetInCabin(bool bEnter)
{
	if (bEnter && Cabin->GetNumStations() == 0)
	{
		FeedbackText = TEXT("No cabin layout loaded");
		FeedbackSecondsLeft = 3.0f;
		return false;
	}
	bInCabin = bEnter;
	Camera->SetActive(!bEnter);
	CabinCamera->SetActive(bEnter);
	DraggedWheel = INDEX_NONE;
	HoveredItem = INDEX_NONE;
	HoverText.Reset();
	Cabin->SetHighlightedItem(INDEX_NONE);
	if (bEnter)
	{
		Station = FMath::Clamp(Station, 0, Cabin->GetNumStations() - 1);
		ApplyStationView();
	}
	return true;
}

void ASimOsaVehicle::ApplyStationView()
{
	FVector Eye;
	FRotator View;
	if (Cabin->GetStationView(Station, Eye, View, StationLabel))
	{
		CabinCamera->SetRelativeLocationAndRotation(
			Eye, FRotator(FMath::Clamp(View.Pitch + LookPitchDeg, -85.0, 85.0), View.Yaw + LookYawDeg, 0.0));
	}
}

void ASimOsaVehicle::ShowFeedback(ESimPlayerActionResult Result)
{
	// Only physical refusals are shown: procedure mistakes go to the debrief, as in SAM Simulator.
	switch (Result)
	{
	case ESimPlayerActionResult::Blocked:
		FeedbackText = TEXT("Blocked: open its cover or lock first (keys turn one step at a time)");
		break;
	case ESimPlayerActionResult::NotYet:
		FeedbackText = TEXT("Not yet");
		break;
	case ESimPlayerActionResult::Invalid:
		FeedbackText = TEXT("Nothing happens");
		break;
	default:
		return;
	}
	FeedbackSecondsLeft = 2.0f;
}

void ASimOsaVehicle::HandleCabin(float DeltaSeconds, APlayerController* Pc)
{
	FeedbackSecondsLeft = FMath::Max(0.0f, FeedbackSecondsLeft - DeltaSeconds);
	if (Pc == nullptr)
	{
		return;
	}
	if (Pc->WasInputKeyJustPressed(CabinKey))
	{
		if (bTouring)
		{
			StopCabinTour();
		}
		else
		{
			SetInCabin(!bInCabin);
		}
	}
	if (bTouring)
	{
		UpdateTour(DeltaSeconds);
		return;
	}
	if (!bInCabin)
	{
		return;
	}
	if (Pc->WasInputKeyJustPressed(NextStationKey))
	{
		Station = (Station + 1) % FMath::Max(1, Cabin->GetNumStations());
		LookYawDeg = 0.0;
		LookPitchDeg = 0.0;
		DraggedWheel = INDEX_NONE;
	}

	// Mouse: turns the dragged hand wheel, otherwise turns the head.
	float Dx = 0.0f;
	float Dy = 0.0f;
	Pc->GetInputMouseDelta(Dx, Dy);
	if (DraggedWheel != INDEX_NONE && !Pc->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		DraggedWheel = INDEX_NONE;
	}
	if (DraggedWheel != INDEX_NONE)
	{
		if (Dx != 0.0f && !Cabin->TurnWheel(DraggedWheel, Dx * WheelTurnsPerMouseUnit))
		{
			FeedbackText = TEXT("Locked: release the wheel lock first");
			FeedbackSecondsLeft = 2.0f;
		}
	}
	else
	{
		LookYawDeg = FMath::Clamp(LookYawDeg + Dx * CabinLookDegPerMouseUnit, -110.0, 110.0);
		LookPitchDeg = FMath::Clamp(LookPitchDeg + Dy * CabinLookDegPerMouseUnit, -70.0, 60.0);
	}
	ApplyStationView();

	const FVector Start = CabinCamera->GetComponentLocation();
	const FVector End = Start + CabinCamera->GetForwardVector() * CabinReachCm;
	HoveredItem = DraggedWheel != INDEX_NONE ? DraggedWheel : Cabin->TraceItem(Start, End);
	Cabin->SetHighlightedItem(HoveredItem);
	HoverText = Cabin->DescribeItem(HoveredItem);
	if (HoveredItem == INDEX_NONE)
	{
		return;
	}
	if (Cabin->IsWheel(HoveredItem))
	{
		if (Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			DraggedWheel = HoveredItem;
		}
		const double Notches = (Pc->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1.0 : 0.0) -
							   (Pc->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1.0 : 0.0);
		if (Notches != 0.0 && !Cabin->TurnWheel(HoveredItem, Notches * WheelTurnsPerScrollNotch))
		{
			FeedbackText = TEXT("Locked: release the wheel lock first");
			FeedbackSecondsLeft = 2.0f;
		}
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		ShowFeedback(Cabin->ClickItem(HoveredItem, 1));
	}
	else if (Pc->WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		ShowFeedback(Cabin->ClickItem(HoveredItem, -1));
	}
}

// ---------------------------------------------------------------------------------------------- cabin tour

void ASimOsaVehicle::StartCabinTour(float SecondsPerView)
{
	bTouring = true;
	TourSecondsPerView = FMath::Max(1.0f, SecondsPerView);
	TourViews.Reset();  // built once the cabin layout is loaded
	TourIndex = 0;
	TourClock = 0.0f;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin tour: starting, %.0f s per view"), TourSecondsPerView);
}

void ASimOsaVehicle::StopCabinTour()
{
	bTouring = false;
	CabinCamera->SetFieldOfView(80.0f);
	LookYawDeg = 0.0;
	LookPitchDeg = 0.0;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin tour: stopped"));
}

void ASimOsaVehicle::UpdateTour(float DeltaSeconds)
{
	if (TourViews.IsEmpty())
	{
		const int32 Stations = Cabin->GetNumStations();
		if (Stations == 0)
		{
			return;  // no cabin yet
		}
		// Each seat as the operator sits, then a close-up of each instrument read from that seat.
		for (int32 S = 0; S < Stations; ++S)
		{
			FSimCabinTourView Seat;
			Seat.Station = S;
			FVector Eye;
			FRotator View;
			Cabin->GetStationView(S, Eye, View, Seat.Label);
			TourViews.Add(Seat);
			for (int32 D = 0; D < Cabin->GetNumDisplays(); ++D)
			{
				FSimCabinTourView Close;
				FString DisplayLabel;
				if (Cabin->GetDisplayView(D, Close.Station, Close.Target, DisplayLabel) && Close.Station == S)
				{
					Close.bLookAt = true;
					Close.Label = Seat.Label + TEXT(" - ") + DisplayLabel;
					TourViews.Add(Close);
				}
			}
		}
		SetInCabin(true);
		TourIndex = 0;
		TourClock = 0.0f;
		ShowTourView();
		return;
	}
	TourClock += DeltaSeconds;
	if (TourClock >= TourSecondsPerView)
	{
		TourClock = 0.0f;
		TourIndex = (TourIndex + 1) % TourViews.Num();
		ShowTourView();
	}
	ApplyStationView();
	HoverText = FString::Printf(TEXT("Cabin tour %d/%d: %s"), TourIndex + 1, TourViews.Num(), *TourViews[TourIndex].Label);
}

void ASimOsaVehicle::ShowTourView()
{
	const FSimCabinTourView& V = TourViews[TourIndex];
	Station = V.Station;
	LookYawDeg = 0.0;
	LookPitchDeg = 0.0;
	FVector Eye;
	FRotator View;
	if (Cabin->GetStationView(Station, Eye, View, StationLabel) && V.bLookAt)
	{
		const FRotator Want = (V.Target - Eye).Rotation();
		LookYawDeg = FRotator::NormalizeAxis(Want.Yaw - View.Yaw);
		LookPitchDeg = Want.Pitch - View.Pitch;
	}
	CabinCamera->SetFieldOfView(V.bLookAt ? 40.0f : 80.0f);
	ApplyStationView();
	const USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin tour view %d/%d at t=%.0f s: %s"), TourIndex + 1, TourViews.Num(),
		Sim != nullptr ? Sim->GetSimTimeSeconds() : 0.0, *V.Label);
}
