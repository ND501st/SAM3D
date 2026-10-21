#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimAirCampaignSubsystem.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The air war (docs/air_war/, roadmap A10): a campaign's raid reaches Unreal as a scenario file the world subsystem plays
// like any mission, and the level options name the raid and the seat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimAirWarRaidTest, "SAM3D.AirWar.Raid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimAirWarRaidTest::RunTest(const FString& Parameters)
{
	using namespace simcore::airwar;
	TestEqual(TEXT("the options"), USimAirCampaignSubsystem::RaidOptions(12, TEXT("commander")), FString(TEXT("SimAirWarRaid=12?SimSeat=commander")));

	const FString DataDir = USimAirCampaignSubsystem::DataDirectory();
	AirWarData Data;
	std::string LoadError;
	const bool bLoaded = LoadAirWarData(std::string(TCHAR_TO_UTF8(*DataDir)), "air_war/theatre_pannonia.json", "air_war/faction_nato.json", Data, &LoadError);
	TestTrue(FString::Printf(TEXT("the campaign data load %s"), UTF8_TO_TCHAR(LoadError.c_str())), bLoaded);
	if (!bLoaded)
	{
		return true;
	}
	AirCampaign Campaign;
	TestTrue(TEXT("the campaign starts"), Campaign.Init(Data, 1));
	Campaign.Posture().emission = 0.1;
	Campaign.Posture().relocate_every_h = 20.0;
	bool bRaid = false;
	for (int32 Day = 0; Day < 6 && !bRaid; ++Day)
	{
		bRaid = Campaign.Advance(Campaign.Now() + 24.0);
	}
	TestTrue(TEXT("a raid comes through the battery's sector within six days"), bRaid);
	if (!bRaid)
	{
		return true;
	}
	// First person (docs/command/): the campaign's news reaches the player's character through the chain of command.
	{
		simcore::command::ChainData Chain;
		std::string ChainError;
		const bool bChain = simcore::command::LoadChainData(std::string(TCHAR_TO_UTF8(*DataDir)), "command/chain_pannonia.json", Chain, &ChainError);
		TestTrue(FString::Printf(TEXT("the chain of command loads %s"), UTF8_TO_TCHAR(ChainError.c_str())), bChain);
		if (bChain)
		{
			simcore::command::InfoNet Net;
			TestTrue(TEXT("the information pipeline starts"), Net.Init(Chain, 1u, Data.theatre.start_hour));
			simcore::command::CampaignFeed Feed;
			Feed.Bind(&Net);
			Feed.Pump(Campaign);
			const std::vector<simcore::command::Delivery> Known = Net.KnownBy(Net.Player());
			TestTrue(TEXT("the player's character has heard something, from someone"), !Known.empty() && !Net.Describe(Known.front()).empty());
			TestTrue(TEXT("but not everything the organisation knows"), Known.size() < Net.Facts().size());
		}
	}
	FString Relative;
	FString Error;
	RaidManifest Manifest;
	TestTrue(FString::Printf(TEXT("the raid's scenario is written %s"), *Error),
		USimAirCampaignSubsystem::WriteRaidScenario(Campaign, *Campaign.PlayerMission(), DataDir, Relative, Manifest, Error));
	TestTrue(TEXT("the manifest has the raid's aircraft"), Manifest.aircraft.size() > 0 && Manifest.package_id == Campaign.PlayerMission()->id);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DAirWarTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FURL Url(nullptr, *FString::Printf(TEXT("OsaPractice?%s"), *USimAirCampaignSubsystem::RaidOptions(Manifest.package_id, TEXT("commander"))), TRAVEL_Absolute);
	World->URL = Url;
	World->InitializeActorsForPlay(Url);
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	TestNotNull(TEXT("the subsystem"), Sim);
	if (Sim != nullptr)
	{
		TestTrue(TEXT("?SimAirWarRaid"), Sim->HasUrlOption(TEXT("SimAirWarRaid")));
		TestTrue(FString::Printf(TEXT("the raid loads from %s %s"), *Relative, *Error), Sim->LoadScenario(Relative, Error));
		TestTrue(TEXT("and runs"), Sim->IsScenarioRunning());
		for (int32 I = 0; I < 20; ++I)
		{
			Sim->Tick(0.05f);
		}
		// The bridge reads the running scenario.
		if (simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner())
		{
			MissionBridge Bridge(Manifest, Campaign.Data(), 1u);
			Bridge.Step(*Runner);
			const PackageOutcome Outcome = Bridge.Finish(*Runner);
			TestEqual(TEXT("the outcome is the raid's"), Outcome.package_id, Manifest.package_id);
			Campaign.CompletePlayerMission(Outcome, Manifest.start_h + Runner->Time() / 3600.0);
			TestFalse(TEXT("the campaign goes on"), Campaign.HasPlayerMission());
			// Saved and loaded (the seed and the actions replayed), it is the same campaign.
			const std::string Saved = SaveCampaignJson(Campaign, "air_war/theatre_pannonia.json", "air_war/faction_nato.json");
			AirCampaign Loaded;
			std::string SaveError;
			TestTrue(FString::Printf(TEXT("the campaign saves and loads %s"), UTF8_TO_TCHAR(SaveError.c_str())),
				LoadCampaignJson(Saved, std::string(TCHAR_TO_UTF8(*DataDir)), Loaded, &SaveError));
			TestTrue(TEXT("to the same hour and journal"), Loaded.Now() == Campaign.Now() && Loaded.Journal().size() == Campaign.Journal().size());
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
