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
	// W A S D are the walker's feet while he is up in the cabin, not the driver's.
	Drive(DeltaSeconds, Views->IsFreeCamera() || IsWalking() ? nullptr : Pc);
	if (bAutoBrake && SpeedMps == 0.0)
	{
		bAutoBrake = false;  // the lesson autoplay's halt is made
	}
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
	// A lesson's highlight wins over the training overlay's (it is set after HandleCrew's).
	const FString LessonHand = Sim != nullptr && Sim->HasTutorial() ? HandleLesson(Pc, Sim, DeltaSeconds) : FString();
	// The body at the camera's seat isn't drawn (first person); the ghost hand shows the advised move.
	// Walking, his own body is the one he walks in: the seat he holds is empty (another's seat keeps its crewman).
	const FString Own = bWalking ? (bHoldingSeat ? StationId : FString()) : Cabin->GetStationId(Station);
	CrewBodies->SetHiddenStation(bInCabin && !bViewOverride ? Own : FString());
	// Nothing of a crewmate inside his head: a hand's breadth seated, more on his feet among them.
	const bool bStanding = bInCabin && !bViewOverride && StandStation == Station;
	CrewBodies->SetClearAround(CabinCamera->GetComponentLocation(), !bInCabin || bViewOverride ? 0.0f : (bStanding ? 60.0f : (bWalking ? 35.0f : 22.0f)));
	// In a lesson only the lesson's hand (the drill master's advice could point elsewhere).
	const bool bLesson = Sim != nullptr && Sim->HasTutorial();
	CrewBodies->SetGhostHand(StationId, bLesson ? LessonHand : (bAssist && Advice.bValid ? Advice.Control : FString()));
}

// ---------------------------------------------------------------------------------------------- lessons

