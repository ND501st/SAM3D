#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SimCoreIncludes.h"
#include "SimCrewBodiesComponent.h"
#include "SimCrewVoiceComponent.h"
#include "SimOsaVehicle.h"
#include "SimSpeech.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The crew's voices (roadmap 8.10): the operating system's speech engine speaks a callout's words. Without any English
// voice installed the test only warns: the crew is then silent, which is allowed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCrewSpeechTest, "SAM3D.Crew.Speech",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCrewSpeechTest::RunTest(const FString& Parameters)
{
	FSimSpeech& Speech = FSimSpeech::Get();
	for (int32 I = 0; I < 200 && !Speech.IsReady(); ++I)
	{
		FPlatformProcess::Sleep(0.05f);
	}
	TestTrue(TEXT("the speech engine listed its voices"), Speech.IsReady());
	for (const FString& V : Speech.DescribeVoices())
	{
		AddInfo(FString::Printf(TEXT("voice: %s"), *V));
	}
	if (Speech.NumVoices(TEXT("en")) == 0)
	{
		AddWarning(TEXT("no English voice installed: the crew would be silent"));
		return true;
	}
	// The words of a contact report, as simcore spells them for an English engine.
	const FString DataDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../data")));
	simcore::VoiceData Voices;
	std::string Error;
	TestTrue(TEXT("voices.json loads"), simcore::LoadVoiceDataFile(std::string(TCHAR_TO_UTF8(*DataDir)) + "/crew/voices.json", nullptr, Voices, &Error));
	simcore::SpokenCallout Line;
	Line.en = "Target, azimuth 35-00, range 20!";
	Line.ru = "Цель, азимут 35-00, дальность 20!";
	const FString Text(UTF8_TO_TCHAR(simcore::SpeechText(Line, "en", Voices).c_str()));
	TestEqual(TEXT("the words"), Text, FString(TEXT("Target, azimuth three five, zero zero, range 20!")));
	FSimSpeechRequest R;
	R.Id = 4242;
	R.Text = Text;
	R.Language = TEXT("en");
	R.Pitch = -2;
	R.Rate = 2;
	Speech.Request(R);
	FSimSpeechResult Out;
	bool bGot = false;
	for (int32 I = 0; I < 400 && !bGot; ++I)
	{
		bGot = Speech.TakeResult(Out) && Out.Id == R.Id;
		if (!bGot)
		{
			FPlatformProcess::Sleep(0.05f);
		}
	}
	TestTrue(TEXT("the line came back"), bGot);
	TestTrue(TEXT("it was spoken"), Out.bOk);
	const double Seconds = static_cast<double>(Out.Pcm.Num()) / FMath::Max(1, Out.SampleRate);
	double Sum = 0.0;
	for (int16 S : Out.Pcm)
	{
		Sum += static_cast<double>(S) * S;
	}
	const double Rms = Out.Pcm.Num() > 0 ? FMath::Sqrt(Sum / Out.Pcm.Num()) : 0.0;
	AddInfo(FString::Printf(TEXT("%.2f s of speech, rms %.0f"), Seconds, Rms));
	TestTrue(TEXT("a line's length"), Seconds > 1.0 && Seconds < 10.0);
	TestTrue(TEXT("not silence"), Rms > 300.0);
	return true;
}

