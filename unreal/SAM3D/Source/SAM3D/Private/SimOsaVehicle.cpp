#include "SimOsaVehicle.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SAM3D.h"
#include "SimCabinComponent.h"
#include "SimCabinHUD.h"
#include "SimCrewBodiesComponent.h"
#include "SimCrewVoiceComponent.h"
#include "SimPu12MastComponent.h"
#include "SimViewComponent.h"
#include "SimWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

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
	CrewBodies = CreateDefaultSubobject<USimCrewBodiesComponent>(TEXT("CrewBodies"));
	CrewBodies->SetupAttachment(Cabin);
	CrewVoice = CreateDefaultSubobject<USimCrewVoiceComponent>(TEXT("CrewVoice"));
	Views = CreateDefaultSubobject<USimViewComponent>(TEXT("Views"));
	CabinCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CabinCamera"));
	CabinCamera->SetupAttachment(Root);
	CabinCamera->SetFieldOfView(80.0f);
	CabinCamera->bAutoActivate = false;
	// A fixed exposure for the closed cabin (a luminance, see CabinExposureBrightness): auto exposure would adapt to
	// the dark compartment and burn out everything its lamps light.
	CabinCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	CabinCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	CabinCamera->PostProcessSettings.AutoExposureMinBrightness = CabinExposureBrightness;
	CabinCamera->PostProcessSettings.AutoExposureMaxBrightness = CabinExposureBrightness;
	CabinHUDClass = ASimCabinHUD::StaticClass();

	CrewHotkeys = DefaultCrewHotkeys();
}

