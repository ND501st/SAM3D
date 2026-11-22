#include "SimFxAudio.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SAM3D.h"
#include "SimBasicModels.h"
#include "SimFxAudioScene.h"
#include "SimFxLogic.h"
#include "SimFxTuning.h"
#include "SimMunitionModels.h"
#include "SimOsaVehicle.h"
#include "SimWorldSubsystem.h"
#include "Sound/SoundWave.h"
#include "Stats/Stats.h"

#include <cmath>
#include <string>
#include <vector>

DECLARE_STATS_GROUP(TEXT("SAM3DFxAudio"), STATGROUP_SAM3DFxAudio, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Fx audio frame"), STAT_SimFxAudioFrame, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sources tracked"), STAT_SimFxAudioSources, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sources heard"), STAT_SimFxAudioHeard, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sounds audible"), STAT_SimFxAudioAudible, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("Voices playing"), STAT_SimFxAudioVoices, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("One-shots on their way"), STAT_SimFxAudioPending, STATGROUP_SAM3DFxAudio);
DECLARE_DWORD_COUNTER_STAT(TEXT("Booms heard"), STAT_SimFxAudioBooms, STATGROUP_SAM3DFxAudio);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Loudness window top (dB)"), STAT_SimFxAudioWindowTop, STATGROUP_SAM3DFxAudio);

namespace
{
	TAutoConsoleVariable<int32> CVarSimFxAudio(TEXT("sam3d.fx.Audio"), 1,
		TEXT("The weapons' and aircraft's sound (docs/fx/README.md 4.2): 1 on, 0 off."), ECVF_Default);
	TAutoConsoleVariable<float> CVarSimFxAudioVolume(TEXT("sam3d.fx.AudioVolume"), 1.0f,
		TEXT("The weapons' and aircraft's sound: its volume, 0 to 2."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarSimFxAudioMaxVoices(TEXT("sam3d.fx.MaxVoices"), 32,
		TEXT("The weapons' and aircraft's sound: voices for the loudest sounds (the rest are tracked, not played), 0 to 128."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarSimFxAudioRange(TEXT("sam3d.fx.AudioRange"), 0.0f,
		TEXT("The weapons' and aircraft's sound: how far (m) a continuous source is tracked; 0: data/fx/sounds.json's."),
		ECVF_Default);

	constexpr double SimFxAudioEarthRadiusM = 6371000.0;
	const TCHAR* const SimFxAudioAssetRoot = TEXT("/Game/SAM3D/FX/Sounds/");

	// Keys of the scene's continuous sources: an entity by its id, a wreck's fire apart.
	uint64 SimFxAudioEntityKey(int32 Id) { return static_cast<uint64>(static_cast<uint32>(Id)); }
	uint64 SimFxAudioWreckKey(int32 Id) { return (1ull << 40) | static_cast<uint64>(static_cast<uint32>(Id)); }
	uint64 SimFxAudioFlyerKey(uint64 N) { return (3ull << 48) | (N & 0xFFFFFFFFFFull); }

	// A site's missile's launch and its burst name the same shot by who fired, at whom and its number.
	std::string SimFxAudioShotId(const simcore::fx::Event& E)
	{
		return E.source + "|" + E.target + "|" + std::to_string(E.number);
	}

	std::string SimFxAudioToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString SimFxAudioToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	bool SimFxAudioIsZero(const simcore::Vec3& V) { return V.x == 0.0 && V.y == 0.0 && V.z == 0.0; }

	// A small deterministic spread (-1..1) for the n-th of a stick of bombs or a burst's rounds.
	double SimFxAudioSpread(uint64 Seed, int32 N)
	{
		const uint32 H = static_cast<uint32>((Seed + static_cast<uint64>(N) * 0x9E3779B97F4A7C15ull) * 2654435761ull >> 16);
		return (H % 20001u) / 10000.0 - 1.0;
	}
}

/** What the subsystem keeps between frames, out of the UObject (plain C++). */
struct FSimFxAudioState
{
	SimFxAudio::FBank Bank;
	double DataMaxRangeM = SimFxAudio::FBank().MaxRangeM;  // the range sounds.json gave (Bank.MaxRangeM unless sam3d.fx.AudioRange is set)
	SimFxAudio::FScene Scene;
	simcore::airwar::WeaponsDef Weapons;
	bool bWeapons = false;
	TSet<FName> LoopSamples;

	// The runner whose feed is read, and how far.
	const simcore::ScenarioRunner* Runner = nullptr;
	bool bStarted = false;
	uint64 Cursor = 0;
	double OriginAltM = 0.0;
	double AirAgeS = 1.0e9;

	// Each entity heard: what it sounds like (looked up once).
	struct FEntity
	{
		bool bResolved = false;
		const SimFxAudio::FProfileDef* Profile = nullptr;
		double LengthM = 15.0;
		bool bMissile = false;
		double LaunchS = 0.0;
		double BoostS = 0.0;
		double SustainS = 0.0;
		bool bDead = false;
		double GroundAltM = 0.0;
		double GroundAgeS = 1.0e9;
	};
	TMap<int32, FEntity> Entities;

	// Our missiles' launches (for the motor's start), the last few seconds.
	struct FLaunch
	{
		double TimeS = 0.0;
		simcore::Vec3 Enu;
	};
	TArray<FLaunch> Launches;

	// Wrecks falling and burning.
	struct FWreck
	{
		int32 Id = -1;
		simcore::fx::WreckFall Fall;
		double T0 = 0.0;
		bool bFire = false;
		bool bLanded = false;
	};
	TArray<FWreck> Wrecks;

	// Missiles that are no entity (a site's SamLaunch, the bridge's ArmLaunch): heard flying from their launch toward
	// their aim while their motors burn, until their burst.
	struct FFlyer
	{
		uint64 Key = 0;
		std::string Who;  // source|target|number: the burst that ends it
		const SimFxAudio::FProfileDef* Profile = nullptr;
		double T0 = 0.0, T1 = 1.0;
		simcore::Vec3 P0, V0, P1, V1;
		double BoostS = 0.0, SustainS = 0.0;
		double LengthM = 3.0;
		double EndS = 0.0;
		bool bBurst = false;

		/** Where it is at `T`: a cubic from the rails to its aim, then on along the way it was going. */
		simcore::Vec3 At(double T) const
		{
			const double H = FMath::Max(T1 - T0, 1.0e-3);
			if (T >= T1)
			{
				return P1 + V1 * (T - T1);
			}
			const double U = FMath::Clamp((T - T0) / H, 0.0, 1.0);
			const double U2 = U * U, U3 = U2 * U;
			return P0 * (2.0 * U3 - 3.0 * U2 + 1.0) + V0 * (H * (U3 - 2.0 * U2 + U)) + P1 * (3.0 * U2 - 2.0 * U3) +
				V1 * (H * (U3 - U2));
		}
	};
	TArray<FFlyer> Flyers;
	uint64 NextFlyerKey = 1;

	// The moment heard last frame (a jump back is a restart).
	double LastNowS = 0.0;
};

// ================================================================================================ lifecycle

USimFxAudioSubsystem::USimFxAudioSubsystem() = default;

bool USimFxAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

bool USimFxAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USimFxAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimFxAudioSubsystem, STATGROUP_Tickables);
}

void USimFxAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USimWorldSubsystem>();
	Super::Initialize(Collection);
	State = MakePimpl<FSimFxAudioState>();
}

void USimFxAudioSubsystem::Deinitialize()
{
	StopAll();
	VoicePool.Reset();
	AppliedGeneration.Reset();
	if (VoiceHost != nullptr)
	{
		VoiceHost->Destroy();
		VoiceHost = nullptr;
	}
	Waves.Reset();
	State.Reset();
	Super::Deinitialize();
}

int32 USimFxAudioSubsystem::GetActiveVoiceCount() const
{
	return State.IsValid() ? State->Scene.NumActiveVoices() : 0;
}

// ================================================================================================ data

void USimFxAudioSubsystem::LoadData()
{
	bDataLoaded = true;
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || !State.IsValid())
	{
		return;
	}
	const FString DataDir = Sim->GetDataDirectory();
	const FString Path = FPaths::Combine(DataDir, TEXT("fx/sounds.json"));
	FString Text;
	FString Error;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !State->Bank.LoadJson(SimFxAudioToStd(Text), Error))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Fx audio: %s did not load (%s): no weapons' or aircraft's sound"), *Path,
			Error.IsEmpty() ? TEXT("missing") : *Error);
		return;
	}
	State->DataMaxRangeM = State->Bank.MaxRangeM;  // sam3d.fx.AudioRange 0 comes back to it
	// The picture's rhythm (effects.json): the guns' bursts and the flak's fuzes, the rockets' and the bombs' ripples, so
	// what is heard is what is seen. Missing: sounds.json's own and the bank's defaults.
	{
		FSimFxTuning Tune;
		FString TuneError;
		if (Tune.Load(FPaths::Combine(DataDir, TEXT("fx/effects.json")), &TuneError))
		{
			SimFxAudio::FBank& B = State->Bank;
			B.GunBurstMinS = Tune.Guns.BurstMinS;
			B.GunBurstMaxS = Tune.Guns.BurstMaxS;
			B.GunPauseMinS = Tune.Guns.PauseMinS;
			B.GunPauseMaxS = Tune.Guns.PauseMaxS;
			B.GunBurstMinRounds = Tune.Guns.BurstMinRounds;
			B.FlakJitterS = Tune.Guns.FlakJitterS;
			B.FlakOffsetM = Tune.Guns.FlakOffsetM;
			B.RippleIntervalS = Tune.Bombs.RocketRippleS;
			B.StickIntervalS = Tune.Bombs.RippleS;
			B.StickMax = FMath::Max(Tune.Bombs.MaxPerRelease, 1);
			B.RippleMax = B.StickMax;
		}
		else
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Fx audio: fx/effects.json did not load (%s): the guns' rhythm is the bank's"), *TuneError);
		}
	}
	State->Scene.SetBank(&State->Bank);

	std::string WeaponsError;
	State->bWeapons = simcore::airwar::LoadWeaponsFile(SimFxAudioToStd(FPaths::Combine(DataDir, TEXT("air_war/weapons.json"))),
		State->Weapons, &WeaponsError);
	if (!State->bWeapons)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Fx audio: air_war/weapons.json did not load (%s): releases sound as bombs"),
			*SimFxAudioToF(WeaponsError));
	}

	// The samples that loop (engines, guns' bursts, fire).
	for (const TPair<FString, SimFxAudio::FProfileDef>& P : State->Bank.Profiles)
	{
		for (const SimFxAudio::FLayerDef& L : P.Value.Layers)
		{
			State->LoopSamples.Add(L.Sample);
		}
	}
	for (const TPair<FString, SimFxAudio::FGunDef>& G : State->Bank.Guns)
	{
		if (!G.Value.Loop.IsNone())
		{
			State->LoopSamples.Add(G.Value.Loop);
		}
	}

	// Every sample, loaded once; a missing one is logged once and never played.
	TSet<FName> All;
	State->Bank.AllSamples(All);
	TArray<FString> Missing;
	for (const FName Name : All)
	{
		const FString Asset = FString::Printf(TEXT("%s%s.%s"), SimFxAudioAssetRoot, *Name.ToString(), *Name.ToString());
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *Asset, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Wave == nullptr)
		{
			Missing.Add(Name.ToString());
			continue;
		}
		if (State->LoopSamples.Contains(Name) && !Wave->bLooping)
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Fx audio: %s is played as a loop but its asset does not loop (set here; rerun tools/ue_make_fx_assets.py)"),
				*Name.ToString());
			Wave->bLooping = true;
		}
		Waves.Add(Name, Wave);
		State->Scene.SetSampleSeconds(Name, Wave->Duration > 0.0f && Wave->Duration < 1000.0f ? Wave->Duration : 4.0);
	}
	if (Missing.Num() > 0)
	{
		Missing.Sort();
		UE_LOG(LogSAM3D, Warning, TEXT("Fx audio: %d of %d samples missing under %s (never played): %s"), Missing.Num(), All.Num(),
			SimFxAudioAssetRoot, *FString::Join(Missing, TEXT(", ")));
	}
	UE_LOG(LogSAM3D, Log, TEXT("Fx audio: %d profiles, %d samples loaded"), State->Bank.Profiles.Num(), Waves.Num());
}