// The crew's bodies (roadmap 8.10): in a game world with the crew practice scenario and the Osa cabin, the vehicle's
// bodies component builds one body per seat, each head where its station's eye point says, and hides the camera's seat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCrewBodiesTest, "SAM3D.Crew.Bodies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCrewBodiesTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DCrewBodiesTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_crew_practice.json"), Error) &&
		Sim->LoadCabin(TEXT("cabins/osa_ak.json"), Error);
	TestTrue(FString::Printf(TEXT("scenario and cabin load %s"), *Error), bLoaded);
	ASimOsaVehicle* Car = bLoaded ? World->SpawnActor<ASimOsaVehicle>() : nullptr;
	USimCrewBodiesComponent* Bodies = Car != nullptr ? Car->FindComponentByClass<USimCrewBodiesComponent>() : nullptr;
	TestNotNull(TEXT("the vehicle has a bodies component"), Bodies);
	if (Bodies != nullptr)
	{
		for (int32 I = 0; I < 100; ++I)  // 5 s: the crew starts its drills
		{
			Sim->Tick(0.05f);
			Bodies->TickComponent(0.05f, LEVELTICK_All, nullptr);
		}
		TestEqual(TEXT("a body per seat"), Bodies->GetNumBodies(), 5);
		for (const simcore::CabinStation& St : Sim->GetCabinLayout()->stations)
		{
			FVector Head;
			bool bVisible = false;
			const FString Id(UTF8_TO_TCHAR(St.id.c_str()));
			TestTrue(FString::Printf(TEXT("%s has a body"), *Id), Bodies->GetBodyHead(Id, Head, bVisible));
			const FVector Eye(St.eye_m.y * 100.0, St.eye_m.x * 100.0, St.eye_m.z * 100.0);
			const double Off = FVector::Dist(Head, Eye);
			AddInfo(FString::Printf(TEXT("%s: head %.0f cm from the eye point"), *Id, Off));
			TestTrue(FString::Printf(TEXT("%s sits in his seat"), *Id), Off < 25.0);
			TestTrue(FString::Printf(TEXT("%s is drawn"), *Id), bVisible);
		}
		Bodies->SetHiddenStation(TEXT("operator"));
		Bodies->TickComponent(0.05f, LEVELTICK_All, nullptr);
		FVector Head;
		bool bVisible = true;
		Bodies->GetBodyHead(TEXT("operator"), Head, bVisible);
		TestFalse(TEXT("the camera's seat is hidden"), bVisible);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// A recorded line takes its recording's time on the bus (docs/crew/10_BODIES_AND_VOICES.md section 4): with the crew
// speaking English, the vehicle's voice component gives the runner the English recordings' lengths, and every fixed line
// with a recording lasts exactly as long as the recording the component plays for it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCrewRecordedLinesTest, "SAM3D.Crew.RecordedLines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCrewRecordedLinesTest::RunTest(const FString& Parameters)
{
	FSimSpeech& Speech = FSimSpeech::Get();
	for (int32 I = 0; I < 200 && !Speech.IsReady(); ++I)
	{
		FPlatformProcess::Sleep(0.05f);
	}
	IConsoleVariable* LanguageVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sam3d.VoiceLanguage"));
	TestNotNull(TEXT("sam3d.VoiceLanguage"), LanguageVar);
	if (LanguageVar == nullptr || !Speech.IsReady())
	{
		return false;
	}
	const FString WasLanguage = LanguageVar->GetString();
	LanguageVar->Set(TEXT("en"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DCrewRecordedLinesTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	const bool bLoaded = Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_crew_practice.json"), Error);
	TestTrue(FString::Printf(TEXT("the scenario loads %s"), *Error), bLoaded);
	ASimOsaVehicle* Car = bLoaded ? World->SpawnActor<ASimOsaVehicle>() : nullptr;
	USimCrewVoiceComponent* Voice = Car != nullptr ? Car->FindComponentByClass<USimCrewVoiceComponent>() : nullptr;
	TestNotNull(TEXT("the vehicle has a voice component"), Voice);
	if (Voice != nullptr)
	{
		for (int32 I = 0; I < 1200; ++I)  // a minute of the drill
		{
			Sim->Tick(0.05f);
			Voice->TickComponent(0.05f, LEVELTICK_All, nullptr);
		}
		const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
		TestEqual(TEXT("the crew speaks English"), Voice->GetLanguage(), FString(TEXT("en")));
		const std::string Dir = Runner->RecordedLengthsFrom();
		TestFalse(TEXT("the runner has the recordings' lengths"), Dir.empty());
		const double KBoyuS = simcore::WavLengthS(Dir + "/commander/order_k_boyu.wav");
		TestTrue(TEXT("the commander's К бою! is recorded"), KBoyuS > 0.0);
		TestEqual(TEXT("К бою! takes its recording's time"), Runner->Callouts()->LineTimeS("order_k_boyu", "commander", "К бою!"), KBoyuS, 1e-9);
		const std::map<std::string, double> Lengths = simcore::RecordedLineLengths(Dir, Runner->Def().crew_data);
		int32 Checked = 0;
		for (const simcore::SpokenCallout& S : Runner->Callouts()->Log())
		{
			const simcore::CalloutDef* Def = Runner->Def().crew_data.FindCallout(S.id);
			auto It = Lengths.find(S.from + "/" + S.id);
			if (It == Lengths.end())
			{
				It = Lengths.find(S.id);
			}
			if (Def == nullptr || Def->ru != S.ru || It == Lengths.end())
			{
				continue;
			}
			++Checked;
			TestEqual(FString::Printf(TEXT("%s/%s lasts its recording"), UTF8_TO_TCHAR(S.from.c_str()), UTF8_TO_TCHAR(S.id.c_str())),
				S.end_s - S.start_s, It->second, 1e-9);
		}
		AddInfo(FString::Printf(TEXT("%d recorded lines on the bus, %d said in a minute"), static_cast<int32>(Lengths.size()), Checked));
		TestTrue(TEXT("recorded lines were said"), Checked > 0);
	}
	LanguageVar->Set(*WasLanguage);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