int32 ASimOsaVehicle::FindStation(const FString& Id) const
{
	for (int32 I = 0; I < Cabin->GetNumStations(); ++I)
	{
		if (Cabin->GetStationId(I) == Id)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

FString ASimOsaVehicle::HandleLesson(APlayerController* Pc, USimWorldSubsystem* Sim, float DeltaSeconds)
{
	const FSimTutorialStep Step = Sim->GetTutorialStep();
	if (Pc != nullptr && !bMenuOpen)
	{
		// Enter: a reading step is read (always the same key, always shown on the card).
		if (Pc->WasInputKeyJustPressed(EKeys::Enter))
		{
			Sim->NoteTutorialContinue();
		}
		// L: the head onto what the step shows.
		if (Pc->WasInputKeyJustPressed(EKeys::L))
		{
			LookAtLessonTarget();
		}
	}
	// The step's control pulses in the cabin (while it waits too: the learner finds it ahead), or a tour step's control
	// or lamp; the ghost hand reaches for it from the lesson's seat when it is time.
	const bool bAction = Step.Phase == ESimTutorialPhase::Act || Step.Phase == ESimTutorialPhase::Wait;
	Cabin->SetAdvisedControl(bAction && !Step.Control.IsEmpty() ? Step.Control : Step.Look);
	RunLessonAutoplay(Sim, Step, DeltaSeconds);
	// The ghost hand is the last of the growing hints (with the card's "Press L"): a demonstration for a learner who
	// hasn't found the control in 25 s. From the start it would cover the very control and its label.
	const bool bAtSeat = bInCabin && (Step.Seat.IsEmpty() || StationId == Step.Seat);
	// A skipped step: it shows the instructor's move at once.
	return Step.Phase == ESimTutorialPhase::Act && !Step.Control.IsEmpty() && bAtSeat && (Step.ShownSeconds > 25.0f || Step.bInstructor)
		? Step.Control : FString();
}

void ASimOsaVehicle::LookAtLessonTarget()
{
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || !Sim->HasTutorial())
	{
		return;
	}
	const FSimTutorialStep Step = Sim->GetTutorialStep();
	const FString Target = !Step.Look.IsEmpty() ? Step.Look : Step.Control;
	if (Target.IsEmpty())
	{
		FeedbackText = TEXT("Nothing to look at in this step");
		FeedbackSecondsLeft = 2.0f;
		return;
	}
	// From the lesson's seat: into the cabin, or back to it (sitting down if he was up and walking).
	bWalking = false;
	const int32 Seat = Step.Seat.IsEmpty() ? INDEX_NONE : FindStation(Step.Seat);
	if (Seat != INDEX_NONE && Seat != Station)
	{
		Station = Seat;
		DraggedWheel = INDEX_NONE;
	}
	if (!bInCabin && !SetInCabin(true))
	{
		return;
	}
	FVector World;
	FVector Facing;
	FString Label;
	FVector Eye;
	FRotator View;
	FString SeatLabel;
	if (!Cabin->GetHighlightTarget(Target, World, Label, Facing) || !Cabin->GetStationView(Station, Eye, View, SeatLabel))
	{
		return;
	}
	// The eye and the target in the vehicle's frame (the cabin camera sits at Eye on the root). A control of this seat
	// beyond arm's reach: he gets up and steps in front of it (simcore/crew_body.hpp), 70 cm off its panel, his eye at
	// the seat's height (the compartment is too low to stand straight). From the seat the deputy commander's gyro panel
	// is nearly edge-on, 1.5 m off, and its switches hide one another.
	const FVector At = GetActorTransform().InverseTransformPosition(World);
	const int32 TargetItem = Cabin->FindItem(Target);
	// Out of reach, or behind him past the head's turn (the PU-12M driver's input board at his back).
	const bool bBehind = FMath::Abs(FRotator::NormalizeAxis((At - Eye).Rotation().Yaw - View.Yaw)) > 130.0;
	const bool bOwnFar = TargetItem != INDEX_NONE && Cabin->GetItemStation(TargetItem) == Cabin->GetStationId(Station) &&
		((At - Eye).Size() > 0.9 * CabinReachCm || bBehind);
	FVector Front = At + GetActorTransform().InverseTransformVectorNoScale(Facing).GetSafeNormal() * 70.0;
	Front.Z = Eye.Z;
	StandOffset = bOwnFar ? Front - Eye : FVector::ZeroVector;
	StandStation = bOwnFar ? Station : INDEX_NONE;
	const FRotator Want = (At - Eye - StandOffset).Rotation();
	LookYawDeg = FMath::Clamp(FRotator::NormalizeAxis(Want.Yaw - View.Yaw), -135.0, 135.0);
	LookPitchDeg = FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch - View.Pitch), -70.0, 60.0);
	ApplyStationView();
	Sim->NoteTutorialEvent(TEXT("look"));
}