USoundWave* USimFxAudioSubsystem::WaveFor(FName Sample) const
{
	const TObjectPtr<USoundWave>* W = Waves.Find(Sample);
	return W != nullptr ? W->Get() : nullptr;
}

// ================================================================================================ voices

void USimFxAudioSubsystem::EnsureVoices(int32 Count)
{
	UWorld* World = GetWorld();
	if (World == nullptr || VoicePool.Num() >= Count)
	{
		return;
	}
	if (VoiceHost == nullptr)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VoiceHost = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (VoiceHost == nullptr)
		{
			return;
		}
		VoiceHost->SetReplicates(false);
#if WITH_EDITOR
		VoiceHost->SetActorLabel(TEXT("SimFxAudioVoices"));
#endif
	}
	while (VoicePool.Num() < Count)
	{
		UAudioComponent* Audio = NewObject<UAudioComponent>(VoiceHost);
		Audio->bAutoActivate = false;
		Audio->bAutoDestroy = false;
		Audio->bStopWhenOwnerDestroyed = true;
		Audio->bIsUISound = false;
		Audio->bAllowSpatialization = true;
		// The engine only places it (pans it): the level, the low-pass and the pitch are set here each frame.
		Audio->bOverrideAttenuation = true;
		Audio->AttenuationOverrides.bAttenuate = false;
		Audio->AttenuationOverrides.bSpatialize = true;
		Audio->AttenuationOverrides.bAttenuateWithLPF = false;
		Audio->AttenuationOverrides.bEnableOcclusion = false;
		Audio->AttenuationOverrides.bEnableListenerFocus = false;
		Audio->AttenuationOverrides.bEnableReverbSend = false;
		Audio->AttenuationOverrides.bEnablePriorityAttenuation = false;
		Audio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(2.0e6f, 0.0f, 0.0f);
		Audio->AttenuationOverrides.FalloffDistance = 1.0e5f;
		Audio->bEnableLowPassFilter = true;
		Audio->LowPassFilterFrequency = 20000.0f;
		// Under the crew's voices: an effect gives way to a word.
		Audio->bOverridePriority = true;
		Audio->Priority = 0.5f;
		Audio->SetUsingAbsoluteLocation(true);
		Audio->SetUsingAbsoluteRotation(true);
		Audio->RegisterComponent();
		VoicePool.Add(Audio);
		AppliedGeneration.Add(0);
	}
}