TArray<FSimCrewHotkey> ASimOsaVehicle::DefaultCrewHotkeys()
{
	// Placeholder crew orders until the order menu exists (phase 8). Crew names and procedures are the
	// practice scenario's (data/scenarios/osa_practice.json, data/procedures/osa_ak.json). Each procedure
	// belongs to one seat, so a key that starts a drill gives each seat involved its part.
	return {
		Hotkey(EKeys::One, TEXT("Driver"), TEXT("power_up")),
		Hotkey(EKeys::One, TEXT("Operator"), TEXT("equipment_on")),
		Hotkey(EKeys::Two, TEXT("Senior operator"), TEXT("search_on_march")),
		Hotkey(EKeys::Three, TEXT("Operator"), TEXT("deploy_at_halt"), true),
		Hotkey(EKeys::Three, TEXT("Driver"), TEXT("pto_on_line")),
		Hotkey(EKeys::Three, TEXT("Deputy commander"), TEXT("orient_vehicle")),
		Hotkey(EKeys::Four, TEXT("Senior operator"), TEXT("acquire_with_soc")),
		Hotkey(EKeys::Four, TEXT("Deputy commander"), TEXT("ready_capture"), true),
		Hotkey(EKeys::Four, TEXT("Operator"), TEXT("capture_with_ssc")),
		Hotkey(EKeys::Five, TEXT("Deputy commander"), TEXT("prepare_firing"), true),
		Hotkey(EKeys::Six, TEXT("Deputy commander"), TEXT("launch"), true),
		Hotkey(EKeys::Seven, TEXT("Senior operator"), TEXT("emcon_tv_azimuth")),
		Hotkey(EKeys::Seven, TEXT("Deputy commander"), TEXT("emcon_tv_engagement")),
		Hotkey(EKeys::Eight, TEXT("Operator"), TEXT("stow_ssc")),
		Hotkey(EKeys::Eight, TEXT("Deputy commander"), TEXT("stow_launch_console")),
		Hotkey(EKeys::Eight, TEXT("Driver"), TEXT("pto_off_line")),
		Hotkey(EKeys::Nine, TEXT("Senior operator"), TEXT("stop_search")),
		Hotkey(EKeys::Nine, TEXT("Operator"), TEXT("equipment_off"), true),
		Hotkey(EKeys::Nine, TEXT("Driver"), TEXT("power_down"), true),
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
	// Esc: the mission's menu (unless the crew menu is open: Esc closes that). Nothing else moves while it is open.
	if (Pc != nullptr && Sim != nullptr && Sim->IsScenarioRunning() && !bMenuOpen && !bTouring && Pc->WasInputKeyJustPressed(EKeys::Escape))
	{
		SetMissionMenuOpen(!bMissionMenuOpen);
	}
	if (bMissionMenuOpen)
	{
		HandleMissionMenu(Pc);
		return;
	}
	// The camera views (F1-F11) and the time keys; the free camera's W A S D are its own, not the driver's.
	if (!bTouring && ShotDelayLeft < 0.0f)
	{
		Views->UpdateViews(Pc, DeltaSeconds);
	}
	Drive(DeltaSeconds, Views->IsFreeCamera() ? nullptr : Pc);
	FollowGround();
	if (Sim != nullptr && Sim->IsScenarioRunning())
	{
		if (Pc != nullptr && !bMenuOpen && Sim->IsBatteryLink() && Pc->WasInputKeyJustPressed(SwitchVehicleKey))
		{
			Sim->SwitchLinkedVehicle();
		}
		ConfigureForScenario(Sim);
		// The menu's seat, once the cabin has its stations.
		if (!PendingSeat.IsEmpty() && Cabin->GetNumStations() > 0)
		{
			for (int32 I = 0; I < Cabin->GetNumStations(); ++I)
			{
				if (Cabin->GetStationId(I) == PendingSeat)
				{
					Station = I;
					if (SetInCabin(true))
					{
						Sim->TakeCrewSeat(PendingSeat, TEXT("Player"));
						FeedbackText = FString::Printf(TEXT("Your seat: %s"), *StationLabel);
						FeedbackSecondsLeft = 4.0f;
					}
				}
			}
			UE_LOG(LogSAM3D, Log, TEXT("Mission start: the player in the %s's seat"), *PendingSeat);
			PendingSeat.Reset();
		}
		UpdateSim(Sim);
		if (!bMenuOpen)
		{
			HandleHotkeys(Pc, Sim);
		}
		AnimateFromSim(Sim);
	}
	HandleCabin(DeltaSeconds, Pc);
	if (Sim != nullptr && Sim->IsScenarioRunning())
	{
		HandleCrew(Pc, Sim, DeltaSeconds);
		if (bIsPost && !bMenuOpen)
		{
			HandlePost(Pc);
		}
	}
	// The body at the camera's seat isn't drawn (first person); the overlay's ghost hand shows the advised move.
	CrewBodies->SetHiddenStation(bInCabin && !bViewOverride ? Cabin->GetStationId(Station) : FString());
	CrewBodies->SetGhostHand(StationId, bAssist && Advice.bValid ? Advice.Control : FString());
}

void ASimOsaVehicle::HandleCrew(APlayerController* Pc, USimWorldSubsystem* Sim, float DeltaSeconds)
{
	StationId = bInCabin ? Cabin->GetStationId(Station) : FString();
	bHoldingSeat = !StationId.IsEmpty() && !Sim->GetCrewSeatHolder(StationId).IsEmpty();
	if (Pc == nullptr || !bInCabin || StationId.IsEmpty())
	{
		Cabin->SetAdvisedControl(FString());
		bMenuOpen = false;
		return;
	}
	// T: take this seat (its NPC stops and its orders become yours), or give it back.
	if (Pc->WasInputKeyJustPressed(EKeys::T))
	{
		if (bHoldingSeat)
		{
			Sim->LeaveCrewSeat(StationId);
		}
		else
		{
			Sim->TakeCrewSeat(StationId, TEXT("Player"));
		}
		bHoldingSeat = !Sim->GetCrewSeatHolder(StationId).IsEmpty();
	}
	// H: the training overlay's level, round and round: guided, hint, demonstrate, off (docs/crew/08 section 2). An
	// exam scenario keeps it off. The task: К бою until ready, then the capture, then the launch.
	if (Pc->WasInputKeyJustPressed(EKeys::H) && Sim->GetAssistLevel() != TEXT("exam"))
	{
		static const TCHAR* Levels[] = {TEXT("guided"), TEXT("hint"), TEXT("demonstrate"), TEXT("unassisted")};
		int32 Next = 0;
		if (bAssist)
		{
			for (int32 I = 0; I < 4; ++I)
			{
				if (AssistLevel == Levels[I])
				{
					Next = (I + 1) % 4;
				}
			}
		}
		AssistLevel = Levels[Next];
		Sim->SetAssistLevel(AssistLevel);
		bAssist = AssistLevel != TEXT("unassisted");
		AssistClock = 0.0f;
		if (!bAssist)
		{
			Cabin->SetAdvisedControl(FString());
			Advice = FSimDrillAdvice();
		}
	}
	if (bAssist)
	{
		AssistClock -= DeltaSeconds;
		if (AssistClock <= 0.0f)
		{
			AssistClock = 1.0f;
			Advice = Sim->GetAssistance(StationId);
			AssistTask = Advice.Task;
			Cabin->SetAdvisedControl(Advice.bValid ? Advice.Control : FString());
		}
	}
	LastVerdict = Sim->GetLastMoveVerdict(StationId);
	// O: the crew menu. 1-9 choose a line, 0 the next page, O or Escape close it.
	if (Pc->WasInputKeyJustPressed(EKeys::O))
	{
		Sim->NoteTutorialEvent(TEXT("crew_menu"));
		bMenuOpen = !bMenuOpen;
		MenuPage = 0;
		MenuEntries = Sim->GetCrewMenu(StationId);
	}
	if (!bMenuOpen)
	{
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Escape))
	{
		bMenuOpen = false;
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Zero))
	{
		const int32 Pages = FMath::Max(1, (MenuEntries.Num() + 8) / 9);
		MenuPage = (MenuPage + 1) % Pages;
	}
	static const FKey Digits[9] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 I = 0; I < 9; ++I)
	{
		if (!Pc->WasInputKeyJustPressed(Digits[I]))
		{
			continue;
		}
		const int32 Index = MenuPage * 9 + I;
		if (!MenuEntries.IsValidIndex(Index))
		{
			continue;
		}
			const FSimCrewMenuEntry& E = MenuEntries[Index];
		FString Reason;
		const bool bOk = E.bOrder ? Sim->TryCrewOrder(E.Id, Reason) : Sim->PlayerSay(StationId, E.Id);
		const FString NotNow = FString::Printf(TEXT("  (not now: %s)"), Reason.IsEmpty() ? TEXT("can't") : *Reason);
		FeedbackText = FString::Printf(TEXT("%s %s%s"), E.bOrder ? TEXT("Order:") : TEXT("Report:"), *E.Ru, bOk ? TEXT("") : *NotNow);
		FeedbackSecondsLeft = 3.0f;
		bMenuOpen = false;
	}
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
	Sim->SetUnitHeading(Sim->YawToHeadingDeg(GetActorLocation(), HeadingDeg));  // HeadingDeg is the Unreal yaw
	Sim->SetSimInput(StoppedInput, FMath::Abs(SpeedMps) < StoppedBelowMps);
}

