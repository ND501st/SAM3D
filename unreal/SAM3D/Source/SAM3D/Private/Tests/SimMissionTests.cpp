#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
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
	TestEqual(TEXT("the tutorial, with its guide"),
		ASimMissionMenuHUD::OptionsFor(TEXT("tutorial_first_shoot"), FString(), FString(), TEXT("tutorials/first_shoot.json")),
		FString(TEXT("SimMission=tutorial_first_shoot?SimTutorial=tutorials/first_shoot.json")));

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
	TestEqual(TEXT("the tutorial, sixteen missions, six of ranges and history, the air war's seven sample raids and the test range's map"),
		Missions.Num(), 1 + 16 + 6 + 7 + 1);
	TestTrue(TEXT("the tutorial first, starting outside, with its guide"), Missions.Num() > 0 && Missions[0].Id == TEXT("tutorial_first_shoot") &&
		Missions[0].Start == TEXT("outside") && Missions[0].Tutorial == TEXT("tutorials/first_shoot.json"));
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
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// The first playable test (docs/PLAYING.md): the tutorial's guide over its scenario moves on with what the player does,
// N skips a step, and the camera views find what they look for (F8 the drone once it is up, F6 nothing before a launch).
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
	if (Sim == nullptr || !Sim->LoadScenario(TEXT("scenarios/tutorial_first_shoot.json"), Error))
	{
		AddError(FString::Printf(TEXT("the tutorial's scenario: %s"), *Error));
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	TestFalse(TEXT("a guide that isn't there is refused"), Sim->LoadTutorial(TEXT("tutorials/no_such_guide.json"), Error));
	TestTrue(FString::Printf(TEXT("the guide loads %s"), *Error), Sim->LoadTutorial(TEXT("tutorials/first_shoot.json"), Error));
	FSimTutorialStep Step = Sim->GetTutorialStep();
	TestTrue(TEXT("step 1 of 12: the welcome"), Step.Number == 1 && Step.Count == 12 && Step.Title == TEXT("Welcome to SAM3D") && Step.Lines.Num() > 0);
	Sim->NoteTutorialEvent(TEXT("orbit"));
	Sim->Tick(0.05f);
	TestEqual(TEXT("turning the camera finishes the welcome"), Sim->GetTutorialStep().Title, FString(TEXT("The camera views (F1 to F11)")));

	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>(Sim->GetUnitLocation(), FRotator::ZeroRotator);
	USimViewComponent* Views = Car != nullptr ? Car->GetViews() : nullptr;
	TestNotNull(TEXT("the vehicle's views"), Views);
	if (Views != nullptr)
	{
		TestEqual(TEXT("it starts round the vehicle"), Views->GetViewKey(), FString(TEXT("F2")));
		Views->SetView(ESimView::Air);
		TestEqual(TEXT("no aircraft yet: F8 stays where it was"), Views->GetViewKey(), FString(TEXT("F2")));
		for (int32 I = 0; I < 140; ++I)
		{
			Sim->Tick(0.05f);
		}
		Views->SetView(ESimView::Air);
		TestEqual(TEXT("the drone is up: F8"), Views->GetViewKey(), FString(TEXT("F8")));
		TestTrue(FString::Printf(TEXT("F8 follows the drone (%s)"), *Views->GetViewLabel()), Views->GetViewLabel().Contains(TEXT("Practice drone 1")));
		Views->SetView(ESimView::Weapon);
		TestEqual(TEXT("no missile: F6 stays on F8"), Views->GetViewKey(), FString(TEXT("F8")));
		TestTrue(TEXT("and says so"), Views->GetViewMessage().Contains(TEXT("No missile")));
		Views->SetView(ESimView::Map);
		TestEqual(TEXT("F10 the map"), Views->GetViewKey(), FString(TEXT("F10")));
	}
	Sim->SkipTutorialStep();
	TestEqual(TEXT("N skips to the cabin"), Sim->GetTutorialStep().Title, FString(TEXT("Into the cabin")));
	Sim->SetTutorialView(TEXT("F1"), true, TEXT("commander"), FString());
	Sim->Tick(0.05f);
	TestEqual(TEXT("at the commander's seat: take command"), Sim->GetTutorialStep().Title, FString(TEXT("Take command")));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