void USimFxAudioSubsystem::StopAll()
{
	for (int32 I = 0; I < VoicePool.Num(); ++I)
	{
		if (UAudioComponent* Audio = VoicePool[I])
		{
			if (Audio->IsPlaying())
			{
				Audio->Stop();
			}
		}
		AppliedGeneration[I] = 0;
	}
	bVoicesPaused = false;
}

void USimFxAudioSubsystem::PauseAll(bool bPause)
{
	if (bVoicesPaused == bPause)
	{
		return;
	}
	bVoicesPaused = bPause;
	for (UAudioComponent* Audio : VoicePool)
	{
		if (Audio != nullptr && Audio->IsPlaying())
		{
			Audio->SetPaused(bPause);
		}
	}
}

void USimFxAudioSubsystem::Present()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const TArray<SimFxAudio::FVoice>& Voices = State->Scene.GetVoices();
	EnsureVoices(Voices.Num());
	for (int32 I = 0; I < VoicePool.Num(); ++I)
	{
		UAudioComponent* Audio = VoicePool[I];
		if (Audio == nullptr)
		{
			continue;
		}
		if (I >= Voices.Num() || !Voices[I].bActive)
		{
			if (AppliedGeneration[I] != 0 || Audio->IsPlaying())
			{
				Audio->Stop();
				AppliedGeneration[I] = 0;
			}
			continue;
		}
		const SimFxAudio::FVoice& V = Voices[I];
		Audio->SetWorldLocation(Sim->ToUnreal(V.Enu));
		Audio->SetVolumeMultiplier(static_cast<float>(V.Volume));
		Audio->SetPitchMultiplier(static_cast<float>(V.Pitch));
		Audio->SetLowPassFilterFrequency(static_cast<float>(V.LowPassHz));
		if (AppliedGeneration[I] != V.Generation)
		{
			AppliedGeneration[I] = V.Generation;
			USoundWave* Wave = WaveFor(V.Sample);
			if (Audio->IsPlaying())
			{
				Audio->Stop();
			}
			if (Wave == nullptr)
			{
				continue;
			}
			Audio->SetSound(Wave);
			Audio->SetLowPassFilterEnabled(true);
			Audio->Play(static_cast<float>(FMath::Max(V.StartOffsetS, 0.0)));
		}
	}
}

// ================================================================================================ the frame

namespace
{
	/** The frame's work outside the UObject: sources, wrecks and the feed's events into the scene. */
	struct FSimFxAudioFrame
	{
		FSimFxAudioState& S;
		USimWorldSubsystem& Sim;
		simcore::ScenarioRunner* Runner;
		const simcore::World& World;
		double Now;

		const SimFxAudio::FBank& Bank() const { return S.Bank; }

		double AltOf(const simcore::Vec3& P) const
		{
			return S.OriginAltM + P.z + (P.x * P.x + P.y * P.y) / (2.0 * SimFxAudioEarthRadiusM);
		}
		double GroundAltAt(const simcore::Vec3& P) const { return World.GetTerrain().HeightAt(P.x, P.y); }
		double AglOf(const simcore::Vec3& P) const { return AltOf(P) - GroundAltAt(P); }
		double GroundZ(const simcore::Vec3& P) const
		{
			if (Runner != nullptr)
			{
				return Runner->GroundEnu(P.x, P.y).z;
			}
			return GroundAltAt(P) - S.OriginAltM - (P.x * P.x + P.y * P.y) / (2.0 * SimFxAudioEarthRadiusM);
		}

		const simcore::scenery::MunitionSpec* Munition(const std::string& Id) const
		{
			return Id.empty() ? nullptr : FSimMunitionModels::Resolve(SimFxAudioToF(Id));
		}

		/** A warhead's TNT equivalent: the event's, else the munition's, else a share of the weapon's mass, else Default. */
		double ChargeOf(const std::string& Weapon, double EventKg, double Default) const
		{
			if (EventKg > 0.0)
			{
				return EventKg;
			}
			if (const simcore::scenery::MunitionSpec* M = Munition(Weapon))
			{
				if (M->charge_kg > 0.0)
				{
					return M->charge_kg;
				}
			}
			if (S.bWeapons)
			{
				if (const simcore::airwar::WeaponDef* W = S.Weapons.Find(Weapon); W != nullptr && W->mass_kg > 0.0)
				{
					return W->mass_kg * Bank().ChargeShareOfMass;
				}
			}
			return Default;
		}

		simcore::airwar::WeaponPath PathOf(const std::string& Weapon) const
		{
			return simcore::airwar::WeaponPathOf(Weapon, S.bWeapons ? &S.Weapons : nullptr);
		}

		bool IsCluster(const std::string& Weapon) const
		{
			if (S.bWeapons)
			{
				if (const simcore::airwar::WeaponDef* W = S.Weapons.Find(Weapon))
				{
					return W->kind == "cluster";
				}
			}
			const simcore::scenery::MunitionSpec* M = Munition(Weapon);
			return M != nullptr && M->has_cluster;
		}

		SimFxAudio::FOneShotDesc At(double T, const simcore::Vec3& P, const SimFxAudio::FEventSoundDef& Sound) const
		{
			SimFxAudio::FOneShotDesc D;
			D.TEventS = T;
			D.Enu = P;
			D.AltM = AltOf(P);
			D.AglM = FMath::Max(D.AltM - GroundAltAt(P), 0.0);
			D.Near = Sound.Sample;
			D.LevelDb1m = Sound.LevelDb1m;
			D.Pitch = Sound.Pitch;
			return D;
		}

		/** An explosion of `ChargeKg` at P: on the ground if it is near it, else in the air; near and far samples. */
		void Blast(double T, const simcore::Vec3& P, double ChargeKg, bool bDebris = true)
		{
			const SimFxAudio::FBlastClassDef* Class = Bank().BlastFor(ChargeKg);
			if (Class == nullptr || ChargeKg <= 0.0)
			{
				return;
			}
			SimFxAudio::FOneShotDesc D = At(T, P, SimFxAudio::FEventSoundDef());
			const bool bGround = D.AglM < Bank().AirBurstAglM;
			D.Near = bGround ? Class->Near : Class->AirNear;
			D.Far = bGround ? Class->Far : Class->AirFar;
			D.ChargeKg = ChargeKg;
			D.bGroundBurst = bGround;
			D.Pitch = FMath::Clamp(std::pow(Class->RefKg / ChargeKg, Bank().BlastPitchExponent), 0.75, 1.3);
			D.Tail = Class->Tail;  // far off, the recorded roll under it
			D.TailDelayS = Class->TailDelayS;
			D.TailDb = Class->TailDb;
			S.Scene.AddOneShot(D);
			if (bDebris && bGround && ChargeKg >= 20.0 && !Bank().Debris.Sample.IsNone())
			{
				SimFxAudio::FOneShotDesc Rain = At(T + 0.3, P, Bank().Debris);
				Rain.MaxRangeM = Bank().DebrisRangeM;
				S.Scene.AddOneShot(Rain);
			}
		}