void ASimOsaVehicle::ConfigureForScenario(USimWorldSubsystem* Sim)
{
	const void* Now = Sim->GetScenarioRunner();
	if (Now == ConfiguredFor)
	{
		return;
	}
	if (ConfiguredFor != nullptr)
	{
		// A battery link: the player moved to the other vehicle. Out of the cabin, the vehicle where that one stands, the
		// old one's parts gone (its cabin rebuilds from the new layout).
		SetInCabin(false);
		SpeedMps = 0.0;
		const FVector At = Sim->GetUnitLocation();
		HeadingDeg = Sim->HeadingToYawDeg(At, Sim->GetUnitHeading());
		SetActorLocation(At - FVector(0.0, 0.0, 300.0));
		FollowGround();
		// (renamed first: the next switch makes them again under the same names)
		if (PostExterior != nullptr)
		{
			PostExterior->Rename(*MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("PostExteriorRemoved")).ToString());
			PostExterior->DestroyComponent();
			PostExterior = nullptr;
		}
		if (PostMast != nullptr)
		{
			PostMast->Rename(*MakeUniqueObjectName(this, USimPu12MastComponent::StaticClass(), TEXT("PostMastRemoved")).ToString());
			PostMast->DestroyComponent();
			PostMast = nullptr;
		}
		CrewHotkeys = DefaultCrewHotkeys();
		Station = 0;
		PickedTarget = INDEX_NONE;
		bMenuOpen = false;
		FeedbackText = FString::Printf(TEXT("In %s"), Sim->IsCommandPost() ? TEXT("the PU-12M") : TEXT("the Osa"));
		FeedbackSecondsLeft = 3.0f;
	}
	ConfiguredFor = Now;
	bIsPost = Sim->IsCommandPost();
	if (!bIsPost)
	{
		for (UStaticMeshComponent* Box : {Hull.Get(), Turret.Get(), SearchAntenna.Get()})
		{
			if (Box != nullptr)
			{
				Box->SetVisibility(!bInCabin);
			}
		}
		Cabin->SetCabinHidden(false);
		return;
	}
	// The PU-12M: its traced exterior (tools/blender/pu12m_model.py, imported to /Game/SAM3D/Vehicles) and its mast
	// (USimPu12MastComponent) instead of the Osa's placeholder boxes; its origin is the middle of the wheelbase on the
	// ground, as the cabin layout's body frame (data/cabins/pu12m.json).
	for (UStaticMeshComponent* Box : {Hull.Get(), Turret.Get(), SearchAntenna.Get()})
	{
		if (Box != nullptr)
		{
			Box->SetVisibility(false);
		}
	}
	PostExterior = NewObject<UStaticMeshComponent>(this, TEXT("PostExterior"));
	PostExterior->SetupAttachment(Root);
	PostExterior->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SAM3D/Vehicles/SM_PU12M.SM_PU12M"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	PostExterior->SetStaticMesh(Mesh);
	PostExterior->RegisterComponent();
	PostExterior->SetVisibility(!bInCabin);
	Cabin->SetCabinHidden(!bInCabin);
	PostMast = NewObject<USimPu12MastComponent>(this, TEXT("PostMast"));
	PostMast->SetupAttachment(Root);
	PostMast->RegisterComponent();
	// The crew's drills go through the crew menu (O) here; the number keys pick launchers in the commander's seat.
	CrewHotkeys.Reset();
	UE_LOG(LogSAM3D, Log, TEXT("Vehicle: the PU-12M command post%s"), Mesh != nullptr ? TEXT("") : TEXT(" (SM_PU12M not imported: no exterior)"));
}

