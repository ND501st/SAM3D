#include "SimAirCampaignSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimWorldSubsystem.h"

namespace
{
	std::string AirWarToStd(const FString& S)
	{
		return std::string(TCHAR_TO_UTF8(*S));
	}

	FString AirWarToF(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	/** Seconds the debrief of a finished raid stays on screen before the campaign screen comes back. */
	constexpr float kReturnAfterRaidS = 20.0f;
}

USimAirCampaignSubsystem::USimAirCampaignSubsystem() = default;

void USimAirCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CleanupHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &USimAirCampaignSubsystem::HandleWorldTearDown);
}

void USimAirCampaignSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(CleanupHandle);
	Bridge.Reset();
	Manifest.Reset();
	Campaign.Reset();
	Super::Deinitialize();
}

TStatId USimAirCampaignSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimAirCampaignSubsystem, STATGROUP_Tickables);
}

UWorld* USimAirCampaignSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* Gi = GetGameInstance();
	return Gi != nullptr ? Gi->GetWorld() : nullptr;
}

FString USimAirCampaignSubsystem::DataDirectory()
{
	// The same resolution as USimWorldSubsystem: <repo>/unreal/SAM3D/SAM3D.uproject -> <repo>/data.
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("data")));
}

bool USimAirCampaignSubsystem::NewCampaign(const FString& InFactionId, int32 Seed, FString& OutError)
{
	using namespace simcore::airwar;
	const FString DataDir = DataDirectory();
	AirWarData Data;
	std::string Error;
	if (!LoadAirWarData(AirWarToStd(DataDir), "air_war/theatre_pannonia.json", "air_war/faction_" + AirWarToStd(InFactionId) + ".json", Data, &Error))
	{
		OutError = AirWarToF(Error);
		UE_LOG(LogSAM3D, Error, TEXT("Air war: the campaign data don't load: %s"), *OutError);
		return false;
	}
	TUniquePtr<AirCampaign> New = MakeUnique<AirCampaign>();
	if (!New->Init(Data, static_cast<unsigned>(FMath::Max(1, Seed)), &Error))
	{
		OutError = AirWarToF(Error);
		return false;
	}
	Campaign = MoveTemp(New);
	FactionId = InFactionId;
	CampaignSeed = FMath::Max(1, Seed);
	bRaidActive = false;
	bBridgeStarted = false;
	bEnded = false;
	Bridge.Reset();
	Manifest.Reset();
	LastRaidReport.Reset();
	ReturnCountdown = -1.0f;
	UE_LOG(LogSAM3D, Log, TEXT("Air war: a campaign against %s (seed %d)"), *AirWarToF(Campaign->Data().faction.name), CampaignSeed);
	StartInfoNet();
	Stepped();
	return true;
}

void USimAirCampaignSubsystem::StartInfoNet()
{
	Net.Reset();
	Feed.Reset();
	if (!Campaign.IsValid())
	{
		return;
	}
	simcore::command::ChainData Chain;
	std::string Error;
	if (!simcore::command::LoadChainData(AirWarToStd(DataDirectory()), "command/chain_pannonia.json", Chain, &Error))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Air war: the chain of command doesn't load (%s): no news reaches the player's character"), *AirWarToF(Error));
		return;
	}
	TUniquePtr<simcore::command::InfoNet> NewNet = MakeUnique<simcore::command::InfoNet>();
	if (!NewNet->Init(Chain, Campaign->Seed(), Campaign->Data().theatre.start_hour, &Error))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Air war: the information pipeline doesn't start: %s"), *AirWarToF(Error));
		return;
	}
	Net = MoveTemp(NewNet);
	Feed = MakeUnique<simcore::command::CampaignFeed>();
	Feed->Bind(Net.Get());
	Feed->Pump(*Campaign);
}

void USimAirCampaignSubsystem::Stepped()
{
	if (Net.IsValid() && Feed.IsValid() && Campaign.IsValid())
	{
		Feed->Pump(*Campaign);
	}
	SaveCampaign();
}

FString USimAirCampaignSubsystem::GetPlayerPost() const
{
	if (!Net.IsValid() || Net->Player() < 0)
	{
		return FString();
	}
	return AirWarToF(Net->Data().positions[static_cast<std::size_t>(Net->Player())].id);
}