void ASimOsaVehicle::RunLessonAutoplay(USimWorldSubsystem* Sim, const FSimTutorialStep& Step, float DeltaSeconds)
{
	if (!bAutoplayAsked)
	{
		bAutoplayAsked = true;
		FParse::Value(FCommandLine::Get(), TEXT("SimTutorialAutoplay="), AutoplayFolder);
		if (!AutoplayFolder.IsEmpty())
		{
			UE_LOG(LogSAM3D, Log, TEXT("Lesson autoplay: into %s"), *AutoplayFolder);
		}
	}
	if (AutoplayFolder.IsEmpty() || Step.Count == 0)
	{
		return;
	}
	if (Step.Number != AutoStep)
	{
		AutoStep = Step.Number;
		AutoClock = 0.0f;
		bAutoLooked = bAutoShot = bAutoUi = bAutoReachChecked = bAutoWatched = false;
		AutoClicks = 0;
	}
	AutoClock += DeltaSeconds;
	// Look where the step points, photograph the step as the learner sees it, then do it as he would.
	if (AutoClock > 0.5f && !bAutoLooked && bInCabin && (!Step.Look.IsEmpty() || !Step.Control.IsEmpty()))
	{
		LookAtLessonTarget();
		bAutoLooked = true;
		// A wheel worked against a display: the photograph shows what the learner sees holding it (the watch view).
		const int32 Wheel = Step.bWheel ? Cabin->FindItem(Step.Control) : INDEX_NONE;
		FVector Shown;
		FVector Facing;
		FString Label;
		const FString Watch = Wheel != INDEX_NONE ? Cabin->GetWheelWatch(Wheel) : FString();
		if (!Watch.IsEmpty() && Cabin->GetHighlightTarget(Watch, Shown, Label, Facing) && LookAnglesTo(Shown, LookYawDeg, LookPitchDeg))
		{
			ApplyStationView();
			bAutoWatched = true;
		}
	}
	if (bAutoWatched && AutoClock > 1.9f)
	{
		LookAtLessonTarget();  // back to the wheel, as letting it go does
		bAutoWatched = false;
	}
	const bool bReady = Step.Phase == ESimTutorialPhase::Read || Step.Phase == ESimTutorialPhase::Act || Step.Phase == ESimTutorialPhase::End;
	if (AutoClock > 1.5f && !bAutoShot && bReady)
	{
		const FString File = FPaths::Combine(AutoplayFolder, FString::Printf(TEXT("%02d_%s.png"), Step.Number, *Step.Id));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogSAM3D, Log, TEXT("Lesson autoplay: step %d/%d '%s' (%s) at t=%.0f s"), Step.Number, Step.Count, *Step.Title,
			*StaticEnum<ESimTutorialPhase>()->GetNameStringByValue(static_cast<int64>(Step.Phase)), Sim->GetSimTimeSeconds());
		bAutoShot = true;
	}
	if (Step.Phase == ESimTutorialPhase::End)
	{
		AutoEndClock = AutoEndClock < 0.0f ? 0.0f : AutoEndClock + DeltaSeconds;
		if (AutoEndClock > 4.0f)
		{
			UE_LOG(LogSAM3D, Log, TEXT("Lesson autoplay: done"));
			FGenericPlatformMisc::RequestExit(false);
			AutoplayFolder.Reset();
		}
		return;
	}
	// An order or a report: the crew menu opens first, as the learner opens it (the photograph shows the line marked).
	if (Step.Phase == ESimTutorialPhase::Act && !Step.Order.IsEmpty() && AutoClock > 0.8f && AutoClicks == 0 && !bMenuOpen && bInCabin)
	{
		OpenCrewMenu(Sim);
	}
	if (AutoClock < 2.2f)
	{
		return;
	}
	if (Step.Phase == ESimTutorialPhase::Read)
	{
		Sim->NoteTutorialContinue();
		AutoClock = -100.0f;  // until the next step comes up
		return;
	}
	if (Step.Phase == ESimTutorialPhase::Act && (Step.bWheel || !Step.Control.IsEmpty() || !Step.Order.IsEmpty()))
	{
		// As a person would: after L, the cross is on the step's control and in reach (the autoplay's own hand doesn't
		// need it, so it says so here).
		if (!bAutoReachChecked && !Step.Control.IsEmpty())
		{
			bAutoReachChecked = true;
			const int32 Want = Cabin->FindItem(Step.Control);
			if (HoveredItem != Want)
			{
				UE_LOG(LogSAM3D, Warning, TEXT("Lesson autoplay: step %d '%s': after L the cross isn't on %s (on: %s)"), Step.Number, *Step.Title,
					*Step.Control, HoveredItem == INDEX_NONE ? TEXT("nothing in reach") : *Cabin->GetItemControl(HoveredItem));
			}
		}
		if (Step.bWheel)
		{
			// By hand: at most 15 degrees or 1.5 km a second, onto the mark.
			const double Err = Step.bWraps ? FMath::Fmod(Step.Target - Step.Value + 540.0, 360.0) - 180.0 : Step.Target - Step.Value;
			const double Rate = (Step.bWraps ? 15.0 : 1500.0) * DeltaSeconds;
			if (Sim->GetTutorialRefusal(Step.Control).IsEmpty())
			{
				Sim->PlayerTurnWheel(Step.Control, Step.Value + FMath::Clamp(Err, -Rate, Rate));
			}
			return;
		}
		if (!Step.Control.IsEmpty() && AutoClock > 2.2f + 0.8f * static_cast<float>(AutoClicks))
		{
			// A click on the item, the way the step's position lies (a selector steps one position a click).
			const int32 Item = Cabin->FindItem(Step.Control);
			if (Item != INDEX_NONE && Sim->GetTutorialRefusal(Step.Control).IsEmpty())
			{
				ShowFeedback(Cabin->ClickItem(Item, Step.Position >= Step.CurrentPosition ? 1 : -1));
			}
			++AutoClicks;
			return;
		}
		if (!Step.Order.IsEmpty() && AutoClock > 2.2f + 1.5f * static_cast<float>(AutoClicks))
		{
			// The crew menu's line, as a learner picks it.
			bMenuOpen = false;
			if (Sim->GetTutorialMenuRefusal(Step.Order).IsEmpty())
			{
				FString Why;
				const bool bOk = Step.bReport ? Sim->PlayerSay(Step.Seat, Step.Order) : Sim->TryCrewOrder(Step.Order, Why);
				FeedbackText = FString::Printf(TEXT("%s %s%s"), Step.bReport ? TEXT("Report:") : TEXT("Order:"), *Step.OrderRu,
					bOk ? TEXT("") : *FString::Printf(TEXT("  (not now: %s)"), *Why));
				FeedbackSecondsLeft = 3.0f;
			}
			++AutoClicks;
		}
		return;
	}
	// The front end's own conditions (the basics): the views, the cabin, the seats, the keys, driving.
	if (bAutoUi)
	{
		return;
	}
	bAutoUi = true;
	for (const FString& C : Step.DoneWhen)
	{
		FString Head;
		FString Arg;
		if (!C.Split(TEXT(":"), &Head, &Arg))
		{
			Head = C;
		}
		if (Head == TEXT("view"))
		{
			Views->SetViewByKey(Arg);
		}
		else if (Head == TEXT("cabin"))
		{
			SetInCabin(true);
		}
		else if (Head == TEXT("seat"))
		{
			const int32 Seat = FindStation(Arg);
			if (Seat != INDEX_NONE)
			{
				Station = Seat;
				SetInCabin(true);
			}
		}
		else if (Head == TEXT("ui"))
		{
			if (Arg == TEXT("time_faster"))
			{
				Sim->SetTimeScale(2.0f);
			}
			else if (Arg == TEXT("time_normal"))
			{
				Sim->SetTimeScale(1.0f);
			}
			Sim->NoteTutorialEvent(Arg);
		}
		else if (Head == TEXT("plotted"))
		{
			// The plotter: ] then P, as the learner does.
			PickTarget(1);
			PlotPicked();
			bAutoUi = false;  // and again a little later if it didn't take (still writing the last), or to keep the board
			AutoClock = Step.DoneWhen.Contains(TEXT("designated")) ? -13.0f : 1.0f;  // fresh for the commander
		}
		else if (Head == TEXT("designated") && !Step.DoneWhen.Contains(TEXT("plotted")) && StationId != Sim->GetCommanderStation())
		{
			// The plotter waiting for the commander: the board kept fresh, the next target again every 15 s or so.
			PickTarget(1);
			PlotPicked();
			bAutoUi = false;
			AutoClock = -13.0f;
		}
		else if (Head == TEXT("moved"))
		{
			AutoThrottle = 1.0;
			bAutoBrake = false;
		}
		else if (Head == TEXT("input") && Arg == TEXT("vehicle_stopped"))
		{
			AutoThrottle = 0.0;
			bAutoBrake = true;
		}
	}
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
	if (Pc->WasInputKeyJustPressed(EKeys::T) && Sim->IsTutorialStrict())
	{
		FeedbackText = TEXT("The lesson keeps you in your seat");
		FeedbackSecondsLeft = 2.5f;
	}
	else if (Pc->WasInputKeyJustPressed(EKeys::T))
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
		if (bMenuOpen)
		{
			bMenuOpen = false;
		}
		else
		{
			OpenCrewMenu(Sim);
		}
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
		// A lesson: only the line its step asks for.
		const FString Refusal = Sim->GetTutorialMenuRefusal(E.Id);
		if (!Refusal.IsEmpty())
		{
			FeedbackText = Refusal;
			FeedbackSecondsLeft = 3.0f;
			continue;
		}
		FString Reason;
		const bool bOk = E.bOrder ? Sim->TryCrewOrder(E.Id, Reason) : Sim->PlayerSay(StationId, E.Id);
		const FString NotNow = FString::Printf(TEXT("  (not now: %s)"), Reason.IsEmpty() ? TEXT("can't") : *Reason);
		FeedbackText = FString::Printf(TEXT("%s %s%s"), E.bOrder ? TEXT("Order:") : TEXT("Report:"), *E.Ru, bOk ? TEXT("") : *NotNow);
		FeedbackSecondsLeft = 3.0f;
		bMenuOpen = false;
	}
}