		/** A missile that is no entity leaves its rails: its motor heard on its way toward its aim (a big one as a large
		 *  rocket), until its burst ends it (EndFlyer) or a while after it should have arrived. */
		void LaunchFlyer(const simcore::fx::Event& E)
		{
			const simcore::scenery::MunitionSpec* Spec = Munition(E.weapon);
			const SimFxAudio::FProfileDef* Profile = Spec != nullptr && Spec->length_m >= Bank().LargeRocketFromLengthM
				? Bank().Profile(Bank().LargeRocketProfile) : nullptr;
			if (Profile == nullptr)
			{
				Profile = Bank().Profile(Bank().MissileProfile);
			}
			if (Profile == nullptr)
			{
				return;
			}
			FSimFxAudioState::FFlyer F;
			F.Key = SimFxAudioFlyerKey(S.NextFlyerKey++);
			F.Who = SimFxAudioShotId(E);
			F.Profile = Profile;
			const bool bAim = !SimFxAudioIsZero(E.aim);
			const simcore::Vec3 Up{0.0, 0.0, 1.0};
			const simcore::Vec3 Heading = E.velocity.LengthSquared() > 1.0 ? E.velocity.Normalized() : Up;
			F.P0 = E.position;
			F.P1 = bAim ? E.aim : E.position + Heading * 5000.0;
			const simcore::Vec3 Run = F.P1 - F.P0;
			const double Dist = Run.Length();
			const simcore::Vec3 Dir = Dist > 1.0 ? Run / Dist : Heading;
			F.T0 = E.t_s;
			F.T1 = E.end_t_s > E.t_s + 0.5 ? E.end_t_s : E.t_s + FMath::Clamp(Dist / Bank().FlyerMps, 2.0, 60.0);
			const double H = F.T1 - F.T0;
			F.V0 = E.velocity.LengthSquared() > 1.0 ? E.velocity : Dir * 30.0;
			F.V1 = Dir * (1.3 * Dist / H);  // faster at the end than its mean: it left the rails slowly
			F.BoostS = Spec != nullptr && Spec->has_motor ? Spec->motor.boost_s : Bank().MissileBoostS;
			F.SustainS = Spec != nullptr && Spec->has_motor ? Spec->motor.sustain_s : Bank().MissileSustainS;
			F.LengthM = Spec != nullptr && Spec->length_m > 0.0 ? Spec->length_m : 3.0;
			F.EndS = F.T1 + 10.0;
			S.Flyers.Add(MoveTemp(F));
		}

		/** Its burst: the shot's flyer goes silent then (the oldest of that name still flying). */
		void EndFlyer(const simcore::fx::Event& E, double AtS)
		{
			const std::string Who = SimFxAudioShotId(E);
			for (FSimFxAudioState::FFlyer& F : S.Flyers)
			{
				if (!F.bBurst && F.Who == Who)
				{
					F.bBurst = true;
					F.EndS = FMath::Max(AtS, F.T0);
					return;
				}
			}
		}

		/** How long an aircraft's gun fires (the picture's gun run): its rounds at the munition's rate, 0.5-4 s. */
		double GunRunS(const simcore::fx::Event& E, const SimFxAudio::FGunDef& G) const
		{
			const simcore::scenery::MunitionSpec* Spec = Munition(E.weapon);
			const double RateHz = Spec != nullptr && Spec->rate_rpm > 0.0 ? FMath::Max(Spec->rate_rpm, 60.0) / 60.0 : G.RateHz;
			return FMath::Clamp(FMath::Max(E.number, 1) / RateHz, 0.5, 4.0);
		}

		/** A burst now, or at its own later time when it flew on past its target to destroy itself. */
		static double BurstTime(const simcore::fx::Event& E)
		{
			const bool bLater = E.end_t_s > E.t_s + 0.05 && E.end != simcore::fx::BurstEnd::Hit &&
				E.end != simcore::fx::BurstEnd::NearBurst;
			return bLater ? E.end_t_s : E.t_s;
		}