FString USimAirCampaignSubsystem::SaveFile()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AirWar"), TEXT("campaign.json")));
}

bool USimAirCampaignSubsystem::SaveCampaign()
{
	if (!Campaign.IsValid())
	{
		return false;
	}
	const std::string Text = simcore::airwar::SaveCampaignJson(*Campaign, "air_war/theatre_pannonia.json", "air_war/faction_" + AirWarToStd(FactionId) + ".json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SaveFile()), true);
	FString Body = AirWarToF(Text);
	if (bEnded)
	{
		Body = Body.Replace(TEXT("\"save_version\""), TEXT("\"ended\": true, \"save_version\""));
	}
	return FFileHelper::SaveStringToFile(Body, *SaveFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool USimAirCampaignSubsystem::HasSavedCampaign() const
{
	return FPaths::FileExists(SaveFile());
}

FString USimAirCampaignSubsystem::GetSavedCampaignLabel() const
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		return FString();
	}
	// "faction_file": "air_war/faction_nato.json", "clock": "D4 12:00"
	auto Field = [&](const TCHAR* Key)
	{
		const FString Pattern = FString::Printf(TEXT("\"%s\": \""), Key);
		const int32 At = Text.Find(Pattern);
		if (At == INDEX_NONE)
		{
			return FString();
		}
		const int32 Start = At + Pattern.Len();
		const int32 End = Text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		return End > Start ? Text.Mid(Start, End - Start) : FString();
	};
	FString Faction = Field(TEXT("faction_file"));
	Faction = Faction.Replace(TEXT("air_war/faction_"), TEXT("")).Replace(TEXT(".json"), TEXT(""));
	return FString::Printf(TEXT("%s, %s"), *Faction, *Field(TEXT("clock")));
}

bool USimAirCampaignSubsystem::LoadSavedCampaign(FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		OutError = TEXT("no saved campaign");
		return false;
	}
	TUniquePtr<simcore::airwar::AirCampaign> New = MakeUnique<simcore::airwar::AirCampaign>();
	std::string Error;
	std::string FactionFile;
	if (!simcore::airwar::LoadCampaignJson(AirWarToStd(Text), AirWarToStd(DataDirectory()), *New, &Error, nullptr, &FactionFile))
	{
		OutError = AirWarToF(Error);
		UE_LOG(LogSAM3D, Error, TEXT("Air war: the saved campaign does not load: %s"), *OutError);
		return false;
	}
	Campaign = MoveTemp(New);
	FactionId = AirWarToF(FactionFile).Replace(TEXT("air_war/faction_"), TEXT("")).Replace(TEXT(".json"), TEXT(""));
	CampaignSeed = static_cast<int32>(Campaign->Seed());
	bRaidActive = false;
	bBridgeStarted = false;
	bEnded = Text.Contains(TEXT("\"ended\": true"));
	Bridge.Reset();
	Manifest.Reset();
	LastRaidReport = TEXT("Campaign loaded.");
	ReturnCountdown = -1.0f;
	StartInfoNet();
	UE_LOG(LogSAM3D, Log, TEXT("Air war: loaded the campaign against %s at %s"), *FactionId, *AirWarToF(Campaign->Clock()));
	return true;
}

bool USimAirCampaignSubsystem::HasCampaign() const
{
	return Campaign.IsValid();
}

bool USimAirCampaignSubsystem::AdvanceToNextRaid(float MaxHours)
{
	if (!Campaign.IsValid() || bRaidActive || bEnded)
	{
		return false;
	}
	if (Campaign->HasPlayerMission())
	{
		return true;
	}
	const bool bRaid = Campaign->Advance(Campaign->Now() + FMath::Max(0.1f, MaxHours));
	Stepped();
	return bRaid;
}

bool USimAirCampaignSubsystem::HasPendingRaid() const
{
	return Campaign.IsValid() && Campaign->HasPlayerMission();
}

