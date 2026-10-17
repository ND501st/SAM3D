#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimEntityActor.h"
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
		TestEqual(TEXT("a base and five moving sections"), Mast->NumSections(), 6);
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

#endif
