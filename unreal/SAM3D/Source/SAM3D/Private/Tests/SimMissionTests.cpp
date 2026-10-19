#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimCabinComponent.h"
#include "SimMissionMenuHUD.h"
#include "SimOsaVehicle.h"
#include "SimViewComponent.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The missions menu (roadmap M2): the catalogue reaches Unreal with every mission's vehicles and seats; a mission chosen
// in it comes back as the level's options, which put the player in that vehicle (a battery link) and that seat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimMissionsMenuTest, "SAM3D.Missions.Menu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimMissionsMenuTest::RunTest(const FString& Parameters)
{
	const FString Options = ASimMissionMenuHUD::OptionsFor(TEXT("battery_link_practice"), TEXT("post"), TEXT("battery_commander"));
	TestEqual(TEXT("the options"), Options, FString(TEXT("SimMission=battery_link_practice?SimVehicle=post?SimSeat=battery_commander")));
	TestEqual(TEXT("outside, no vehicle to choose"), ASimMissionMenuHUD::OptionsFor(TEXT("osa_practice"), FString(), FString()),
		FString(TEXT("SimMission=osa_practice")));
	TestEqual(TEXT("the first lesson, with its guide"),
		ASimMissionMenuHUD::OptionsFor(TEXT("tutorial_basics"), FString(), FString(), TEXT("tutorials/basics.json")),
		FString(TEXT("SimMission=tutorial_basics?SimTutorial=tutorials/basics.json")));
	TestEqual(TEXT("a seat lesson, in its seat"),
		ASimMissionMenuHUD::OptionsFor(TEXT("school_operator"), FString(), TEXT("operator"), TEXT("tutorials/school_operator.json")),
		FString(TEXT("SimMission=school_operator?SimSeat=operator?SimTutorial=tutorials/school_operator.json")));

	FURL Url(nullptr, *(FString(TEXT("OsaPractice?")) + Options), TRAVEL_Absolute);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DMissionsTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->URL = Url;
	World->InitializeActorsForPlay(Url);
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	TestNotNull(TEXT("the subsystem"), Sim);
	if (Sim == nullptr)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}

	TArray<FSimMission> Missions;
	FString Error;
	TestTrue(FString::Printf(TEXT("the catalogue loads %s"), *Error), Sim->GetMissions(Missions, Error));
	TestEqual(TEXT("eleven lessons (the Osa's and the PU-12M's), sixteen missions, six of ranges and history, the air war's seven sample raids and the test range's map"),
		Missions.Num(), 11 + 16 + 6 + 7 + 1);
	TestTrue(TEXT("the first lesson first, starting outside, with its guide"), Missions.Num() > 0 && Missions[0].Id == TEXT("tutorial_basics") &&
		Missions[0].Start == TEXT("outside") && Missions[0].Tutorial == TEXT("tutorials/basics.json"));
	// Then a lesson for each seat of the Osa, each starting in its seat.
	const TCHAR* Seats[] = {TEXT("driver"), TEXT("operator"), TEXT("senior_operator"), TEXT("deputy_commander"), TEXT("commander")};
	for (int32 I = 0; I < 5; ++I)
	{
		const FString Want = FString(TEXT("school_")) + Seats[I];
		TestTrue(FString::Printf(TEXT("seat lesson %d: %s"), I + 1, *Want), Missions.IsValidIndex(I + 1) && Missions[I + 1].Id == Want &&
			Missions[I + 1].Start == Seats[I] && Missions[I + 1].Tutorial == FString::Printf(TEXT("tutorials/%s.json"), *Want));
	}
	const FSimMission* Link = Missions.FindByPredicate([](const FSimMission& M) { return M.Id == TEXT("battery_link_practice"); });
	TestTrue(TEXT("the battery link, with the Osa and the PU-12M to start in"), Link != nullptr && Link->bLink && Link->Vehicles.Num() == 2);
	if (Link != nullptr && Link->Vehicles.Num() == 2)
	{
		TestEqual(TEXT("the post's seats"), Link->Vehicles[1].Seats.Num(), 5);
		const FSimMissionSeat* Cmdr = Link->Vehicles[1].Seats.FindByPredicate([](const FSimMissionSeat& S) { return S.bCommands; });
		TestTrue(TEXT("the battery commander commands"), Cmdr != nullptr && Cmdr->Id == TEXT("battery_commander"));
	}

	TestTrue(TEXT("?SimMission"), Sim->HasUrlOption(TEXT("SimMission")));
	TestFalse(TEXT("no ?SimMenu"), Sim->HasUrlOption(TEXT("SimMenu")));
	TestEqual(TEXT("?SimVehicle"), Sim->GetLaunchOption(TEXT("SimVehicle")), FString(TEXT("post")));
	TestTrue(FString::Printf(TEXT("the link loads %s"), *Error), Sim->LoadScenario(TEXT("scenarios/battery_link_practice.json"), Error));
	TestTrue(TEXT("?SimVehicle=post: the player in the PU-12M"), Sim->IsCommandPost());
	TestTrue(FString::Printf(TEXT("its cabin loads %s"), *Error), Sim->LoadCabin(Sim->GetDefaultCabinFile(), Error));

	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>(Sim->GetUnitLocation(), FRotator::ZeroRotator);
	TestNotNull(TEXT("the player's vehicle"), Car);
	if (Car != nullptr)
	{
		Car->StartInSeat(TEXT("battery_commander"));
		for (int32 I = 0; I < 10; ++I)
		{
			Sim->Tick(0.05f);
			Car->Tick(0.05f);
		}
		TestTrue(TEXT("in the cabin"), Car->IsInCabin());
		TestEqual(TEXT("at the battery commander's seat"), Car->GetStationId(), FString(TEXT("battery_commander")));
		TestEqual(TEXT("the seat is the player's"), Sim->GetCrewSeatHolder(TEXT("battery_commander")), FString(TEXT("Player")));
		// The mission's menu pauses the sim (single player) and gives it back.
		const double T0 = Sim->GetSimTimeSeconds();
		Car->SetMissionMenuOpen(true);
		for (int32 I = 0; I < 20; ++I)
		{
			Sim->Tick(0.05f);
		}
		TestTrue(TEXT("paused"), Sim->IsSimPaused() && Sim->GetSimTimeSeconds() - T0 < 1e-6);
		Car->SetMissionMenuOpen(false);
		for (int32 I = 0; I < 20; ++I)
		{
			Sim->Tick(0.05f);
		}
		TestTrue(TEXT("running again"), !Sim->IsSimPaused() && Sim->GetSimTimeSeconds() - T0 > 0.5);
		// Up from the seat and down again (X): the battery commander stands in the knee room in front of the bench.
		Car->ToggleWalking();
		TestTrue(TEXT("on his feet"), Car->IsWalking());
		Car->ToggleWalking();
		TestTrue(TEXT("back in his seat"), !Car->IsWalking() && Car->GetStationId() == TEXT("battery_commander"));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// The first lesson (docs/TUTORIALS.md): a free lesson; Enter reads a step, the camera's moves finish the next ones, N skips
// one, and the camera views find what they look for (F8 the friendly pair on patrol, F6 nothing before a launch).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimTutorialGuideTest, "SAM3D.Missions.Tutorial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimTutorialGuideTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DTutorialTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FURL Url(nullptr, TEXT("KesmarkRange"), TRAVEL_Absolute);
	World->InitializeActorsForPlay(Url);
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	if (Sim == nullptr || !Sim->LoadScenario(TEXT("scenarios/tutorial_basics.json"), Error))
	{
		AddError(FString::Printf(TEXT("the first lesson's scenario: %s"), *Error));
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	TestFalse(TEXT("a guide that isn't there is refused"), Sim->LoadTutorial(TEXT("tutorials/no_such_guide.json"), Error));
	TestTrue(FString::Printf(TEXT("the guide loads %s"), *Error), Sim->LoadTutorial(TEXT("tutorials/basics.json"), Error));
	FSimTutorialStep Step = Sim->GetTutorialStep();
	TestTrue(TEXT("step 1 of 21: the welcome, to read"), Step.Number == 1 && Step.Count == 21 && Step.Title == TEXT("Welcome to SAM3D") &&
		Step.Lines.Num() > 0 && Step.Phase == ESimTutorialPhase::Read);
	TestTrue(TEXT("a free lesson: N may skip"), Sim->CanSkipTutorialStep() && !Sim->IsTutorialStrict());
	Sim->Tick(0.05f);
	TestEqual(TEXT("it waits to be read"), Sim->GetTutorialStep().Number, 1);
	Sim->NoteTutorialContinue();
	Sim->Tick(0.05f);
	Step = Sim->GetTutorialStep();
	TestTrue(TEXT("Enter: on to turning the camera, to do"), Step.Title == TEXT("Turn the camera") && Step.Phase == ESimTutorialPhase::Act);
	Sim->NoteTutorialEvent(TEXT("orbit"));
	Sim->Tick(0.05f);
	TestEqual(TEXT("turning the camera finishes it"), Sim->GetTutorialStep().Title, FString(TEXT("Closer and further")));
	TestEqual(TEXT("and says so"), Sim->GetTutorialStep().LastDone, FString(TEXT("Turn the camera")));
	Sim->NoteTutorialEvent(TEXT("zoom"));
	Sim->Tick(0.05f);
	Sim->NoteTutorialContinue();
	Sim->Tick(0.05f);
	TestEqual(TEXT("then F8"), Sim->GetTutorialStep().Title, FString(TEXT("F8: the aircraft")));

	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>(Sim->GetUnitLocation(), FRotator::ZeroRotator);
	USimViewComponent* Views = Car != nullptr ? Car->GetViews() : nullptr;
	TestNotNull(TEXT("the vehicle's views"), Views);
	if (Views != nullptr)
	{
		TestEqual(TEXT("it starts round the vehicle"), Views->GetViewKey(), FString(TEXT("F2")));
		Views->SetView(ESimView::Air);
		TestEqual(TEXT("the pair is up: F8"), Views->GetViewKey(), FString(TEXT("F8")));
		TestTrue(FString::Printf(TEXT("F8 follows the pair (%s)"), *Views->GetViewLabel()), Views->GetViewLabel().Contains(TEXT("MiG-21")));
		Sim->SetTutorialView(TEXT("F8"), false, FString(), FString());
		Sim->Tick(0.05f);
		TestEqual(TEXT("F8 done: on to the chase"), Sim->GetTutorialStep().Title, FString(TEXT("F4: the chase")));
		Views->SetView(ESimView::Weapon);
		TestEqual(TEXT("no missile: F6 stays on F8"), Views->GetViewKey(), FString(TEXT("F8")));
		TestTrue(TEXT("and says so"), Views->GetViewMessage().Contains(TEXT("No missile")));
		TestTrue(TEXT("a view by its key"), Views->SetViewByKey(TEXT("F10")) && Views->GetViewKey() == TEXT("F10"));
	}
	Sim->SkipTutorialStep();
	TestEqual(TEXT("N skips a step"), Sim->GetTutorialStep().Title, FString(TEXT("F7: the battery's vehicles")));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// A seat lesson (docs/TUTORIALS.md): strict, in its seat; Enter through the tour, then the highlighted switch is the only
// one that works (the others are refused with the reason), the cabin finds it for the marker, and setting it moves on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSeatLessonTest, "SAM3D.Missions.SeatLesson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimSeatLessonTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DSeatLessonTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FURL Url(nullptr, TEXT("KesmarkRange"), TRAVEL_Absolute);
	World->InitializeActorsForPlay(Url);
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	if (Sim == nullptr || !Sim->LoadScenario(TEXT("scenarios/school_senior_operator.json"), Error) ||
		!Sim->LoadCabin(Sim->GetDefaultCabinFile(), Error) || !Sim->LoadTutorial(TEXT("tutorials/school_senior_operator.json"), Error))
	{
		AddError(FString::Printf(TEXT("the senior operator's lesson: %s"), *Error));
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>(Sim->GetUnitLocation(), FRotator::ZeroRotator);
	TestNotNull(TEXT("the player's vehicle"), Car);
	if (Car == nullptr)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	Car->StartInSeat(TEXT("senior_operator"));
	// The cabin builds its parts on its own tick (in the game, every frame).
	auto Run = [Sim, Car](int32 Ticks)
	{
		for (int32 I = 0; I < Ticks; ++I)
		{
			Sim->Tick(0.05f);
			Car->Tick(0.05f);
			Car->GetCabin()->TickComponent(0.05f, LEVELTICK_All, nullptr);
		}
	};
	Run(10);
	TestEqual(TEXT("in the senior operator's seat"), Car->GetStationId(), FString(TEXT("senior_operator")));
	TestTrue(TEXT("a strict lesson, which may be skipped"), Sim->IsTutorialStrict() && Sim->CanSkipTutorialStep());
	FSimTutorialStep Step = Sim->GetTutorialStep();
	TestTrue(TEXT("24 steps, the first to read, for this seat"), Step.Count == 24 && Step.Phase == ESimTutorialPhase::Read &&
		Step.Seat == TEXT("senior_operator"));
	// The tour and the commander's order: five steps to read.
	for (int32 I = 0; I < 5; ++I)
	{
		Sim->NoteTutorialContinue();
		Run(1);
	}
	// The first switch, once the order has been heard.
	for (int32 I = 0; I < 600 && Sim->GetTutorialStep().Phase != ESimTutorialPhase::Act; ++I)
	{
		Run(1);
	}
	Step = Sim->GetTutorialStep();
	TestTrue(FString::Printf(TEXT("step 6: ПОИСК (%s, %s)"), *Step.Title, *Step.WaitText),
		Step.Number == 6 && Step.Phase == ESimTutorialPhase::Act && Step.Control == TEXT("soc.search") && Step.Position == 1 &&
		!Step.ControlLabel.IsEmpty() && !Step.PositionLabel.IsEmpty());
	TestFalse(TEXT("another switch is refused, with the reason"), Sim->GetTutorialRefusal(TEXT("soc.hv")).IsEmpty());
	TestTrue(TEXT("the step's switch is not"), Sim->GetTutorialRefusal(TEXT("soc.search")).IsEmpty());
	TestFalse(TEXT("nor is the crew menu open to other orders"), Sim->GetTutorialMenuRefusal(TEXT("contact")).IsEmpty());
	FVector Where;
	FVector Facing;
	FString Label;
	TestTrue(TEXT("the cabin finds the switch for the marker"),
		Car->GetCabin() != nullptr && Car->GetCabin()->GetHighlightTarget(TEXT("soc.search"), Where, Label, Facing) && !Label.IsEmpty());
	Sim->PlayerSetControl(TEXT("soc.search"), 1);
	Run(2);
	Step = Sim->GetTutorialStep();
	TestTrue(FString::Printf(TEXT("set: on to the guard (%s)"), *Step.Title), Step.Number == 7 && Step.Control == TEXT("soc.hv_cover"));
	// Backspace: step 6 again, to read; Enter: on again.
	Sim->BackTutorialStep();
	Step = Sim->GetTutorialStep();
	TestTrue(TEXT("back: step 6 to read again"), Step.Number == 6 && Step.bReview && Step.Phase == ESimTutorialPhase::Read);
	Sim->NoteTutorialContinue();
	Run(1);
	TestEqual(TEXT("Enter: on to step 7"), Sim->GetTutorialStep().Number, 7);
	// N: the instructor lifts the guard.
	Sim->SkipTutorialStep();
	Run(2);
	TestTrue(TEXT("skipped: the instructor lifted the guard"), Sim->GetTutorialStep().Number == 8 && Sim->GetControlPosition(TEXT("soc.hv_cover")) == 1);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