FString USimAirCampaignSubsystem::GetPendingRaidWarning() const
{
	if (!HasPendingRaid())
	{
		return FString();
	}
	const simcore::airwar::Package* P = Campaign->PlayerMission();
	const FString Over = AirWarToF(simcore::airwar::FormatHour(P->tot_h, Campaign->Data().theatre.start_hour));
	const FString What = AirWarToF(simcore::airwar::RaidScenarioBuilder::DescribeForPlayer(*P));
	// First person: the warning is what reaches the player's character down the chain (the national sector, the
	// division's warning net, the regiment's duty officer, the battery), by whom and when.
	if (Net.IsValid())
	{
		const int32 Fact = Net->FindFact(simcore::command::FactKind::AirWarning, "package:" + std::to_string(P->id));
		simcore::command::Delivery D;
		if (Fact >= 0 && Net->PreviewDelivery(Fact, Net->Player(), P->tot_h, D))
		{
			return FString::Printf(TEXT("'Vozdukh!' %s: a %s, expected over the sector about %s."), *AirWarToF(Net->Describe(D)), *What, *Over);
		}
		return TEXT("A raid is coming through the sector, and no warning reached you in time.");
	}
	return FString::Printf(TEXT("%s: the warning net reports a %s. Expected over the sector about %s."), *AirWarToF(Campaign->Clock()), *What, *Over);
}

FString USimAirCampaignSubsystem::RaidOptions(int32 PackageId, const FString& Seat)
{
	FString Options = FString::Printf(TEXT("SimAirWarRaid=%d"), PackageId);
	if (!Seat.IsEmpty())
	{
		Options += FString::Printf(TEXT("?SimSeat=%s"), *Seat);
	}
	return Options;
}

bool USimAirCampaignSubsystem::WriteRaidScenario(const simcore::airwar::AirCampaign& InCampaign, const simcore::airwar::Package& Package,
	const FString& DataDir, FString& OutRelativeFile, simcore::airwar::RaidManifest& OutManifest, FString& OutError)
{
	simcore::airwar::RaidScenarioBuilder Builder;
	// On the Kesmark range map's ground when the map is there (data/maps/, simcore/terrain_map.hpp), with the day's weather;
	// the battery and its dummies set down on open ground near where the campaign has them.
	simcore::airwar::ScenarioFormat Format;
	if (FPaths::FileExists(FPaths::Combine(DataDir, TEXT("maps/kesmark_range.json"))))
	{
		Format.map_file = "maps/kesmark_range.json";
		std::string MapError;
		Format.map = simcore::LoadMapCached(TCHAR_TO_UTF8(*DataDir), Format.map_file, &MapError);
		if (!Format.map)
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Air war: map %s: %s (the battery stands where the campaign has it)"),
				UTF8_TO_TCHAR(Format.map_file.c_str()), UTF8_TO_TCHAR(MapError.c_str()));
		}
	}
	const simcore::airwar::RaidScenario Raid = Builder.Build(InCampaign, Package, Format);
	if (!Raid.ok)
	{
		OutError = AirWarToF(Raid.error);
		return false;
	}
	// The same strict loader the game uses: a raid that would not start is refused here, not in the level.
	simcore::ScenarioDef Def;
	simcore::ConsoleData Console;
	std::string Error;
	if (!simcore::LoadScenario(Raid.json_text, AirWarToStd(DataDir), Def, Console, &Error))
	{
		OutError = AirWarToF(Error);
		return false;
	}
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AirWar")));
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("raid_%d.json"), Package.id));
	if (!FFileHelper::SaveStringToFile(AirWarToF(Raid.json_text), *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("cannot write %s"), *File);
		return false;
	}
	FString Relative = File;
	FString From = DataDir;
	FPaths::NormalizeDirectoryName(From);
	if (!FPaths::MakePathRelativeTo(Relative, *(From + TEXT("/"))))
	{
		OutError = FString::Printf(TEXT("%s is not reachable from the data folder %s"), *File, *DataDir);
		return false;
	}
	OutRelativeFile = Relative;
	OutManifest = Raid.manifest;
	return true;
}