		/** The guns of an air defence site through a pass, in the bursts the picture fires (SimFxLogic::GunBursts on the
		 *  event's seq and effects.json's rhythm, the munition's rate): a burst of its loop with a tail, or its single
		 *  reports; the rounds' cracks if the listener is near the line they fly beyond the gun; a flak gun's shells
		 *  bursting round the aim at their time of flight, a light gun's rounds a few of them destroying themselves. */
		void GunPass(const simcore::fx::Event& E)
		{
			const SimFxAudio::FGunDef* G = Bank().GunFor(SimFxAudioToF(E.weapon));
			if (G == nullptr)
			{
				return;
			}
			const simcore::scenery::MunitionSpec* Spec = Munition(E.weapon);
			const double T0 = E.t_s;
			const double T1 = E.end_t_s > E.t_s ? E.end_t_s : E.t_s + 6.0;  // as the picture's pass
			const simcore::Vec3 Site = E.position + simcore::Vec3{0.0, 0.0, 2.0};
			const simcore::Vec3 Aim = SimFxAudioIsZero(E.aim) ? Site + simcore::Vec3{0.0, 0.0, 1000.0} : E.aim;
			const simcore::Vec3 Line = (Aim - Site).Normalized();
			const simcore::Vec3 L = S.Scene.GetListener().Enu;
			const double Along = simcore::Dot(L - Site, Line);
			const simcore::Vec3 Closest = Site + Line * Along;
			const bool bCracks = Along > 20.0 && Along < 3000.0 && (L - Closest).Length() < Bank().CrackRangeM &&
				!Bank().Crack.Sample.IsNone();
			const double MuzzleMps = Spec != nullptr && Spec->muzzle_mps > 0.0 ? Spec->muzzle_mps : G->MuzzleMps;
			const double RateHz = Spec != nullptr && Spec->rate_rpm > 0.0 ? FMath::Max(Spec->rate_rpm, 30.0) / 60.0 : G->RateHz;
			const double Calibre = Spec != nullptr && Spec->calibre_mm > 0.0 ? Spec->calibre_mm : 23.0;
			const bool bFlak = Spec != nullptr && Spec->burst == "time";  // as the picture: a time fuze bursts round the target's height
			const double Flight = Along / FMath::Max(MuzzleMps, 100.0);
			const auto Crack = [&](double T)
			{
				SimFxAudio::FOneShotDesc C = At(T, Closest, Bank().Crack);
				C.MaxRangeM = Bank().CrackRangeM * 1.5;
				S.Scene.AddOneShot(C);
			};
			// A round's flight to the aim (the picture's lead and drag; the target's motion is not known here).
			const double DragK = simcore::fx::kDragKTypical23mm * 23.0 / Calibre;
			double FlakTof = 0.0;
			simcore::fx::LeadAim(Site, Aim, simcore::Vec3{}, MuzzleMps, DragK, &FlakTof);
			const double ChargeKg = Spec != nullptr && Spec->charge_kg > 0.0 ? Spec->charge_kg : Bank().FlakDefaultKg;
			const bool bSelfDestructs = Spec != nullptr && Spec->sd_hi_s > 0.0 && !bFlak && Bank().SdPopsPerBurst > 0;

			TArray<SimFxLogic::FGunBurst> Bursts;
			// The picture's bursts: a slow gun's hold a few rounds (FSimFxWorld::StartGunPass).
			double BurstMinS = 0.0, BurstMaxS = 0.0;
			SimFxLogic::GunBurstSpan(Bank().GunBurstMinS, Bank().GunBurstMaxS,
				SimFxLogic::NamedIntFor(Bank().GunBurstMinRounds, Spec != nullptr ? FString(UTF8_TO_TCHAR(Spec->id.c_str())) : FString(), SimFxAudioToF(E.weapon), 1),
				RateHz, BurstMinS, BurstMaxS);
			SimFxLogic::GunBursts(static_cast<uint32>(E.seq), T0, T1, BurstMinS, BurstMaxS,
				Bank().GunPauseMinS, Bank().GunPauseMaxS, Bursts);
			TArray<double> Shots;
			const uint64 Seed = static_cast<uint64>(E.seq);
			int32 Round = 0;
			for (int32 K = 0; K < Bursts.Num(); ++K)
			{
				const SimFxLogic::FGunBurst& B = Bursts[K];
				SimFxLogic::GunBurstShots(B, RateHz, T1, Shots);
				if (!G->Loop.IsNone())
				{
					// The loop for as long as its rounds leave the barrels (a round's worth at least), then its tail.
					const double Len = FMath::Max(Shots.Last() - B.StartS + 1.0 / RateHz, 0.1);
					SimFxAudio::FOneShotDesc D = At(B.StartS, Site, SimFxAudio::FEventSoundDef{G->Loop, G->LevelDb1m, G->Pitch});
					D.LoopForS = Len;
					S.Scene.AddOneShot(D);
					if (!G->Tail.IsNone())
					{
						S.Scene.AddOneShot(At(B.StartS + Len, Site, SimFxAudio::FEventSoundDef{G->Tail, G->LevelDb1m - 6.0, 1.0}));
					}
				}
				for (int32 J = 0; J < Shots.Num(); ++J, ++Round)
				{
					const double T = Shots[J];
					if (G->Loop.IsNone() && !G->Single.IsNone())
					{
						S.Scene.AddOneShot(At(T, Site, SimFxAudio::FEventSoundDef{G->Single, G->LevelDb1m, G->Pitch}));
					}
					if (bCracks && (J == 0 || J == Shots.Num() - 1 || J % 4 == 0))
					{
						Crack(T + Flight);
					}
					if (bFlak)
					{
						// Its shell bursting where the picture's does: round the aim, the fuze's time a little off.
						const double Tof = FMath::Max(FlakTof + Bank().FlakJitterS * SimFxAudioSpread(Seed + 11, Round), 0.5);
						const simcore::Vec3 Off{Bank().FlakOffsetM * SimFxAudioSpread(Seed + 13, Round),
							Bank().FlakOffsetM * SimFxAudioSpread(Seed + 17, Round), Bank().FlakOffsetM * SimFxAudioSpread(Seed + 19, Round)};
						Blast(T + Tof, Aim + Off, ChargeKg, false);
					}
				}
				if (bSelfDestructs)
				{
					// A few of the burst's rounds going off at the end of their flight (the picture's grey puffs).
					simcore::fx::RoundParams P;
					P.muzzle_mps = MuzzleMps;
					P.drag_k = DragK;
					const simcore::Vec3 Dir = simcore::fx::LeadAim(Site, Aim, simcore::Vec3{}, MuzzleMps, DragK);
					for (int32 J = 0; J < Bank().SdPopsPerBurst; ++J)
					{
						const double U = 0.5 + 0.5 * SimFxAudioSpread(Seed + 23, K * 16 + J);
						const double Sd = Spec->sd_lo_s + (Spec->sd_hi_s - Spec->sd_lo_s) * U;
						const double Fired = B.StartS + (B.EndS - B.StartS) * (0.5 + 0.5 * SimFxAudioSpread(Seed + 29, K * 16 + J));
						Blast(Fired + Sd, simcore::fx::RoundAt(Site, Dir, P, Sd).position, ChargeKg, false);
					}
				}
			}
		}

		/** The simulation's time-fuzed shell (FlakBurst): its gun's report as it is fired, its burst where and when the
		 *  simulation has it. */
		void FlakShell(const simcore::fx::Event& E)
		{
			const SimFxAudio::FGunDef* G = Bank().GunFor(SimFxAudioToF(E.weapon));
			if (G != nullptr && !G->Single.IsNone())
			{
				S.Scene.AddOneShot(At(E.t_s, E.position, SimFxAudio::FEventSoundDef{G->Single, G->LevelDb1m, G->Pitch}));
			}
			const simcore::scenery::MunitionSpec* Spec = Munition(E.weapon);
			const double ChargeKg = Spec != nullptr && Spec->charge_kg > 0.0 ? Spec->charge_kg : Bank().FlakDefaultKg;
			Blast(E.end_t_s, E.aim, ChargeKg, false);
		}