void ASimOsaVehicle::OpenCrewMenu(USimWorldSubsystem* Sim)
{
	bMenuOpen = true;
	MenuEntries = Sim->GetCrewMenu(StationId);
	MenuPage = 0;
	const FString Want = Sim->HasTutorial() ? Sim->GetTutorialStep().Order : FString();
	const int32 At = Want.IsEmpty() ? INDEX_NONE : MenuEntries.IndexOfByPredicate([&Want](const FSimCrewMenuEntry& E) { return E.Id == Want; });
	if (At != INDEX_NONE)
	{
		MenuPage = At / 9;
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
	// The lesson autoplay's feet (when no key is down): forward, under 0.4 of full speed, or braking to a halt.
	if (Throttle == 0.0 && AutoThrottle != 0.0 && FMath::Abs(SpeedMps) < 0.4 * MaxSpeedMps)
	{
		Throttle = AutoThrottle;
	}
	bBrake = bBrake || bAutoBrake;
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
	if (Sim->IsTutorialStrict())
	{
		return;  // a lesson gives the orders itself
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
	// The plotter: what he heard on the air-picture net first (the board's numbers), then the tube's tracks.
	TArray<int32> List = Sim->GetPostHeardNumbers();
	List.Append(Sim->GetPostTrackNumbers());
	return List;
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
	if (USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Sim->NoteTutorialEvent(TEXT("pick"));  // a lesson's "pick it" step
	}
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
	if (StationId == Sim->GetCommanderStation())
	{
		// The commander picks a board number with [ ] and gives it to a launcher with 1-5. (The plotter picks his on the
		// board itself, with the mouse wheel: HandleBoard.)
		if (Pc->WasInputKeyJustPressed(EKeys::RightBracket))
		{
			PickTarget(1);
		}
		if (Pc->WasInputKeyJustPressed(EKeys::LeftBracket))
		{
			PickTarget(-1);
		}
		static const FKey Digits[5] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five};
		for (int32 I = 0; I < 5; ++I)
		{
			if (Pc->WasInputKeyJustPressed(Digits[I]))
			{
				DesignatePicked(I);
			}
		}
	}
}

void ASimOsaVehicle::HandleBoard(APlayerController* Pc, const FVector& Start, const FVector& Forward)
{
	// The plotter's hands on the plotting board: the cross on its glass, the mouse wheel turns through the numbers he
	// can write (the reports heard, then the tube's tracks), a click writes the picked one where the cross is.
	bOnBoard = false;
	USimWorldSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Pc == nullptr || Sim == nullptr || !bIsPost || StationId.IsEmpty() || StationId != Sim->GetPlotterStation() || Cabin == nullptr)
	{
		return;
	}
	FVector2D Uv;
	double DistanceCm = 0.0, East = 0.0, North = 0.0, Mpb = 0.0;
	if (!Sim->IsPostBoardOpen() || !Cabin->TraceBoard(Start, Start + Forward * StandingReachCm, Uv, DistanceCm, East, North, Mpb))
	{
		return;
	}
	// The latch on its frame wins (to fold the board).
	if (HoveredItem != INDEX_NONE && Cabin->GetItemControl(HoveredItem) == TEXT("map.board"))
	{
		return;
	}
	bOnBoard = true;
	HoveredItem = INDEX_NONE;
	Cabin->SetHighlightedItem(INDEX_NONE);
	HoverText.Empty();
	if (!PickList().Contains(PickedTarget))
	{
		PickedTarget = INDEX_NONE;
	}
	const int32 Notches = (Pc->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1 : 0) - (Pc->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	if (Notches != 0)
	{
		PickTarget(Notches > 0 ? 1 : -1);
	}
	if (PickedTarget == INDEX_NONE && PickList().Num() > 0)
	{
		PickedTarget = PickList()[0];  // the oldest report waiting first
	}
	if (Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (!IsHoldingSeat())
		{
			FeedbackText = TEXT("Take the seat first: T");
		}
		else if (PickedTarget < 0)
		{
			FeedbackText = TEXT("Nothing to write: listen for a report");
		}
		else
		{
			const float Miss = Sim->PlotPostAt(PickedTarget, East, North, Mpb);
			if (Miss >= 0.0f)
			{
				LastPlotMissCm = Miss;
				LastPlotTime = GetWorld()->GetTimeSeconds();
				FeedbackText = FString::Printf(TEXT("%d written (%.0f cm off)"), PickedTarget, Miss);
				PickedTarget = INDEX_NONE;
			}
			else
			{
				FeedbackText = TEXT("Not now: finish the last mark first");
			}
		}
		FeedbackSecondsLeft = 2.0f;
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
	StandStation = INDEX_NONE;  // in or out, he sits down
	bWalking = false;
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
	if (bWalking)
	{
		CabinCamera->SetRelativeLocationAndRotation(WalkEye, FRotator(WalkPitchDeg, WalkYawDeg, 0.0));
		return;
	}
	FVector Eye;
	FRotator View;
	if (Cabin->GetStationView(Station, Eye, View, StationLabel))
	{
		const FVector Stand = StandStation == Station ? StandOffset : FVector::ZeroVector;
		CabinCamera->SetRelativeLocationAndRotation(
			Eye + Stand, FRotator(FMath::Clamp(View.Pitch + LookPitchDeg, -85.0, 85.0), View.Yaw + LookYawDeg, 0.0));
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
	if (Pc->WasInputKeyJustPressed(StandKey))
	{
		ToggleWalking();
	}
	if (bWalking)
	{
		// His feet: W A S D along where he looks, on the floor the cabin has.
		const double Fwd = (Pc->IsInputKeyDown(EKeys::W) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::S) ? 1.0 : 0.0);
		const double Side = (Pc->IsInputKeyDown(EKeys::D) ? 1.0 : 0.0) - (Pc->IsInputKeyDown(EKeys::A) ? 1.0 : 0.0);
		const FVector Move = FRotator(0.0, WalkYawDeg, 0.0).Vector() * Fwd + FRotator(0.0, WalkYawDeg + 90.0, 0.0).Vector() * Side;
		if (!Move.IsNearlyZero())
		{
			WalkEye = Cabin->WalkStep(WalkEye, WalkEye + Move.GetSafeNormal() * WalkSpeedCmPerS * DeltaSeconds);
		}
	}

	// Mouse: turns the dragged hand wheel, otherwise turns the head.
	float Dx = 0.0f;
	float Dy = 0.0f;
	Pc->GetInputMouseDelta(Dx, Dy);
	// A lesson playing itself (-SimTutorialAutoplay) takes no mouse: a hand on the desk would move its head and its wheels.
	const bool bAutoplaying = !AutoplayFolder.IsEmpty();
	if (bAutoplaying)
	{
		Dx = Dy = 0.0f;
	}
	if (DraggedWheel != INDEX_NONE && (bAutoplaying || !Pc->IsInputKeyDown(EKeys::LeftMouseButton)))
	{
		DraggedWheel = INDEX_NONE;
		bWatchReturn = bWheelWatch;  // let go: the head comes back to the wheel
		bWheelWatch = false;
	}
	const double Ease = 1.0 - FMath::Exp(-10.0 * static_cast<double>(DeltaSeconds));  // about a tenth of a second
	if (DraggedWheel != INDEX_NONE)
	{
		if (Dx != 0.0f && !Cabin->TurnWheel(DraggedWheel, Dx * WheelTurnsPerMouseUnit))
		{
			FeedbackText = TEXT("Locked: release the wheel lock first");
			FeedbackSecondsLeft = 2.0f;
		}
		// Eyes on the display the wheel is worked against (the range gate on the echo, the line on the target).
		if (bWheelWatch)
		{
			LookYawDeg += (WatchYawDeg - LookYawDeg) * Ease;
			LookPitchDeg += (WatchPitchDeg - LookPitchDeg) * Ease;
		}
	}
	else if (bWalking)
	{
		// On his feet he turns all the way round.
		WalkYawDeg = FRotator::NormalizeAxis(WalkYawDeg + Dx * CabinLookDegPerMouseUnit);
		WalkPitchDeg = FMath::Clamp(WalkPitchDeg + Dy * CabinLookDegPerMouseUnit, -80.0, 70.0);
	}
	else
	{
		// Head and shoulders: far enough round to reach the distribution cabinet behind the rear seat.
		LookYawDeg = FMath::Clamp(LookYawDeg + Dx * CabinLookDegPerMouseUnit, -135.0, 135.0);
		LookPitchDeg = FMath::Clamp(LookPitchDeg + Dy * CabinLookDegPerMouseUnit, -70.0, 60.0);
		if (bWatchReturn && (Dx != 0.0f || Dy != 0.0f))
		{
			bWatchReturn = false;  // he looks round himself
		}
		if (bWatchReturn)
		{
			LookYawDeg += (ReturnYawDeg - LookYawDeg) * Ease;
			LookPitchDeg += (ReturnPitchDeg - LookPitchDeg) * Ease;
			bWatchReturn = FMath::Abs(ReturnYawDeg - LookYawDeg) + FMath::Abs(ReturnPitchDeg - LookPitchDeg) > 0.05;
		}
	}
	ApplyStationView();

	// An arm's length for anything; further for this seat's own controls, which the crewman stands and steps to.
	const FVector Start = CabinCamera->GetComponentLocation();
	const FVector Forward = CabinCamera->GetForwardVector();
	int32 Reached = Cabin->TraceItem(Start, Start + Forward * CabinReachCm);
	if (Reached == INDEX_NONE && !StationId.IsEmpty())
	{
		const int32 Far = Cabin->TraceItem(Start, Start + Forward * StandingReachCm);
		Reached = Far != INDEX_NONE && Cabin->GetItemStation(Far) == StationId ? Far : INDEX_NONE;
	}
	HoveredItem = DraggedWheel != INDEX_NONE ? DraggedWheel : Reached;
	Cabin->SetHighlightedItem(HoveredItem);
	HoverText = Cabin->DescribeItem(HoveredItem);
	if (DraggedWheel == INDEX_NONE && AutoplayFolder.IsEmpty())
	{
		HandleBoard(Pc, Start, Forward);
		if (bOnBoard)
		{
			return;
		}
	}
	if (HoveredItem == INDEX_NONE)
	{
		return;
	}
	// A lesson that only lets the step's control work says why another won't move.
	USimWorldSubsystem* LessonSim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const FString Refusal = LessonSim != nullptr ? LessonSim->GetTutorialRefusal(Cabin->GetItemControl(HoveredItem)) : FString();
	auto Refuse = [this, &Refusal]()
	{
		FeedbackText = Refusal;
		FeedbackSecondsLeft = 3.0f;
	};
	if (bAutoplaying)
	{
		return;
	}
	if (Cabin->IsWheel(HoveredItem))
	{
		if (Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			if (Refusal.IsEmpty())
			{
				DraggedWheel = HoveredItem;
				StartWheelWatch(HoveredItem);
			}
			else
			{
				Refuse();
			}
		}
		const double Notches = (Pc->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1.0 : 0.0) -
							   (Pc->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1.0 : 0.0);
		if (Notches != 0.0 && !Refusal.IsEmpty())
		{
			Refuse();
		}
		else if (Notches != 0.0 && !Cabin->TurnWheel(HoveredItem, Notches * WheelTurnsPerScrollNotch))
		{
			FeedbackText = TEXT("Locked: release the wheel lock first");
			FeedbackSecondsLeft = 2.0f;
		}
		return;
	}
	const bool bLeft = Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bRight = Pc->WasInputKeyJustPressed(EKeys::RightMouseButton);
	if ((bLeft || bRight) && !Refusal.IsEmpty())
	{
		Refuse();
	}
	else if (bLeft)
	{
		ShowFeedback(Cabin->ClickItem(HoveredItem, 1));
	}
	else if (bRight)
	{
		ShowFeedback(Cabin->ClickItem(HoveredItem, -1));
	}
}

bool ASimOsaVehicle::LookAnglesTo(const FVector& World, double& OutYawDeg, double& OutPitchDeg) const
{
	FVector Eye;
	FRotator View;
	FString Label;
	if (!Cabin->GetStationView(Station, Eye, View, Label))
	{
		return false;
	}
	const FVector Stand = StandStation == Station ? StandOffset : FVector::ZeroVector;
	const FRotator Want = (GetActorTransform().InverseTransformPosition(World) - Eye - Stand).Rotation();
	OutYawDeg = FMath::Clamp(FRotator::NormalizeAxis(Want.Yaw - View.Yaw), -135.0, 135.0);
	OutPitchDeg = FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch - View.Pitch), -70.0, 60.0);
	return true;
}