bool USimAirCampaignSubsystem::FightPendingRaid(UObject* WorldContext, const FString& Seat, FString& OutError)
{
	if (!HasPendingRaid())
	{
		OutError = TEXT("no raid is waiting");
		return false;
	}
	const simcore::airwar::Package* P = Campaign->PlayerMission();
	simcore::airwar::RaidManifest NewManifest;
	if (!WriteRaidScenario(*Campaign, *P, DataDirectory(), RaidScenarioFile, NewManifest, OutError))
	{
		UE_LOG(LogSAM3D, Error, TEXT("Air war: raid %d can't be played (%s); it is fought without you"), P->id, *OutError);
		Campaign->SkipPlayerMission();
		Stepped();
		return false;
	}
	Manifest = MakeUnique<simcore::airwar::RaidManifest>(NewManifest);
	bRaidActive = true;
	bBridgeStarted = false;
	ReturnCountdown = -1.0f;
	UE_LOG(LogSAM3D, Log, TEXT("Air war: fighting raid %d (%s) from %s"), P->id, *RaidScenarioFile, Seat.IsEmpty() ? TEXT("outside") : *Seat);
	if (WorldContext != nullptr)
	{
		// The raid is fought on the map's level when it exists (the district, its towns and the battery's ground), else here.
		const TCHAR* MapLevel = TEXT("/Game/Maps/KesmarkRange");
		const FString Level = FPackageName::DoesPackageExist(MapLevel) ? FString(MapLevel) : UGameplayStatics::GetCurrentLevelName(WorldContext, true);
		UGameplayStatics::OpenLevel(WorldContext, FName(*Level), true, RaidOptions(P->id, Seat));
	}
	return true;
}

void USimAirCampaignSubsystem::LetPendingRaidPass()
{
	if (HasPendingRaid())
	{
		Campaign->SkipPlayerMission();
		Stepped();
	}
}

FSimBatteryPosture USimAirCampaignSubsystem::GetPosture() const
{
	FSimBatteryPosture Out;
	if (!Campaign.IsValid())
	{
		return Out;
	}
	const simcore::airwar::BatteryPosture& P = Campaign->Posture();
	Out.Emission = static_cast<float>(P.emission);
	Out.EmconDiscipline = static_cast<float>(P.emcon_discipline);
	Out.RadioDiscipline = static_cast<float>(P.radio_discipline);
	Out.bDummyTraffic = P.dummy_traffic;
	Out.bDecoyEmitters = !P.decoy_mode.empty();
	Out.bNets = P.nets;
	Out.bFightSilent = P.fight_silent;
	Out.RelocateEveryH = static_cast<float>(P.relocate_every_h);
	return Out;
}

void USimAirCampaignSubsystem::SetPosture(const FSimBatteryPosture& In)
{
	if (!Campaign.IsValid())
	{
		return;
	}
	simcore::airwar::BatteryPosture& P = Campaign->Posture();
	P.emission = FMath::Clamp(In.Emission, 0.0f, 1.0f);
	P.emcon_discipline = FMath::Clamp(In.EmconDiscipline, 0.0f, 1.0f);
	P.radio_discipline = FMath::Clamp(In.RadioDiscipline, 0.0f, 1.0f);
	P.dummy_traffic = In.bDummyTraffic;
	P.decoy_mode = In.bDecoyEmitters ? "when_radar_off" : "";
	P.nets = In.bNets;
	P.fight_silent = In.bFightSilent;
	P.relocate_every_h = FMath::Max(0.0f, In.RelocateEveryH);
}

bool USimAirCampaignSubsystem::RelocateBattery(FVector2D EastNorthKm, FString& OutWhy)
{
	if (!Campaign.IsValid() || bRaidActive)
	{
		OutWhy = TEXT("no campaign");
		return false;
	}
	const simcore::Vec3 To{EastNorthKm.X * 1000.0, EastNorthKm.Y * 1000.0, 0.0};
	std::string Why;
	if (!Campaign->CanRelocateBattery(To, &Why))
	{
		OutWhy = AirWarToF(Why);
		return false;
	}
	Campaign->RelocateBattery(To);
	Stepped();
	return true;
}

void USimAirCampaignSubsystem::OpenCampaignScreen(UObject* WorldContext)
{
	if (WorldContext != nullptr)
	{
		UGameplayStatics::OpenLevel(WorldContext, FName(*UGameplayStatics::GetCurrentLevelName(WorldContext, true)), true, TEXT("SimCampaign"));
	}
}

void USimAirCampaignSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World == nullptr)
	{
		return;
	}
	if (ReturnCountdown >= 0.0f)
	{
		ReturnCountdown -= DeltaTime;
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(731, 0.0f, FColor(255, 200, 90),
				FString::Printf(TEXT("The raid is over. Back to the campaign in %.0f s (Esc: the missions menu)."), FMath::Max(0.0f, ReturnCountdown)));
		}
		if (ReturnCountdown < 0.0f)
		{
			OpenCampaignScreen(World);
		}
		return;
	}
	if (!bRaidActive || !Campaign.IsValid() || !Manifest.IsValid())
	{
		return;
	}
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetScenarioRunner() : nullptr;
	if (Runner == nullptr)
	{
		return;
	}
	if (!bBridgeStarted)
	{
		Bridge = MakeUnique<simcore::airwar::MissionBridge>(*Manifest, Campaign->Data(), static_cast<unsigned>(CampaignSeed) * 977u + static_cast<unsigned>(Manifest->package_id));
		bBridgeStarted = true;
		RaidWorld = World;
	}
	// After the world subsystem's fixed steps of this frame (it ticks before game-instance tickables): the bridge reads
	// the runner and adds the raid's weapons; it never steps the simulation.
	Bridge->Step(*Runner);
	if (Bridge->UnitDestroyed() && GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(732, 5.0f, FColor::Red, TEXT("Our vehicle is out of action."));
	}
	if (Runner->Finished() || Bridge->UnitDestroyed())
	{
		FinishRaid(*Runner, Runner->Finished() ? TEXT("the raid is over") : TEXT("our vehicle is out of action"));
		ReturnCountdown = kReturnAfterRaidS;
	}
}

void USimAirCampaignSubsystem::FinishRaid(simcore::ScenarioRunner& Runner, const TCHAR* Why)
{
	if (!bRaidActive || !Bridge.IsValid() || !Manifest.IsValid() || !Campaign.IsValid())
	{
		return;
	}
	const simcore::airwar::PackageOutcome Outcome = Bridge->Finish(Runner);
	int32 Drones = 0;
	for (const simcore::DebriefEntry& E : Runner.Log().Entries())
	{
		const bool bHit = E.kind == simcore::DebriefEntry::Kind::Hit;
		const bool bDestroyed = E.kind == simcore::DebriefEntry::Kind::Scenario && E.detail.compare(0, 9, "destroyed") == 0;
		if (!bHit && !bDestroyed)
		{
			continue;
		}
		for (const simcore::airwar::RaidAircraft& A : Manifest->aircraft)
		{
			Drones += A.drone && A.target_id == E.what ? 1 : 0;
		}
	}
	LastRaidReport = FString::Printf(TEXT("%s: %d aircraft came into the sector; %d missiles launched; %d shot down%s%s."), Why,
		static_cast<int32>(Manifest->aircraft.size()), Runner.Launches(), Outcome.Lost(), Drones > 0 ? *FString::Printf(TEXT(" and %d decoys"), Drones) : TEXT(""),
		Bridge->UnitDestroyed() ? TEXT("; our vehicle was hit") : TEXT(""));
	Campaign->CompletePlayerMission(Outcome, Manifest->start_h + Runner.Time() / 3600.0);
	Stepped();
	UE_LOG(LogSAM3D, Log, TEXT("Air war: %s"), *LastRaidReport);
	bRaidActive = false;
	bBridgeStarted = false;
	Bridge.Reset();
	Manifest.Reset();
	RaidWorld.Reset();
}

void USimAirCampaignSubsystem::HandleWorldTearDown(UWorld* World)
{
	// A raid left before it ended (the player went to the menu, restarted, or quit): what happened so far counts.
	if (!bRaidActive || !bBridgeStarted || World == nullptr || World != RaidWorld.Get())
	{
		return;
	}
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetScenarioRunner() : nullptr;
	if (Runner != nullptr)
	{
		FinishRaid(*Runner, TEXT("the raid was left early"));
	}
	else
	{
		bRaidActive = false;
		bBridgeStarted = false;
		Bridge.Reset();
		Manifest.Reset();
	}
}
