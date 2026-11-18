#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SimAirCampaignSubsystem.h"
#include "SimFxAudioScene.h"

#include <cmath>
#include <string>

#if WITH_DEV_AUTOMATION_TESTS

// The weapons' and aircraft's sound (docs/fx/README.md 4.2) as plain C++: SimFxAudio::FScene driven frame by frame with
// data/fx/sounds.json, the standard atmosphere and every sample given a length (no assets needed).

namespace
{
	constexpr double SimFxAudioTestDt = 1.0 / 60.0;

	bool SimFxAudioTestLoadBank(SimFxAudio::FBank& Bank, FAutomationTestBase& Test)
	{
		const FString Path = FPaths::Combine(USimAirCampaignSubsystem::DataDirectory(), TEXT("fx/sounds.json"));
		FString Text;
		FString Error;
		const bool bRead = FFileHelper::LoadFileToString(Text, *Path);
		const bool bOk = bRead && Bank.LoadJson(std::string(TCHAR_TO_UTF8(*Text)), Error);
		Test.TestTrue(FString::Printf(TEXT("%s loads (%s)"), *Path, bRead ? *Error : TEXT("not found")), bOk);
		return bOk;
	}

	void SimFxAudioTestSetUp(SimFxAudio::FScene& Scene, const SimFxAudio::FBank& Bank)
	{
		Scene.SetBank(&Bank);
		Scene.SetAllSamplesAvailable(4.0);
		Scene.SetAir([](double AltM) { return simcore::StandardAtmosphere(AltM); }, 70.0);
	}

	SimFxAudio::FListener SimFxAudioTestEar()
	{
		SimFxAudio::FListener L;
		L.Enu = simcore::Vec3{0.0, 0.0, 1.7};
		L.AltM = 1.7;
		L.AglM = 1.7;
		return L;
	}