void ASimOsaVehicle::StartWheelWatch(int32 Item)
{
	bWheelWatch = false;
	bWatchReturn = false;
	const FString Display = Cabin->GetWheelWatch(Item);
	FVector World;
	FVector Facing;
	FString Label;
	if (Display.IsEmpty() || !Cabin->GetHighlightTarget(Display, World, Label, Facing) || !LookAnglesTo(World, WatchYawDeg, WatchPitchDeg))
	{
		return;
	}
	ReturnYawDeg = LookYawDeg;
	ReturnPitchDeg = LookPitchDeg;
	bWheelWatch = true;
}

void ASimOsaVehicle::ToggleWalking()
{
	if (bWalking)
	{
		const int32 Seat = Cabin->SeatNear(WalkEye, 60.0);
		if (Seat == INDEX_NONE)
		{
			FeedbackText = TEXT("No seat here: walk up to one, then X sits you down (Tab: back to a seat)");
			FeedbackSecondsLeft = 3.0f;
			return;
		}
		bWalking = false;
		Station = Seat;
		LookYawDeg = 0.0;
		LookPitchDeg = 0.0;
		ApplyStationView();
		FeedbackText = FString::Printf(TEXT("Sat down: %s%s"), *StationLabel, TEXT("   (T takes the seat over)"));
		FeedbackSecondsLeft = 3.0f;
		return;
	}
	FVector Eye;
	FVector SeatEye;
	FRotator View;
	FString Label;
	if (!Cabin->GetStandUpPoint(Station, Eye) || !Cabin->GetStationView(Station, SeatEye, View, Label))
	{
		FeedbackText = TEXT("No room to get up here");
		FeedbackSecondsLeft = 2.0f;
		return;
	}
	bWalking = true;
	StandStation = INDEX_NONE;
	DraggedWheel = INDEX_NONE;
	bWheelWatch = bWatchReturn = false;
	WalkEye = Eye;
	WalkYawDeg = FRotator::NormalizeAxis(View.Yaw + LookYawDeg);
	WalkPitchDeg = FMath::Clamp(View.Pitch + LookPitchDeg, -80.0, 70.0);
	ApplyStationView();
	FeedbackText = TEXT("On your feet: W A S D walk, X sits you down at a seat");
	FeedbackSecondsLeft = 3.0f;
}

void ASimOsaVehicle::NextStation()
{
	bWalking = false;
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
