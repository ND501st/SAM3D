#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The trainer's instructor (roadmap 10.3) through the subsystem: in a game world with the trainer scenario he enters
// training drones, sees every target as it is, turns and sends them down, and the trainer holds two at a time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimInstructorTest, "SAM3D.Instructor.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimInstructorTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DInstructorTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_trainer.json"), Error);
	TestTrue(FString::Printf(TEXT("the trainer scenario loads %s"), *Error), bLoaded);
	if (bLoaded)
	{
		TestTrue(TEXT("the instructor's seat works here"), Sim->CanInstruct());
		FString Id1, Id2, Id3, Why;
		TestTrue(TEXT("a drone entered to the north"), Sim->InstructorEnterDrone(0.0, 30.0, 1500.0, Id1, Why));
		TestTrue(TEXT("a second to the east"), Sim->InstructorEnterDrone(90.0, 25.0, 800.0, Id2, Why));
		TestTrue(TEXT("a third to the west"), Sim->InstructorEnterDrone(270.0, 28.0, 1200.0, Id3, Why));
		TestEqual(TEXT("fresh ids"), Id1 + Id2 + Id3, FString(TEXT("i1i2i3")));
		for (int32 I = 0; I < 20; ++I)
		{
			Sim->Tick(0.05f);
		}
		const TArray<FSimInstructorTrack> Picture = Sim->GetInstructorPicture();
		TestEqual(TEXT("three in the sky"), Picture.Num(), 3);
		const FSimInstructorTrack* North = Picture.FindByPredicate([&](const FSimInstructorTrack& T) { return T.Id == Id1; });
		TestTrue(TEXT("the first 30 km out, flying south"), North != nullptr && FMath::Abs(North->RangeKm - 30.0) < 1.0 &&
			FMath::Abs(North->HeadingDeg - 180.0) < 10.0);
		TestTrue(TEXT("he turns the first"), Sim->Instruct(Id1, TEXT("turn"), 90.0, Why));
		TestTrue(TEXT("and sends the second down"), Sim->Instruct(Id2, TEXT("height"), 200.0, Why));
		TestFalse(TEXT("a third is one too many"), Sim->Instruct(Id3, TEXT("turn"), 0.0, Why));
		TestTrue(FString::Printf(TEXT("the reason: %s"), *Why), Why.Contains(TEXT("two targets at a time")));
		TestFalse(TEXT("an unknown order"), Sim->Instruct(Id3, TEXT("loop"), 0.0, Why));
		TestTrue(TEXT("let one go, the third can be taken"), Sim->Instruct(Id1, TEXT("release"), 0.0, Why) && Sim->Instruct(Id3, TEXT("turn"), 0.0, Why));
		TestTrue(TEXT("the debrief has him"), Sim->GetDebriefText().Contains(TEXT("Instructor")));
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