		void Event(const simcore::fx::Event& E)
		{
			using simcore::fx::EventKind;
			using simcore::airwar::WeaponPath;
			const simcore::Vec3 Aim = SimFxAudioIsZero(E.aim) ? E.position : E.aim;
			switch (E.kind)
			{
			case EventKind::MissileLaunch:
				S.Scene.AddOneShot(At(E.t_s, E.position, Bank().Launch));
				S.Launches.Add({E.t_s, E.position});
				break;
			case EventKind::MissileEnd:
				// A burst now (Hit, NearBurst), or where it destroys itself after flying on (Wide, Early).
				Blast(BurstTime(E), Aim, ChargeOf(E.weapon.empty() ? std::string("9m33") : E.weapon, E.charge_kg,
					Bank().WarheadDefaultKg));
				break;
			case EventKind::SamLaunch:
				S.Scene.AddOneShot(At(E.t_s, E.position, Bank().Launch));
				LaunchFlyer(E);
				break;
			case EventKind::SamEnd:
				// On the target, or flown on past it to destroy itself (SelfDestruct: at its end time and aim).
				Blast(BurstTime(E), Aim, ChargeOf(E.weapon, E.charge_kg, Bank().SamDefaultKg));
				EndFlyer(E, BurstTime(E));
				break;
			case EventKind::AircraftDown:
			{
				Blast(E.t_s, E.position, Bank().KillBlastKg, false);
				if (!Bank().Fireball.Sample.IsNone())
				{
					// Its fuel going up (the picture's rolling fireball, LOOK 6): a deep whoomph and a few seconds' roar.
					S.Scene.AddOneShot(At(E.t_s, E.position, Bank().Fireball));
				}
				if (E.world_id >= 0 && (E.fire || E.falls) && !S.Wrecks.ContainsByPredicate([&E](const FSimFxAudioState::FWreck& W) { return W.Id == E.world_id; }))
				{
					simcore::fx::WreckFallParams P;
					P.pos = E.position;
					P.vel = E.velocity;
					P.ground_z_m = GroundZ(E.position);
					P.ground_z_at = [this](double X, double Y) { return GroundZ(simcore::Vec3{X, Y, 0.0}); };  // the visuals' ground: the same landing
					P.seed = static_cast<uint32>(E.world_id) * 2654435761u ^ 0x5A17u;  // the visuals' rule: the same fall
					P.falls = E.falls;
					FSimFxAudioState::FWreck W;
					W.Id = E.world_id;
					W.Fall = simcore::fx::WreckFall(P);
					W.T0 = E.t_s;
					W.bFire = E.fire;
					S.Wrecks.Add(MoveTemp(W));
				}
				break;
			}
			case EventKind::WeaponRelease:
			{
				const WeaponPath Path = PathOf(E.weapon);
				if (Path == WeaponPath::Gun)
				{
					// The aircraft's gun: its loop at the aircraft for as long as the picture's run fires.
					if (const SimFxAudio::FGunDef* G = Bank().GunFor(SimFxAudioToF(E.weapon)))
					{
						SimFxAudio::FOneShotDesc D = At(E.t_s, E.position,
							SimFxAudio::FEventSoundDef{G->Loop.IsNone() ? G->Single : G->Loop, G->LevelDb1m, G->Pitch});
						D.LoopForS = G->Loop.IsNone() ? 0.0 : GunRunS(E, *G);
						S.Scene.AddOneShot(D);
					}
				}
				else if (Path == WeaponPath::Rocket)
				{
					// A ripple of rockets leaving the pods.
					const int32 N = FMath::Clamp(E.number, 1, FMath::Max(Bank().RippleMax, 1));
					for (int32 I = 0; I < N; ++I)
					{
						const double Dt = I * Bank().RippleIntervalS;
						S.Scene.AddOneShot(At(E.t_s + Dt, E.position + E.velocity * Dt, Bank().RocketLaunch));
					}
				}
				else if (Path == WeaponPath::Missile || Path == WeaponPath::Atgm)
				{
					S.Scene.AddOneShot(At(E.t_s, E.position, Bank().ArmLaunch));
				}
				else if (E.end_t_s > E.t_s && !Bank().BombRush.Sample.IsNone())
				{
					// A bomb heard in its last seconds, by whoever is near where it falls: the rush ends on its burst.
					SimFxAudio::FOneShotDesc D = At(E.end_t_s - Bank().RushLeadS, Aim + simcore::Vec3{0.0, 0.0, 20.0}, Bank().BombRush);
					D.StartOffsetS = FMath::Max(0.0, S.Scene.SampleSecondsOf(Bank().BombRush.Sample) - Bank().RushLeadS);
					D.MaxRangeM = Bank().RushRangeM;
					S.Scene.AddOneShot(D);
				}
				break;
			}
			case EventKind::WeaponImpact:
			{
				const WeaponPath Path = PathOf(E.weapon);
				const uint64 Seed = static_cast<uint64>(E.seq);
				if (E.delivery == "jettison")
				{
					break;  // thrown off unarmed: it lands without going off (the picture's puff of dirt)
				}
				if (Path == WeaponPath::Gun)
				{
					// The rounds striking over the run's length (the picture walks them along the ground as long).
					const SimFxAudio::FGunDef* G = Bank().GunFor(SimFxAudioToF(E.weapon));
					const double RunS = G != nullptr ? GunRunS(E, *G) : 1.0;
					const int32 N = FMath::Max(Bank().GunImpactBursts, 1);
					for (int32 I = 0; I < N; ++I)
					{
						const simcore::Vec3 P = Aim + simcore::Vec3{8.0 * SimFxAudioSpread(Seed, I), 8.0 * SimFxAudioSpread(Seed + 7, I), 0.0};
						Blast(E.t_s + RunS * I / N, P, Bank().GunImpactKg, false);
					}
				}
				else if (IsCluster(E.weapon) && !Bank().Cluster.Sample.IsNone())
				{
					S.Scene.AddOneShot(At(E.t_s, Aim, Bank().Cluster));
				}
				else
				{
					// Each bomb of a stick (or rocket of a ripple) bursts at its own time and place.
					const double Charge = ChargeOf(E.weapon, E.charge_kg, Bank().WarheadDefaultKg);
					const int32 N = FMath::Clamp(E.number, 1, FMath::Max(Bank().StickMax, 1));
					for (int32 I = 0; I < N; ++I)
					{
						const simcore::Vec3 P = Aim + simcore::Vec3{Bank().StickSpreadM * SimFxAudioSpread(Seed, I),
							Bank().StickSpreadM * SimFxAudioSpread(Seed + 3, I), 0.0};
						Blast(E.t_s + I * Bank().StickIntervalS, P, Charge, I == 0);
					}
				}
				break;
			}
			case EventKind::CruiseImpact:
				Blast(E.t_s, Aim, ChargeOf(E.weapon, E.charge_kg, Bank().CruiseDefaultKg));
				break;
			case EventKind::ArmLaunch:
				// The bridge's ARM model (an EM ARM is an entity and has no launch here): its motor on its way.
				S.Scene.AddOneShot(At(E.t_s, E.position, Bank().ArmLaunch));
				LaunchFlyer(E);
				break;
			case EventKind::ArmBurst:
				Blast(E.t_s, E.position, ChargeOf(E.weapon, E.charge_kg, Bank().ArmDefaultKg));
				EndFlyer(E, E.t_s);
				break;
			case EventKind::GunPass:
				GunPass(E);
				break;
			case EventKind::FlakBurst:
				FlakShell(E);
				break;
			case EventKind::AircraftDamaged:
				break;  // its smoke and fire are silent beside its engines
			case EventKind::ChaffDrop:
				break;  // a chaff cartridge's squib is lost in the engines
			case EventKind::FlareDrop:
				// Each flare's pop out of the dispenser as the picture throws it (roadmap B13), a few of a long salvo.
				if (!Bank().FlarePop.Sample.IsNone())
				{
					const int32 N = FMath::Clamp(E.number, 1, FMath::Max(Bank().FlarePopsMax, 1));
					for (int32 I = 0; I < N; ++I)
					{
						const double Dt = I * Bank().FlarePopS;
						S.Scene.AddOneShot(At(E.t_s + Dt, E.position + E.velocity * Dt, Bank().FlarePop));
					}
				}
				break;
			case EventKind::AamLaunch:
				// An air-to-air missile off its rail: the igniter's crack and its motor heard on its way to the target.
				if (!Bank().AamLaunch.Sample.IsNone())
				{
					S.Scene.AddOneShot(At(E.t_s, E.position, Bank().AamLaunch));
				}
				LaunchFlyer(E);
				break;
			case EventKind::AamEnd:
				// On the target now; a miss bursts where it destroys itself further on (the picture's fly-on); a missile whose
				// target was gone ends where it is, unheard.
				if (E.end == simcore::fx::BurstEnd::Hit)
				{
					Blast(E.t_s, Aim, ChargeOf(E.weapon, E.charge_kg, Bank().WarheadDefaultKg));
					EndFlyer(E, E.t_s);
				}
				else if (E.end == simcore::fx::BurstEnd::Wide)
				{
					const double T1 = E.t_s + Bank().AamFlyOnS;
					const simcore::Vec3 Run = E.velocity.LengthSquared() > 1.0 ? E.velocity.Normalized() : simcore::Vec3{0.0, 0.0, 0.0};
					Blast(T1, E.position + Run * (600.0 * Bank().AamFlyOnS), ChargeOf(E.weapon, E.charge_kg, Bank().WarheadDefaultKg));
					EndFlyer(E, T1);
				}
				else
				{
					EndFlyer(E, E.t_s);
				}
				break;
			case EventKind::AirGunBurst:
				// A fighter's gun at the shooter for the burst's seconds (its loop, the munition's own rate if it has one).
				if (const SimFxAudio::FGunDef* G = Bank().GunFor(SimFxAudioToF(E.weapon)))
				{
					SimFxAudio::FOneShotDesc D = At(E.t_s, E.position,
						SimFxAudio::FEventSoundDef{G->Loop.IsNone() ? G->Single : G->Loop, G->LevelDb1m, G->Pitch});
					D.LoopForS = G->Loop.IsNone() ? 0.0 : FMath::Clamp(E.end_t_s - E.t_s, 0.2, 4.0);
					S.Scene.AddOneShot(D);
				}
				break;
			}
		}