void ASimOsaVehicle::AnimateFromSim(const USimWorldSubsystem* Sim)
{
	if (bIsPost)
	{
		// The mast at the console's mast_up progress (the radio operator cranking it), the AB-1 out once it is set down.
		if (PostMast != nullptr)
		{
			PostMast->SetExtension(FMath::Max(0.f, Sim->GetPostMastExtension()));
			PostMast->SetGeneratorOut(Sim->IsPostGeneratorOut());
		}
		return;
	}
	// The wheel's azimuth is in the crew's frame (the gyro half-compass, engagement.hpp): as true azimuth from north,
	// minus the hull's true heading, it is the turret's yaw on the hull (relative, so the same in either mapping).
	const double AzimuthFromNorth = Sim->IndicatedToTrueAzimuth(Sim->GetWheelValue(TurretAzimuthWheel));
	TurretPivot->SetRelativeRotation(FRotator(0.0, AzimuthFromNorth - Sim->GetUnitHeading(), 0.0));
	// 1 = raised (upright), 0 = folded back 90 degrees for transport.
	const double Raised = Sim->GetSubsystemProgress(SearchAntennaSubsystem);
	// The search antenna turns on its own (33 rpm) once raised: with the EM layer, where its beam points (roadmap E3),
	// relative to the turret it stands on.
	double AntennaYaw = 0.0;
	double AntennaTrueAz = 0.0;
	if (Raised >= 1.0 && Sim->GetSearchAntennaAzimuth(AntennaTrueAz))
	{
		AntennaYaw = FRotator::NormalizeAxis(AntennaTrueAz - AzimuthFromNorth);
	}
	AntennaHinge->SetRelativeRotation(FRotator(-90.0 * (1.0 - Raised), AntennaYaw, 0.0));
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

// ---------------------------------------------------------------------------------------------- the command post

TArray<int32> ASimOsaVehicle::PickList() const
{
	const USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || !bIsPost || StationId.IsEmpty())
	{
		return {};
	}
	if (StationId == Sim->GetCommanderStation())
	{
		return Sim->GetPostBoardNumbers();
	}
	return Sim->GetPostTrackNumbers();
}