	SimFxAudio::FSourceUpdate SimFxAudioTestJet(uint64 Key, const SimFxAudio::FProfileDef* Profile, double T,
		const simcore::Vec3& At, const simcore::Vec3& Vel)
	{
		SimFxAudio::FSourceUpdate U;
		U.Key = Key;
		U.Profile = Profile;
		U.TSimS = T;
		U.Enu = At;
		U.VelocityMps = Vel;
		U.AltM = At.z;
		U.AglM = At.z;
		U.LengthM = 16.0;
		U.bSeedHistory = true;
		return U;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioBankTest, "SAM3D.Fx.Audio.Bank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioBankTest::RunTest(const FString& Parameters)
{
	// sounds.json loads; every model's profile exists; every sample it names is one tools/fx_sounds.py makes.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	for (const TPair<FString, FString>& M : Bank.ModelProfiles)
	{
		TestNotNull(FString::Printf(TEXT("%s's profile %s"), *M.Key, *M.Value), Bank.Profile(M.Value));
	}
	for (const TPair<FString, FString>& A : Bank.ArchetypeProfiles)
	{
		TestNotNull(FString::Printf(TEXT("archetype %s's profile %s"), *A.Key, *A.Value), Bank.Profile(A.Value));
	}
	TestNotNull(TEXT("the missiles' profile"), Bank.Profile(Bank.MissileProfile));
	TestNotNull(TEXT("the wrecks' fire"), Bank.Profile(Bank.FireProfile));
	TestNotNull(TEXT("a heavy jet by its length"), Bank.ProfileFor(TEXT("no_such_model"), TEXT("jet"), 45.0));
	const SimFxAudio::FProfileDef* MiG = Bank.ProfileFor(TEXT("mig21"), TEXT("jet"), 14.7);
	TestTrue(TEXT("a MiG-21 is a turbojet"), MiG != nullptr && MiG->Id == TEXT("turbojet_fighter"));
	const SimFxAudio::FGunDef* Shilka = Bank.GunFor(TEXT("zsu234"));
	TestTrue(TEXT("the guns of a ZSU-23-4"), Shilka != nullptr && Shilka->Loop == FName(TEXT("S_FX_Gun_23mm_Quad")));
	const SimFxAudio::FBlastClassDef* Big = Bank.BlastFor(430.0);
	TestTrue(TEXT("a 500 kg bomb's burst is the large class"), Big != nullptr && Big->Near == FName(TEXT("S_FX_Explosion_Large_Near")));

	FString Tool;
	const FString ToolPath = FPaths::Combine(USimAirCampaignSubsystem::DataDirectory(), TEXT(".."), TEXT("tools"), TEXT("fx_sounds.py"));
	if (FFileHelper::LoadFileToString(Tool, *ToolPath))
	{
		TSet<FName> All;
		Bank.AllSamples(All);
		for (const FName N : All)
		{
			TestTrue(FString::Printf(TEXT("tools/fx_sounds.py makes %s"), *N.ToString()),
				Tool.Contains(FString::Printf(TEXT("'%s'"), *N.ToString())));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioArrivalTest, "SAM3D.Fx.Audio.Arrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioArrivalTest::RunTest(const FString& Parameters)
{
	// An explosion 3.4 km away is heard about 10 s after it happens (3400 m / 340.3 m/s), not before.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	SimFxAudio::FScene Scene;
	SimFxAudioTestSetUp(Scene, Bank);
	const SimFxAudio::FListener Ear = SimFxAudioTestEar();
	Scene.BeginFrame(0.0, SimFxAudioTestDt, Ear);
	const SimFxAudio::FBlastClassDef* Class = Bank.BlastFor(100.0);
	if (Class == nullptr)
	{
		AddError(TEXT("no blast class"));
		return false;
	}
	SimFxAudio::FOneShotDesc D;
	D.TEventS = 0.0;
	D.Enu = simcore::Vec3{3400.0, 0.0, 1.0};
	D.AltM = 1.0;
	D.AglM = 1.0;
	D.Near = Class->Near;
	D.Far = Class->Far;
	D.ChargeKg = 100.0;
	D.bGroundBurst = true;
	const uint64 Key = Scene.AddOneShot(D);
	TestTrue(TEXT("the one-shot is taken"), Key != 0);
	double StartedAt = -1.0;
	bool bVoiced = false;
	bool bEarly = false;
	for (double T = 0.0; T < 14.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		Scene.Mix(32, 1.0);
		const double Started = Scene.OneShotStartedAt(Key);
		if (Started >= 0.0 && StartedAt < 0.0)
		{
			StartedAt = Started;
			bVoiced = Scene.VoiceFor(Key) != nullptr;
			bEarly = T < 9.9;
		}
	}
	const double Expected = (D.Enu - Ear.Enu).Length() / simcore::StandardAtmosphere(1.0).speed_of_sound_mps;
	TestTrue(FString::Printf(TEXT("it arrives %.3f s after it happened (expected %.3f)"), StartedAt, Expected),
		FMath::Abs(StartedAt - Expected) < 0.05);
	TestFalse(TEXT("nothing is heard of it before"), bEarly);
	TestTrue(TEXT("it gets a voice when it arrives"), bVoiced);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioBoomTest, "SAM3D.Fx.Audio.SupersonicPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioBoomTest::RunTest(const FString& Parameters)
{
	// A fighter at Mach 1.5, 5 km up, passes over the listener: silence while it comes, then exactly one boom, then its
	// roar going away.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	SimFxAudio::FScene Scene;
	SimFxAudioTestSetUp(Scene, Bank);
	const SimFxAudio::FListener Ear = SimFxAudioTestEar();
	const SimFxAudio::FProfileDef* Profile = Bank.Profile(TEXT("turbojet_fighter"));
	TestNotNull(TEXT("the turbojet's profile"), Profile);
	if (Profile == nullptr)
	{
		return false;
	}
	const double Speed = 1.5 * simcore::StandardAtmosphere(5000.0).speed_of_sound_mps;
	const simcore::Vec3 Vel{0.0, Speed, 0.0};
	const uint64 Key = 42;
	bool bHeardBeforeBoom = false;
	bool bHeardAfterBoom = false;
	double BoomAt = -1.0;
	for (double T = 0.0; T < 130.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		Scene.UpdateSource(SimFxAudioTestJet(Key, Profile, T, simcore::Vec3{0.0, -30000.0 + Speed * T, 5000.0}, Vel));
		Scene.Mix(32, 1.0);
		const int32 Booms = Scene.GetStats().Booms;
		if (Booms > 0 && BoomAt < 0.0)
		{
			BoomAt = T;
		}
		if (Booms == 0 && Scene.IsHeard(Key))
		{
			bHeardBeforeBoom = true;
		}
		if (Booms > 0 && Scene.IsHeard(Key))
		{
			bHeardAfterBoom = true;
		}
	}
	TestEqual(TEXT("exactly one boom"), Scene.GetStats().Booms, 1);
	TestFalse(TEXT("silence before it"), bHeardBeforeBoom);
	TestTrue(TEXT("the roar going away after it"), bHeardAfterBoom);
	// Overhead at 30 km / v; the cone reaches the ground some seconds later (h sqrt(M^2-1) / v in uniform air).
	const double Overhead = 30000.0 / Speed;
	TestTrue(FString::Printf(TEXT("the boom comes after it has passed overhead (%.1f s, overhead %.1f s)"), BoomAt, Overhead),
		BoomAt > Overhead + 3.0 && BoomAt < Overhead + 25.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioVoicesTest, "SAM3D.Fx.Audio.VoiceBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioVoicesTest::RunTest(const FString& Parameters)
{
	// A hundred aircraft round the listener: never more voices than MaxVoices, all of them used, the rest virtual.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	SimFxAudio::FScene Scene;
	SimFxAudioTestSetUp(Scene, Bank);
	const SimFxAudio::FListener Ear = SimFxAudioTestEar();
	const SimFxAudio::FProfileDef* Profile = Bank.Profile(TEXT("turbofan_fighter"));
	if (Profile == nullptr)
	{
		AddError(TEXT("no turbofan profile"));
		return false;
	}
	constexpr int32 MaxVoices = 32;
	int32 MostVoices = 0;
	for (double T = 0.0; T < 3.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		for (int32 I = 0; I < 100; ++I)
		{
			const double Az = I * 2.399963;  // the golden angle: spread all round
			const double R = 1000.0 + 90.0 * I;
			const simcore::Vec3 Radial{std::sin(Az), std::cos(Az), 0.0};
			const simcore::Vec3 Along{Radial.y, -Radial.x, 0.0};
			const simcore::Vec3 At = Radial * R + Along * (200.0 * T) + simcore::Vec3{0.0, 0.0, 800.0 + 10.0 * I};
			Scene.UpdateSource(SimFxAudioTestJet(static_cast<uint64>(1000 + I), Profile, T, At, Along * 200.0));
		}
		Scene.Mix(MaxVoices, 1.0);
		MostVoices = FMath::Max(MostVoices, Scene.NumActiveVoices());
		if (Scene.GetVoices().Num() > MaxVoices || Scene.NumActiveVoices() > MaxVoices)
		{
			AddError(FString::Printf(TEXT("%d voices at %.2f s"), Scene.NumActiveVoices(), T));
			break;
		}
	}
	TestEqual(TEXT("every voice in use"), MostVoices, MaxVoices);
	TestTrue(FString::Printf(TEXT("more sounds than voices (%d audible)"), Scene.GetStats().Audible), Scene.GetStats().Audible > MaxVoices);
	TestEqual(TEXT("a hundred sources tracked"), Scene.GetStats().Sources, 100);
	// Fewer voices at run time (the cvar): the pool shrinks to it.
	Scene.BeginFrame(3.0, SimFxAudioTestDt, Ear);
	Scene.Mix(8, 1.0);
	TestTrue(TEXT("down to 8"), Scene.NumActiveVoices() <= 8 && Scene.GetVoices().Num() == 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioWindowTest, "SAM3D.Fx.Audio.LoudnessWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioWindowTest::RunTest(const FString& Parameters)
{
	// A light aircraft 8 km off is heard in a quiet sky; a jet passing 60 m away pushes the window up and the light
	// aircraft below it: not played. When the jet has gone it comes back.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	SimFxAudio::FScene Scene;
	SimFxAudioTestSetUp(Scene, Bank);
	const SimFxAudio::FListener Ear = SimFxAudioTestEar();
	const SimFxAudio::FProfileDef* Piston = Bank.Profile(TEXT("piston"));
	const SimFxAudio::FProfileDef* Jet = Bank.Profile(TEXT("turbojet_fighter"));
	if (Piston == nullptr || Jet == nullptr)
	{
		AddError(TEXT("no piston or turbojet profile"));
		return false;
	}
	const uint64 Quiet = 7, Loud = 8;
	const simcore::Vec3 QuietVel{50.0, 0.0, 0.0};
	const auto QuietAt = [&QuietVel](double T) { return simcore::Vec3{-4000.0, 7000.0, 600.0} + QuietVel * T; };
	double T = 0.0;
	for (; T < 2.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		Scene.UpdateSource(SimFxAudioTestJet(Quiet, Piston, T, QuietAt(T), QuietVel));
		Scene.Mix(32, 1.0);
	}
	TestTrue(FString::Printf(TEXT("alone it is heard (%.1f dB at the ear, window top %.1f)"), Scene.LevelAtEar(Quiet), Scene.WindowTopDb()),
		Scene.IsAudible(Quiet) && Scene.VoiceFor(Quiet) != nullptr);
	// The jet: 60 m off, slow enough to stay there for a while.
	const simcore::Vec3 JetVel{0.0, 20.0, 0.0};
	for (; T < 4.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		Scene.UpdateSource(SimFxAudioTestJet(Quiet, Piston, T, QuietAt(T), QuietVel));
		Scene.UpdateSource(SimFxAudioTestJet(Loud, Jet, T, simcore::Vec3{60.0, -20.0 + 20.0 * (T - 2.0), 30.0}, JetVel));
		Scene.Mix(32, 1.0);
	}
	TestTrue(FString::Printf(TEXT("the jet is loud (%.1f dB, window top %.1f)"), Scene.LevelAtEar(Loud), Scene.WindowTopDb()),
		Scene.WindowTopDb() > 115.0 && Scene.VoiceFor(Loud) != nullptr);
	TestFalse(FString::Printf(TEXT("the light aircraft is under the window (%.1f dB)"), Scene.LevelAtEar(Quiet)), Scene.IsAudible(Quiet));
	TestNull(TEXT("and has no voice"), Scene.VoiceFor(Quiet));
	// The jet goes; the window comes slowly back down to its floor and the light aircraft is heard again.
	for (; T < 12.0; T += SimFxAudioTestDt)
	{
		Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
		Scene.UpdateSource(SimFxAudioTestJet(Quiet, Piston, T, QuietAt(T), QuietVel));
		Scene.Mix(32, 1.0);
	}
	TestTrue(TEXT("heard again once the jet has gone"), Scene.IsAudible(Quiet));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioCabinTest, "SAM3D.Fx.Audio.Enclosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioCabinTest::RunTest(const FString& Parameters)
{
	// The same jet 300 m off, heard in the open and from the closed cabin: inside it is duller (the low-pass at 700 Hz)
	// and quieter (about 14 dB); something beside the vehicle comes through less muffled (about 6 dB).
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	const SimFxAudio::FProfileDef* Jet = Bank.Profile(TEXT("turbofan_fighter"));
	if (Jet == nullptr)
	{
		AddError(TEXT("no turbofan profile"));
		return false;
	}
	const auto Run = [&](bool bEnclosed, const simcore::Vec3& Where, double& OutCutoff, double& OutVolume)
	{
		SimFxAudio::FScene Scene;
		SimFxAudioTestSetUp(Scene, Bank);
		SimFxAudio::FListener Ear = SimFxAudioTestEar();
		Ear.bEnclosed = bEnclosed;
		Ear.VehicleEnu = simcore::Vec3{0.0, 0.0, 0.0};
		const simcore::Vec3 Vel{0.0, 10.0, 0.0};
		for (double T = 0.0; T < 2.0; T += SimFxAudioTestDt)
		{
			Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
			Scene.UpdateSource(SimFxAudioTestJet(5, Jet, T, Where + Vel * T, Vel));
			Scene.Mix(32, 1.0);
		}
		const SimFxAudio::FVoice* V = Scene.VoiceFor(5);
		OutCutoff = V != nullptr ? V->LowPassHz : -1.0;
		OutVolume = V != nullptr ? V->Volume : -1.0;
	};
	double OpenCut = 0.0, OpenVol = 0.0, ShutCut = 0.0, ShutVol = 0.0;
	Run(false, simcore::Vec3{300.0, 0.0, 50.0}, OpenCut, OpenVol);
	Run(true, simcore::Vec3{300.0, 0.0, 50.0}, ShutCut, ShutVol);
	TestTrue(FString::Printf(TEXT("heard in both (%.3f, %.3f)"), OpenVol, ShutVol), OpenVol > 0.0 && ShutVol > 0.0);
	TestTrue(FString::Printf(TEXT("the cabin lowers the cut-off (%.0f Hz open, %.0f Hz inside)"), OpenCut, ShutCut),
		ShutCut <= Bank.CabinCutoffHz + 1e-6 && ShutCut < OpenCut);
	TestTrue(FString::Printf(TEXT("and the level (%.1f dB)"), 20.0 * std::log10(FMath::Max(ShutVol, 1e-9) / FMath::Max(OpenVol, 1e-9))),
		FMath::Abs(20.0 * std::log10(ShutVol / OpenVol) + Bank.CabinLossDb) < 1.0);
	double NearOpenCut = 0.0, NearOpenVol = 0.0, NearShutCut = 0.0, NearShutVol = 0.0;
	Run(false, simcore::Vec3{15.0, 0.0, 2.0}, NearOpenCut, NearOpenVol);
	Run(true, simcore::Vec3{15.0, 0.0, 2.0}, NearShutCut, NearShutVol);
	TestTrue(FString::Printf(TEXT("beside the vehicle only %.1f dB"), 20.0 * std::log10(FMath::Max(NearShutVol, 1e-9) / FMath::Max(NearOpenVol, 1e-9))),
		NearOpenVol > 0.0 && FMath::Abs(20.0 * std::log10(NearShutVol / NearOpenVol) + Bank.CabinNearLossDb) < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioRecordingsTest, "SAM3D.Fx.Audio.Recordings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioRecordingsTest::RunTest(const FString& Parameters)
{
	// A fighter near is heard by its synthesised roar, far off by the recorded jet (sounds.json "band"), the two
	// crossfading at equal power between propagation near_m and far_m; a recording is dulled only by the distance it
	// was not already recorded at.
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	const SimFxAudio::FProfileDef* Jet = Bank.Profile(TEXT("turbojet_fighter"));
	if (Jet == nullptr)
	{
		AddError(TEXT("no turbojet profile"));
		return false;
	}
	FName Near, Far;
	for (const SimFxAudio::FLayerDef& L : Jet->Layers)
	{
		Near = L.Band == SimFxAudio::EBand::Near ? L.Sample : Near;
		Far = L.Band == SimFxAudio::EBand::Far ? L.Sample : Far;
	}
	TestTrue(TEXT("the turbojet has a near and a far layer"), !Near.IsNone() && !Far.IsNone());
	TestTrue(FString::Printf(TEXT("%s was recorded far off"), *Far.ToString()), Bank.RecordedAt(Far) > 500.0);
	const auto Run = [&](double DistM, const SimFxAudio::FVoice*& OutNear, const SimFxAudio::FVoice*& OutFar, SimFxAudio::FScene& Scene)
	{
		SimFxAudioTestSetUp(Scene, Bank);
		const SimFxAudio::FListener Ear = SimFxAudioTestEar();
		const simcore::Vec3 Vel{0.0, 5.0, 0.0};
		const simcore::Vec3 At{DistM, 0.0, 300.0};
		for (double T = 0.0; T < 1.0 + DistM / 330.0; T += SimFxAudioTestDt)
		{
			Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
			Scene.UpdateSource(SimFxAudioTestJet(9, Jet, T, At + Vel * T, Vel));
			Scene.Mix(32, 1.0);
		}
		OutNear = Scene.VoiceFor(9, Near);
		OutFar = Scene.VoiceFor(9, Far);
	};
	{
		SimFxAudio::FScene Scene;
		const SimFxAudio::FVoice* N = nullptr;
		const SimFxAudio::FVoice* F = nullptr;
		Run(300.0, N, F, Scene);
		TestTrue(TEXT("near: the synthesised roar plays, not the recording"), N != nullptr && F == nullptr);
	}
	{
		SimFxAudio::FScene Scene;
		const SimFxAudio::FVoice* N = nullptr;
		const SimFxAudio::FVoice* F = nullptr;
		Run(6000.0, N, F, Scene);
		TestTrue(TEXT("far: the recording plays, not the synthesised roar"), N == nullptr && F != nullptr);
		if (F != nullptr)
		{
			// The air at 6 km dulls a sound to its cut-off there; the recording, made RecordedAt off, only by the rest.
			SimFxAudio::FAir Air;
			Air.Build([](double A) { return simcore::StandardAtmosphere(A); }, 1.7, 70.0, Bank.AbsorptionBudgetDb);
			const double D = (simcore::Vec3{6000.0, 0.0, 300.0} - simcore::Vec3{0.0, 0.0, 1.7}).Length();
			TestTrue(FString::Printf(TEXT("dulled by the rest of the way (%.0f Hz, the whole way %.0f Hz)"), F->LowPassHz, Air.CutoffHz(D)),
				F->LowPassHz > Air.CutoffHz(D) * 1.2 && FMath::Abs(F->LowPassHz - Air.CutoffHz(D - Bank.RecordedAt(Far))) < 0.1 * F->LowPassHz);
		}
	}
	{
		// Half way through the crossfade both play, each 3 dB down: their powers add to one.
		double GNear = 0.0, GFar = 0.0;
		SimFxAudio::BandGains(0.5 * (Bank.NearM + Bank.FarM), Bank.NearM, Bank.FarM, GNear, GFar);
		TestTrue(FString::Printf(TEXT("equal power half way (%.3f, %.3f)"), GNear, GFar),
			FMath::Abs(GNear * GNear + GFar * GFar - 1.0) < 1e-9 && FMath::Abs(GNear - GFar) < 1e-9);
		SimFxAudio::BandGains(Bank.NearM - 1.0, Bank.NearM, Bank.FarM, GNear, GFar);
		TestTrue(TEXT("all near under near_m"), GNear == 1.0 && GFar == 0.0);
		SimFxAudio::BandGains(Bank.FarM + 1.0, Bank.NearM, Bank.FarM, GNear, GFar);
		TestTrue(TEXT("all far over far_m"), GNear < 1e-9 && GFar == 1.0);
	}
	// Every recording the bank names is played by something.
	TSet<FName> All;
	Bank.AllSamples(All);
	for (const TPair<FName, double>& R : Bank.RecordedAtM)
	{
		TestTrue(FString::Printf(TEXT("the recording %s is played"), *R.Key.ToString()), All.Contains(R.Key));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxAudioBlastTailTest, "SAM3D.Fx.Audio.BlastTail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxAudioBlastTailTest::RunTest(const FString& Parameters)
{
	// A 120 kg ground burst 6 km off rolls on under the recorded roll from its tail delay after it arrives, under the
	// blast's level by tail_db; 300 m off there is no roll (the near half of the crossfade).
	SimFxAudio::FBank Bank;
	if (!SimFxAudioTestLoadBank(Bank, *this))
	{
		return false;
	}
	const SimFxAudio::FBlastClassDef* Class = Bank.BlastFor(120.0);
	if (Class == nullptr || Class->Tail.IsNone())
	{
		AddError(TEXT("the 120 kg class has no tail"));
		return false;
	}
	const auto Run = [&](double DistM, double& OutTailAt, double& OutTailBelowDb)
	{
		SimFxAudio::FScene Scene;
		SimFxAudioTestSetUp(Scene, Bank);
		Scene.SetSampleSeconds(Class->Tail, 11.0);
		const SimFxAudio::FListener Ear = SimFxAudioTestEar();
		Scene.BeginFrame(0.0, SimFxAudioTestDt, Ear);
		SimFxAudio::FOneShotDesc D;
		D.Enu = simcore::Vec3{DistM, 0.0, 1.0};
		D.AltM = 1.0;
		D.AglM = 1.0;
		D.Near = Class->Near;
		D.Far = Class->Far;
		D.ChargeKg = 120.0;
		D.bGroundBurst = true;
		D.Tail = Class->Tail;
		D.TailDelayS = Class->TailDelayS;
		D.TailDb = Class->TailDb;
		const uint64 Key = Scene.AddOneShot(D);
		OutTailAt = -1.0;
		OutTailBelowDb = 0.0;
		for (double T = 0.0; T < DistM / 330.0 + 4.0; T += SimFxAudioTestDt)
		{
			Scene.BeginFrame(T, SimFxAudioTestDt, Ear);
			Scene.Mix(32, 1.0);
			const SimFxAudio::FVoice* Tail = Scene.VoiceFor(Key, Class->Tail);
			const SimFxAudio::FVoice* Blast = Scene.VoiceFor(Key, Class->Far);
			if (Tail != nullptr && OutTailAt < 0.0)
			{
				OutTailAt = T - Scene.OneShotStartedAt(Key);
				OutTailBelowDb = Blast != nullptr ? Blast->LevelDb - Tail->LevelDb : 0.0;
			}
		}
	};
	double TailAt = 0.0, Below = 0.0;
	Run(6000.0, TailAt, Below);
	TestTrue(FString::Printf(TEXT("far: the roll comes %.2f s after the blast arrives (tail_delay_s %.2f)"), TailAt, Class->TailDelayS),
		TailAt >= Class->TailDelayS - 0.02 && TailAt < Class->TailDelayS + 0.1);
	TestTrue(FString::Printf(TEXT("under it by %.1f dB (tail_db %.1f)"), Below, Class->TailDb), FMath::Abs(Below + Class->TailDb) < 0.5);
	Run(300.0, TailAt, Below);
	TestTrue(TEXT("near: no roll"), TailAt < 0.0);
	return true;
}

#endif
