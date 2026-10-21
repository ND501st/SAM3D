#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimCabinComponent.h"
#include "SimCrewBodiesComponent.h"
#include "SimEntityActor.h"
#include "SimOsaVehicle.h"
#include "SimPu12MastComponent.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The PU-12M's exterior (docs/PU12_DEVELOPMENT.md P5): a battery entity of kind command_post wears the traced model, and
// its mast's sections stand where tools/blender/pu12m_trace.py puts them, stowed (about 1.5 m above the roof) and raised
// (16 m above the ground); the guys show only when it is up, the AB-1 only when it is set out. The player's own post
// follows its console's mast_up subsystem.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimPu12MastTest, "SAM3D.PU12M.Mast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimPu12MastTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DPu12MastTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	ASimEntityActor* Post = World->SpawnActor<ASimEntityActor>();
	TestNotNull(TEXT("an entity actor"), Post);
	const bool bModel = Post != nullptr && Post->UseVehicleModel(TEXT("command_post"));
	TestTrue(TEXT("the PU-12M model is imported (tools/ue_import_support_vehicles.py)"), bModel);
	USimPu12MastComponent* Mast = Post != nullptr ? Post->GetMast() : nullptr;
	TestNotNull(TEXT("the post has its mast"), Mast);
	if (Mast != nullptr)
	{
		TestTrue(TEXT("the rig file was read"), Mast->IsRigLoaded());
		TestEqual(TEXT("a fixed section and eight moving ones"), Mast->NumSections(), 9);
		Mast->SetExtension(0.f);
		const float Stowed = Mast->SectionTopZ(Mast->NumSections() - 1);
		TestTrue(FString::Printf(TEXT("stowed, the top at %.0f cm: about 1.5 m above the 2.055 m roof"), Stowed), FMath::Abs(Stowed - 355.5f) < 2.f);
		TestFalse(TEXT("no guys while stowed"), Mast->AreGuysShown());
		Mast->SetExtension(1.f);
		TestTrue(TEXT("raised, the top at 16 m"), FMath::IsNearlyEqual(Mast->SectionTopZ(Mast->NumSections() - 1), 1600.f, 1.f));
		TestTrue(TEXT("guyed when up"), Mast->AreGuysShown());
		for (int32 I = 2; I < Mast->NumSections(); ++I)
		{
			TestTrue(FString::Printf(TEXT("section %d stands above section %d"), I, I - 1), Mast->SectionTopZ(I) > Mast->SectionTopZ(I - 1));
		}
		// At its own pace toward a target: 420 s up, so 42 s gets a tenth of the way.
		Mast->SetExtension(0.f);
		Mast->SetTargetExtension(1.f);
		for (int32 I = 0; I < 420; ++I)
		{
			Mast->TickComponent(0.1f, LEVELTICK_All, nullptr);
		}
		TestTrue(FString::Printf(TEXT("a tenth up after 42 s (%.3f)"), Mast->GetExtension()), FMath::IsNearlyEqual(Mast->GetExtension(), 0.1f, 0.01f));
	}

	// The player's own post: the mast follows the console's mast_up subsystem.
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/pu12m_deploy.json"), Error);
	TestTrue(FString::Printf(TEXT("the PU-12M scenario loads %s"), *Error), bLoaded);
	if (bLoaded)
	{
		TestTrue(TEXT("stowed at the start"), FMath::IsNearlyEqual(Sim->GetPostMastExtension(), 0.f, 1e-3f));
		for (int32 I = 0; I < 3000; ++I)  // 150 s: the radio operator is raising it
		{
			Sim->Tick(0.05f);
		}
		const float E = Sim->GetPostMastExtension();
		AddInfo(FString::Printf(TEXT("mast at %.2f after 150 s"), E));
		TestTrue(TEXT("on its way up"), E > 0.05f && E < 1.f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// The PU-12M's cabin (P6): the battery scenario's own vehicle is the post, the director's default cabin is its own
// (data/cabins/pu12m.json), the player's vehicle wears the PU-12M's exterior and mast, the cabin has a seat for each of
// the five stations with the IT-45 and the two IT boards on its panels, a body in every seat, and the post's API (the
// IT-45's numbers, the board's, the launchers on the IT boards) is wired through.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimPu12CabinTest, "SAM3D.PU12M.Cabin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimPu12CabinTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DPu12CabinTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/pu12m_battery.json"), Error);
	TestTrue(FString::Printf(TEXT("the battery scenario loads %s"), *Error), bLoaded);
	if (!bLoaded)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	TestTrue(TEXT("the player's vehicle is the post"), Sim->IsCommandPost());
	TestEqual(TEXT("its own cabin"), Sim->GetDefaultCabinFile(), FString(TEXT("cabins/pu12m.json")));
	TestEqual(TEXT("the battery commander commands"), Sim->GetCommanderStation(), FString(TEXT("battery_commander")));
	TestTrue(FString::Printf(TEXT("the cabin loads %s"), *Error), Sim->LoadCabin(Sim->GetDefaultCabinFile(), Error));
	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>();
	TestNotNull(TEXT("the player's vehicle"), Car);
	if (Car != nullptr)
	{
		Car->Tick(0.05f);
		TestTrue(TEXT("it knows it is the post"), Car->IsCommandPost());
		UStaticMeshComponent* Exterior = nullptr;
		for (UActorComponent* C : Car->GetComponents())
		{
			if (C->GetName() == TEXT("PostExterior"))
			{
				Exterior = Cast<UStaticMeshComponent>(C);
			}
		}
		TestTrue(TEXT("it wears SM_PU12M"), Exterior != nullptr && Exterior->GetStaticMesh() != nullptr);
		TestNotNull(TEXT("and its mast"), Car->FindComponentByClass<USimPu12MastComponent>());
		USimCabinComponent* Cabin = Car->FindComponentByClass<USimCabinComponent>();
		TestTrue(TEXT("a cabin component"), Cabin != nullptr);
		if (Cabin != nullptr)
		{
			TestTrue(TEXT("into the cabin"), Car->SetInCabin(true));
			TestEqual(TEXT("five seats"), Cabin->GetNumStations(), 5);
			TestEqual(TEXT("the IT-45, the IT-1M, the IT-2M, the plotter's map and the ДХ's window"), Cabin->GetNumDisplays(), 5);
			TestFalse(TEXT("the hull hidden from inside"), Exterior != nullptr && Exterior->IsVisible());
			TestFalse(TEXT("the compartment shown inside"), Cabin->IsCabinHidden());
			Car->SetInCabin(false);
			TestTrue(TEXT("and hidden from outside"), Cabin->IsCabinHidden());
			Car->SetInCabin(true);
		}
		USimCrewBodiesComponent* Bodies = Car->FindComponentByClass<USimCrewBodiesComponent>();
		for (int32 I = 0; I < 200; ++I)  // 10 s
		{
			Sim->Tick(0.05f);
			if (Bodies != nullptr)
			{
				Bodies->TickComponent(0.05f, LEVELTICK_All, nullptr);
			}
		}
		TestEqual(TEXT("a body in each seat"), Bodies != nullptr ? Bodies->GetNumBodies() : 0, 5);
	}
	TestEqual(TEXT("four launchers on the IT boards"), Sim->GetLauncherStatus().Num(), 4);
	// The post deployed: the ASPD-U warms up and the regiment's telecode brings the raid onto the IT-45; the NPC
	// plotter writes it on the board.
	for (int32 I = 0; I < 20 * 300 && Sim->GetPostBoardNumbers().Num() == 0; ++I)
	{
		Sim->Tick(0.05f);
	}
	AddInfo(FString::Printf(TEXT("IT-45: %d targets, board: %d"), Sim->GetPostTrackNumbers().Num(), Sim->GetPostBoardNumbers().Num()));
	TestTrue(TEXT("targets on the IT-45"), Sim->GetPostTrackNumbers().Num() > 0);
	TestTrue(TEXT("and on the board"), Sim->GetPostBoardNumbers().Num() > 0);
	TestFalse(TEXT("an unknown launcher can't be given a target"), Sim->DesignateBatteryTarget(TEXT("99"), 1));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

// The PU-12M as a seat of the Osa's battery (docs/PU12_DEVELOPMENT.md P8): a battery link loads as one scenario, both
// vehicles step together, and the player moves between them with the vehicle actor, its cabin and its exterior following.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimBatteryLinkTest, "SAM3D.PU12M.BatteryLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimBatteryLinkTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DBatteryLinkTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/battery_link_practice.json"), Error);
	TestTrue(FString::Printf(TEXT("the link loads %s"), *Error), bLoaded);
	if (!bLoaded)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return true;
	}
	TestTrue(TEXT("a battery link"), Sim->IsBatteryLink());
	TestFalse(TEXT("the player starts in the Osa"), Sim->IsCommandPost());
	TestEqual(TEXT("its cabin"), Sim->GetDefaultCabinFile(), FString(TEXT("cabins/osa_ak.json")));
	TestTrue(FString::Printf(TEXT("the Osa's cabin loads %s"), *Error), Sim->LoadCabin(Sim->GetDefaultCabinFile(), Error));
	TestEqual(TEXT("the other vehicle"), Sim->GetOtherLinkedVehicle(), FString(TEXT("the PU-12M")));
	ASimOsaVehicle* Car = World->SpawnActor<ASimOsaVehicle>(Sim->GetUnitLocation(), FRotator::ZeroRotator);
	TestNotNull(TEXT("the player's vehicle"), Car);
	for (int32 I = 0; I < 200; ++I)  // 10 s
	{
		Sim->Tick(0.05f);
		if (Car != nullptr)
		{
			Car->Tick(0.05f);
		}
	}
	const simcore::ScenarioRunner* Osa = Sim->GetScenarioRunner();
	TestTrue(TEXT("the Osa's time runs"), Osa != nullptr && Osa->Time() > 9.0);
	TestFalse(TEXT("the vehicle is the Osa"), Car != nullptr && Car->IsCommandPost());

	TestTrue(TEXT("to the PU-12M"), Sim->SwitchLinkedVehicle());
	TestTrue(TEXT("now the post"), Sim->IsCommandPost());
	TestEqual(TEXT("with its own cabin"), Sim->GetDefaultCabinFile(), FString(TEXT("cabins/pu12m.json")));
	TestTrue(TEXT("its cabin is loaded"), Sim->GetCabinLayout() != nullptr && Sim->GetCabinLayout()->stations.size() == 5);
	const simcore::ScenarioRunner* Post = Sim->GetScenarioRunner();
	TestTrue(TEXT("the post kept time with the Osa"), Post != nullptr && Osa != nullptr && FMath::Abs(Post->Time() - Osa->Time()) < 1e-6);
	if (Car != nullptr)
	{
		Car->Tick(0.05f);
		TestTrue(TEXT("the vehicle became the post"), Car->IsCommandPost());
		const FVector Expected = Sim->GetUnitLocation();
		TestTrue(TEXT("and stands where the post does"), FVector::Dist2D(Car->GetActorLocation(), Expected) < 100.0);
		TestTrue(TEXT("into its cabin"), Car->SetInCabin(true));
		USimCabinComponent* Cabin = Car->FindComponentByClass<USimCabinComponent>();
		if (Cabin != nullptr)
		{
			Cabin->TickComponent(0.05f, LEVELTICK_All, nullptr);
			TestEqual(TEXT("five seats"), Cabin->GetNumStations(), 5);
		}
	}
	for (int32 I = 0; I < 200; ++I)
	{
		Sim->Tick(0.05f);
	}
	TestTrue(TEXT("the Osa fights on while the player is in the post"), Osa != nullptr && Osa->Time() > 19.0);
	TestTrue(TEXT("and back to the Osa"), Sim->SwitchLinkedVehicle());
	if (Car != nullptr)
	{
		Car->Tick(0.05f);
		TestFalse(TEXT("the vehicle is the Osa again"), Car->IsCommandPost());
		TestFalse(TEXT("out of the cabin"), Car->IsInCabin());
		bool bExterior = false;
		for (UActorComponent* C : Car->GetComponents())
		{
			bExterior = bExterior || (C->GetName() == TEXT("PostExterior") && IsValid(C) && !C->IsBeingDestroyed());
		}
		TestFalse(TEXT("without the post's exterior"), bExterior);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