		void ReadFeed()
		{
			if (Runner == nullptr)
			{
				return;
			}
			const simcore::fx::Feed& Feed = Runner->Fx();
			if (Feed.Last() <= S.Cursor)
			{
				return;
			}
			const std::vector<simcore::fx::Event> New = Feed.Since(S.Cursor);
			S.Cursor = Feed.Last();
			for (const simcore::fx::Event& E : New)
			{
				Event(E);
			}
			S.Launches.RemoveAll([this](const FSimFxAudioState::FLaunch& L) { return Now - L.TimeS > 10.0; });
		}

		/** Each aircraft and missile in the snapshot as a continuous source. */
		void Entities(double RealDtS)
		{
			const simcore::scenery::ModelLibrary* Models = FSimBasicModels::Library();
			const double SimNow = Sim.GetSimTimeSeconds();
			for (const FSimEntityState& St : Sim.GetEntityStatesCached())
			{
				if (St.bIsGround)
				{
					continue;
				}
				FSimFxAudioState::FEntity& Info = S.Entities.FindOrAdd(St.Id);
				const double T = St.SimTimeSeconds > 0.0 ? St.SimTimeSeconds : SimNow;
				if (!Info.bResolved)
				{
					Info.bResolved = true;
					Info.bMissile = St.bIsMissile;
					const std::string ModelId = SimFxAudioToStd(St.Model);
					const simcore::scenery::ModelDef* M = Models != nullptr ? Models->Find(ModelId) : nullptr;
					const FString Arch = M != nullptr ? SimFxAudioToF(M->archetype) : FString(St.bIsMissile ? TEXT("missile") : TEXT(""));
					const bool bFlies = St.bIsMissile || Arch == TEXT("jet") || Arch == TEXT("prop") || Arch == TEXT("helicopter") ||
						Arch == TEXT("drone") || Arch == TEXT("missile");
					Info.LengthM = M != nullptr && M->length_m > 0.0 ? M->length_m : (St.bIsMissile ? 3.0 : Bank().BoomDefaultLengthM);
					Info.Profile = bFlies ? Bank().ProfileFor(St.Model, Arch, Info.LengthM) : nullptr;
					if (St.bIsMissile)
					{
						if (Info.Profile == nullptr)
						{
							Info.Profile = Bank().Profile(Bank().MissileProfile);
						}
						const simcore::scenery::MunitionSpec* Spec = FSimMunitionModels::Resolve(St.Model);
						if (Spec == nullptr)
						{
							Spec = FSimMunitionModels::Resolve(St.Name);
						}
						Info.BoostS = Spec != nullptr && Spec->has_motor ? Spec->motor.boost_s : Bank().MissileBoostS;
						Info.SustainS = Spec != nullptr && Spec->has_motor ? Spec->motor.sustain_s : Bank().MissileSustainS;
						// Its launch: our feed's, if one was heard of here just before it was first seen; else first sight.
						Info.LaunchS = T;
						const simcore::Vec3 P = Sim.ToEnu(St.Location);
						for (const FSimFxAudioState::FLaunch& L : S.Launches)
						{
							if (L.TimeS <= T + 0.1 && T - L.TimeS < 3.0 && (L.Enu - P).Length() < 2000.0)
							{
								Info.LaunchS = L.TimeS;
							}
						}
					}
				}
				if (Info.Profile == nullptr)
				{
					continue;
				}
				const uint64 Key = SimFxAudioEntityKey(St.Id);
				if (!St.bAlive)
				{
					if (!Info.bDead)
					{
						Info.bDead = true;
						S.Scene.EndSource(Key, T);
					}
					continue;
				}
				Info.bDead = false;
				SimFxAudio::FSourceUpdate U;
				U.Key = Key;
				U.Profile = Info.Profile;
				U.TSimS = T;
				U.Enu = Sim.ToEnu(St.Location);
				const bool bNew = !S.Scene.HasSource(Key);
				if (bNew)
				{
					U.VelocityMps = Sim.ToEnu(St.Location + St.Velocity) - U.Enu;
					U.bSeedHistory = !Info.bMissile;  // an aircraft was flying before; a missile starts at its launch
				}
				U.AltM = AltOf(U.Enu);
				Info.GroundAgeS += RealDtS;
				if (bNew || Info.GroundAgeS > 0.5)
				{
					Info.GroundAltM = GroundAltAt(U.Enu);
					Info.GroundAgeS = 0.0;
				}
				U.AglM = FMath::Max(U.AltM - Info.GroundAltM, 0.0);
				U.LengthM = Info.LengthM;
				S.Scene.UpdateSource(U);
				if (Info.bMissile)
				{
					// A missile sounds while its motor burns (simcore fx::MotorPhaseAt's boost and sustain).
					S.Scene.SetEmission(Key, Info.LaunchS, Info.LaunchS + Info.BoostS, Info.LaunchS + Info.BoostS + Info.SustainS);
				}
			}
		}

		/** Wrecks: a burning one is a fire on its fall; on the ground an explosion, then it burns there a while. */
		void Wrecks()
		{
			const SimFxAudio::FProfileDef* Fire = Bank().Profile(Bank().FireProfile);
			for (int32 I = S.Wrecks.Num() - 1; I >= 0; --I)
			{
				FSimFxAudioState::FWreck& W = S.Wrecks[I];
				const double T = Now - W.T0;
				const double GroundT = W.Fall.GroundTime();
				if (!W.bLanded && T >= GroundT)
				{
					W.bLanded = true;
					Blast(W.T0 + GroundT, W.Fall.Position(GroundT), Bank().KillBlastKg);
				}
				if (T > GroundT + Bank().WreckGroundFireS + 5.0)
				{
					S.Wrecks.RemoveAt(I);
					continue;
				}
				if (!W.bFire || Fire == nullptr)
				{
					continue;
				}
				SimFxAudio::FSourceUpdate U;
				U.Key = SimFxAudioWreckKey(W.Id);
				U.Profile = Fire;
				U.TSimS = Now;
				U.Enu = W.Fall.Position(FMath::Max(T, 0.0));
				U.AltM = AltOf(U.Enu);
				U.AglM = FMath::Max(U.AltM - GroundAltAt(U.Enu), 0.0);
				S.Scene.UpdateSource(U);
				S.Scene.SetEmission(U.Key, W.T0, 1.0e300, W.T0 + GroundT + Bank().WreckGroundFireS);
			}
		}