void ASimOsaVehicle::PickTarget(int32 Direction)
{
	const TArray<int32> List = PickList();
	if (List.Num() == 0)
	{
		PickedTarget = INDEX_NONE;
		return;
	}
	const int32 At = List.IndexOfByKey(PickedTarget);
	const int32 Next = At == INDEX_NONE ? (Direction >= 0 ? 0 : List.Num() - 1) : (At + (Direction >= 0 ? 1 : -1) + List.Num()) % List.Num();
	PickedTarget = List[Next];
}

bool ASimOsaVehicle::DesignatePicked(int32 LauncherIndex)
{
	USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || PickedTarget < 0)
	{
		return false;
	}
	const TArray<FSimLauncherStatus> Launchers = Sim->GetLauncherStatus();
	if (!Launchers.IsValidIndex(LauncherIndex))
	{
		return false;
	}
	const bool bOk = Sim->DesignateBatteryTarget(Launchers[LauncherIndex].Callsign, PickedTarget);
	FeedbackText = bOk ? FString::Printf(TEXT("Target %d to %s"), PickedTarget, *Launchers[LauncherIndex].Callsign)
		: FString::Printf(TEXT("Can't give %d to %s now (the net down, the launcher empty, or the number gone)"), PickedTarget,
			*Launchers[LauncherIndex].Callsign);
	FeedbackSecondsLeft = 3.0f;
	return bOk;
}

bool ASimOsaVehicle::PlotPicked()
{
	USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || PickedTarget < 0)
	{
		return false;
	}
	const bool bOk = Sim->PlotPostTrack(PickedTarget);
	FeedbackText = bOk ? FString::Printf(TEXT("Plotting %02d"), PickedTarget)
		: TEXT("Not now: take the seat (T), and finish the last mark first");
	FeedbackSecondsLeft = 2.5f;
	return bOk;
}

void ASimOsaVehicle::HandlePost(APlayerController* Pc)
{
	const USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Pc == nullptr || Sim == nullptr || !bInCabin || StationId.IsEmpty())
	{
		return;
	}
	if (!PickList().Contains(PickedTarget))
	{
		PickedTarget = INDEX_NONE;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::RightBracket))
	{
		PickTarget(1);
	}
	if (Pc->WasInputKeyJustPressed(EKeys::LeftBracket))
	{
		PickTarget(-1);
	}
	if (StationId == Sim->GetCommanderStation())
	{
		static const FKey Digits[5] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five};
		for (int32 I = 0; I < 5; ++I)
		{
			if (Pc->WasInputKeyJustPressed(Digits[I]))
			{
				DesignatePicked(I);
			}
		}
	}
	else if (Pc->WasInputKeyJustPressed(EKeys::P))
	{
		PlotPicked();
	}
}

// ---------------------------------------------------------------------------------------------- the mission's menu

void ASimOsaVehicle::StartInSeat(const FString& SeatId)
{
	PendingSeat = SeatId;
}

bool ASimOsaVehicle::CanLeaveMission() const
{
	return GetWorld() != nullptr && GetWorld()->GetNetMode() == NM_Standalone;
}

