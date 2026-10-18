#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimMissionMenuHUD.h"
#include "SimOsaVehicle.h"
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
	TestEqual(TEXT("seventeen missions and six of ranges and history"), Missions.Num(), 17 + 6);
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

#endif