		/** The site missiles flying now: each a continuous source (its rocket motor) on its path. */
		void Flyers()
		{
			for (int32 I = S.Flyers.Num() - 1; I >= 0; --I)
			{
				const FSimFxAudioState::FFlyer& F = S.Flyers[I];
				if (Now >= F.EndS)
				{
					S.Scene.EndSource(F.Key, F.EndS);
					S.Flyers.RemoveAtSwap(I);
					continue;
				}
				if (Now < F.T0)
				{
					continue;
				}
				SimFxAudio::FSourceUpdate U;
				U.Key = F.Key;
				U.Profile = F.Profile;
				U.LengthM = F.LengthM;
				if (!S.Scene.HasSource(F.Key))
				{
					// Its track from its rails: the sound of its launch left from there.
					U.TSimS = F.T0;
					U.Enu = F.P0;
					U.AltM = AltOf(U.Enu);
					U.AglM = FMath::Max(U.AltM - GroundAltAt(U.Enu), 0.0);
					S.Scene.UpdateSource(U);
				}
				U.TSimS = Now;
				U.Enu = F.At(Now);
				U.AltM = AltOf(U.Enu);
				U.AglM = FMath::Max(U.AltM - GroundAltAt(U.Enu), 0.0);
				S.Scene.UpdateSource(U);
				S.Scene.SetEmission(F.Key, F.T0, F.T0 + F.BoostS, F.T0 + F.BoostS + F.SustainS);
			}
		}
	};
}

void USimFxAudioSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_SimFxAudioFrame);
	// Its time this frame, every return included (the photographer's cost lines).
	const uint64 FrameStart = FPlatformTime::Cycles64();
	ON_SCOPE_EXIT
	{
		LastFrameMs = static_cast<float>(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - FrameStart));
	};
	UWorld* World = GetWorld();
	if (World == nullptr || !State.IsValid())
	{
		return;
	}
	if (CVarSimFxAudio.GetValueOnGameThread() == 0)
	{
		if (State->Scene.NumActiveVoices() > 0 || State->Scene.GetStats().Sources > 0)
		{
			State->Scene.Reset();
			StopAll();
		}
		return;
	}
	if (!bDataLoaded)
	{
		LoadData();
	}
	USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>();
	simcore::World* SimWorld = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	APlayerController* Pc = World->GetFirstPlayerController();
	if (!State->Bank.bLoaded || Sim == nullptr || SimWorld == nullptr || Pc == nullptr || Pc->PlayerCameraManager == nullptr)
	{
		return;
	}
	// Paused: every voice waits where it is.
	if (Sim->IsSimPaused())
	{
		PauseAll(true);
		return;
	}
	PauseAll(false);

	// A new scenario (another runner, or one made where the last was: its feed or its clock starts again): forget
	// everything, read its feed from the start.
	simcore::ScenarioRunner* Runner = Sim->GetFxRunner();
	const double NowS = Sim->GetRenderTimeSeconds();
	const bool bFeedBack = Runner != nullptr && Runner->Fx().Last() < State->Cursor;
	const bool bClockBack = State->bStarted && NowS < State->LastNowS - 0.5;
	if (!State->bStarted || Runner != State->Runner || bFeedBack || bClockBack)
	{
		State->Scene.Reset();
		StopAll();
		State->Runner = Runner;
		State->bStarted = true;
		State->Cursor = 0;
		State->Entities.Reset();
		State->Wrecks.Reset();
		State->Launches.Reset();
		State->Flyers.Reset();
		State->OriginAltM = SimWorld->Frame().AltitudeOf(simcore::Vec3{});
		State->AirAgeS = 1.0e9;
	}
	State->LastNowS = NowS;
	State->AirAgeS += DeltaTime;
	if (State->AirAgeS > 10.0)
	{
		// The weather's air (it changes rarely): the speed of sound, the absorption and the humidity at the surface.
		TWeakObjectPtr<USimWorldSubsystem> WeakSim(Sim);
		const simcore::WeatherPreset& Preset = SimWorld->GetWeather().Preset();
		State->Scene.SetAir([WeakSim](double AltM)
			{
				const USimWorldSubsystem* S = WeakSim.Get();
				const simcore::World* W = S != nullptr ? S->GetSimWorld() : nullptr;
				return W != nullptr ? W->GetWeather().Air(AltM) : simcore::StandardAtmosphere(AltM);
			},
			simcore::fx::RelativeHumidityPct(Preset.surface_temperature_c, Preset.surface_dewpoint_c));
		State->AirAgeS = 0.0;
	}
	{
		// The cvar over sounds.json's range; back to 0, sounds.json's again.
		const float Range = CVarSimFxAudioRange.GetValueOnGameThread();
		State->Bank.MaxRangeM = Range > 0.0f ? static_cast<double>(Range) : State->DataMaxRangeM;
	}

	FSimFxAudioFrame Frame{*State, *Sim, Runner, *SimWorld, NowS};

	// The listener: the player's camera; shut in the cabin, its vehicle muffles what is outside.
	SimFxAudio::FListener Listener;
	Listener.Enu = Sim->ToEnu(Pc->PlayerCameraManager->GetCameraLocation());
	Listener.AltM = Frame.AltOf(Listener.Enu);
	Listener.AglM = FMath::Max(Listener.AltM - Frame.GroundAltAt(Listener.Enu), 0.0);
	if (const ASimOsaVehicle* Vehicle = Cast<ASimOsaVehicle>(Pc->GetPawn()))
	{
		Listener.bEnclosed = Vehicle->IsInCabin() && !Vehicle->IsOutside();
		Listener.VehicleEnu = Sim->ToEnu(Vehicle->GetActorLocation());
	}
	State->Scene.BeginFrame(Frame.Now, DeltaTime, Listener);

	Frame.ReadFeed();
	Frame.Entities(DeltaTime);
	Frame.Wrecks();
	Frame.Flyers();

	// Faster than twice real time the sounds would only be noise: they fade out and stop.
	const bool bSounding = Sim->GetTimeScale() <= 2.0f;
	const int32 MaxVoices = FMath::Clamp(CVarSimFxAudioMaxVoices.GetValueOnGameThread(), 0, 128);
	const double Volume = FMath::Clamp(CVarSimFxAudioVolume.GetValueOnGameThread(), 0.0f, 2.0f);
	State->Scene.Mix(MaxVoices, Volume, bSounding);
	Present();

	const SimFxAudio::FStats& Stats = State->Scene.GetStats();
	SET_DWORD_STAT(STAT_SimFxAudioSources, Stats.Sources);
	SET_DWORD_STAT(STAT_SimFxAudioHeard, Stats.Heard);
	SET_DWORD_STAT(STAT_SimFxAudioAudible, Stats.Audible);
	SET_DWORD_STAT(STAT_SimFxAudioVoices, Stats.Voices);
	SET_DWORD_STAT(STAT_SimFxAudioPending, Stats.Pending);
	SET_DWORD_STAT(STAT_SimFxAudioBooms, Stats.Booms);
	SET_FLOAT_STAT(STAT_SimFxAudioWindowTop, Stats.WindowTopDb);
}