void ASimOsaVehicle::SetMissionMenuOpen(bool bOpen)
{
	bMissionMenuOpen = bOpen;
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim != nullptr && CanLeaveMission())
	{
		Sim->SetSimPaused(bOpen);  // a networked game runs on for the others
	}
	DraggedWheel = INDEX_NONE;
}

void ASimOsaVehicle::HandleMissionMenu(APlayerController* Pc)
{
	if (Pc == nullptr)
	{
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Enter))
	{
		SetMissionMenuOpen(false);
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Q))
	{
		FGenericPlatformMisc::RequestExit(false);
		return;
	}
	if (!CanLeaveMission())
	{
		return;
	}
	const FName Level(*UGameplayStatics::GetCurrentLevelName(this, true));
	if (Pc->WasInputKeyJustPressed(EKeys::R))
	{
		// The same mission from the start: the level's own options again (or the command line's scenario).
		const FString Options = FString::Join(GetWorld()->URL.Op, TEXT("?"));
		UE_LOG(LogSAM3D, Log, TEXT("Mission menu: restart (%s)"), *Options);
		UGameplayStatics::OpenLevel(this, Level, true, Options);
	}
	else if (Pc->WasInputKeyJustPressed(EKeys::M))
	{
		UE_LOG(LogSAM3D, Log, TEXT("Mission menu: to the missions"));
		UGameplayStatics::OpenLevel(this, Level, true, TEXT("SimMenu"));
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
	// The placeholder hull's boxes enclose the cabin: from inside they would close the windows to Lumen's sky light
	// (a closed mesh is solid to its distance fields), so they are hidden while the camera is in the cabin.
	for (UStaticMeshComponent* Outside : {Hull.Get(), Turret.Get(), SearchAntenna.Get()})
	{
		if (Outside != nullptr)
		{
			Outside->SetVisibility(!bEnter && !bIsPost);
		}
	}
	if (PostExterior != nullptr)
	{
		PostExterior->SetVisibility(!bEnter);
	}
	// The PU-12M's compartment is closed: from outside nothing of it shows, and its lining (thick, to keep Lumen's sky
	// out) would stand through the hull.
	Cabin->SetCabinHidden(bIsPost && !bEnter);
	SetDaylightShut(bIsPost && bEnter);
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

void ASimOsaVehicle::SetDaylightShut(bool bShut)
{
	// The PU-12M's hatches are shut and its windscreens' armoured covers closed (the squad leader and driver drawing): no
	// daylight reaches the compartment, only its lamps light it. Lumen still lets the sky light seep through the hull's
	// lining and paints the cream walls mauve, so while the camera is inside, the sky lights are put out. (The Osa's cabin
	// has windows: its daylight stays.)
	if (bShut)
	{
		for (TObjectIterator<USkyLightComponent> It; It; ++It)
		{
			USkyLightComponent* Sky = *It;
			if (Sky != nullptr && Sky->GetWorld() == GetWorld() && !ShutSkyLights.Contains(Sky))
			{
				ShutSkyLights.Add(Sky, Sky->Intensity);
				Sky->SetIntensity(0.0f);
			}
		}
		return;
	}
	for (const TPair<TWeakObjectPtr<USkyLightComponent>, float>& Shut : ShutSkyLights)
	{
		if (USkyLightComponent* Sky = Shut.Key.Get())
		{
			Sky->SetIntensity(Shut.Value);
		}
	}
	ShutSkyLights.Reset();
}

void ASimOsaVehicle::SetCabinViewOverride(FVector BodyLocationM, FRotator Rotation, float FovDeg)
{
	if (!bInCabin && !SetInCabin(true))
	{
		return;
	}
	bTouring = false;
	bViewOverride = true;
	// The layout's body frame (x right, y forward, z up, m) to the vehicle's (X forward, Y right, Z up, cm).
	OverrideLocation = FVector(BodyLocationM.Y, BodyLocationM.X, BodyLocationM.Z) * 100.0;
	OverrideRotation = Rotation;
	CabinCamera->SetFieldOfView(FovDeg);
	ApplyStationView();
}

void ASimOsaVehicle::ClearCabinViewOverride()
{
	bViewOverride = false;
	CabinCamera->SetFieldOfView(80.0f);
	ApplyStationView();
}

void ASimOsaVehicle::ApplyStationView()
{
	if (bViewOverride)
	{
		CabinCamera->SetRelativeLocationAndRotation(OverrideLocation, OverrideRotation);
		return;
	}
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
	if (ShotDelayLeft >= 0.0f)
	{
		ShotDelayLeft -= DeltaSeconds;
		if (ShotDelayLeft < 0.0f)
		{
			StartCabinTour(3.0f);
		}
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
		bViewOverride = false;
		return;
	}
	if (bViewOverride)
	{
		ApplyStationView();
		return;
	}
	if (Pc->WasInputKeyJustPressed(NextStationKey))
	{
		NextStation();
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
		// Head and shoulders: far enough round to reach the distribution cabinet behind the rear seat.
		LookYawDeg = FMath::Clamp(LookYawDeg + Dx * CabinLookDegPerMouseUnit, -135.0, 135.0);
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

void ASimOsaVehicle::NextStation()
{
	Station = (Station + 1) % FMath::Max(1, Cabin->GetNumStations());
	LookYawDeg = 0.0;
	LookPitchDeg = 0.0;
	DraggedWheel = INDEX_NONE;
	if (bInCabin)
	{
		ApplyStationView();
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

void ASimOsaVehicle::StartCabinShots(const FString& Folder, float DelaySeconds)
{
	ShotFolder = Folder;
	ShotDelayLeft = FMath::Max(0.0f, DelaySeconds);
	ShotTaken = INDEX_NONE;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin shots: into %s after %.0f s"), *Folder, ShotDelayLeft);
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
			// -SimShotsAround: the seat's surroundings too, to look for parts running into each other.
			if (FParse::Param(FCommandLine::Get(), TEXT("SimShotsAround")))
			{
				static const struct { double Yaw, Pitch; const TCHAR* Name; } Around[] = {
					{-90.0, -10.0, TEXT("left")}, {90.0, -10.0, TEXT("right")}, {180.0, -5.0, TEXT("behind")},
					{0.0, -55.0, TEXT("down")}, {0.0, 40.0, TEXT("up")}};
				for (const auto& A : Around)
				{
					FSimCabinTourView Look = Seat;
					Look.YawDeg = A.Yaw;
					Look.PitchDeg = A.Pitch;
					Look.FovDeg = 100.0f;
					Look.Label = Seat.Label + TEXT(" - ") + A.Name;
					TourViews.Add(Look);
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
	if (!ShotFolder.IsEmpty() && ShotTaken != TourIndex && TourClock >= 0.75f * TourSecondsPerView)
	{
		// The picture late in the view: the lamps and the exposure have settled.
		FString Name = TourViews[TourIndex].Label.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("/"), TEXT("_"));
		const FString File = FPaths::Combine(ShotFolder, FString::Printf(TEXT("%02d_%s.png"), TourIndex + 1, *Name));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Cabin shot: %s"), *File);
		ShotTaken = TourIndex;
	}
	if (TourClock >= TourSecondsPerView)
	{
		TourClock = 0.0f;
		if (!ShotFolder.IsEmpty() && TourIndex + 1 >= TourViews.Num())
		{
			UE_LOG(LogSAM3D, Log, TEXT("Cabin shots: done"));
			FGenericPlatformMisc::RequestExit(false);
			return;
		}
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
	else
	{
		LookYawDeg = V.YawDeg;
		LookPitchDeg = V.PitchDeg;
	}
	CabinCamera->SetFieldOfView(V.bLookAt ? 40.0f : (V.FovDeg > 0.0f ? V.FovDeg : 80.0f));
	ApplyStationView();
	const USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin tour view %d/%d at t=%.0f s: %s"), TourIndex + 1, TourViews.Num(),
		Sim != nullptr ? Sim->GetSimTimeSeconds() : 0.0, *V.Label);
}
