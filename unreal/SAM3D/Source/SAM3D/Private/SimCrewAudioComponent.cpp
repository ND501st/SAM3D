#include "SimCrewAudioComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundWaveProcedural.h"
#include "SimCoreIncludes.h"
#include "SimWorldSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAudio(TEXT("sam3d.Audio"), 1,
		TEXT("Cabin audio: the radio's hiss and call tone in the commander's headset (0 off)."));
}

USimCrewAudioComponent::USimCrewAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USimCrewAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateUObject(this, &USimCrewAudioComponent::Generate);
	Player = NewObject<UAudioComponent>(GetOwner());
	Player->bAutoActivate = false;
	Player->bAllowSpatialization = false;  // in the headset
	Player->SetSound(Wave);
	Player->RegisterComponent();
	Player->Play();
}

void USimCrewAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Player != nullptr)
	{
		Player->Stop();
	}
	Super::EndPlay(Reason);
}

void USimCrewAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	const USimWorldSubsystem* Sim = World ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const simcore::ScenarioRunner* Runner = Sim ? Sim->GetScenarioRunner() : nullptr;
	if (Runner == nullptr || CVarAudio.GetValueOnGameThread() == 0)
	{
		HissLevel.store(0.0f);
		ToneLevel.store(0.0f);
		return;
	}
	const simcore::HeadsetSound H = simcore::HeadsetSoundNow(*Runner);
	HissLevel.store(static_cast<float>(H.hiss) * Volume);
	ToneLevel.store(H.tone ? 0.25f * Volume : 0.0f);

}

void USimCrewAudioComponent::Generate(USoundWaveProcedural* InWave, int32 SamplesRequired)
{
	// Band-limited noise (a first-order high-pass near 300 Hz and low-pass near 3 kHz: a voice channel's band) and the
	// call tone at 1 kHz. 16-bit mono.
	const float Hiss = HissLevel.load();
	const float Tone = ToneLevel.load();
	const int32 Count = FMath::Max(SamplesRequired, 512);
	TArray<int16> Buffer;
	Buffer.SetNumUninitialized(Count);
	const float LowA = 1.0f - FMath::Exp(-2.0f * PI * 3000.0f / SampleRate);
	const float HighA = FMath::Exp(-2.0f * PI * 300.0f / SampleRate);
	for (int32 I = 0; I < Count; ++I)
	{
		NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
		const float White = static_cast<float>(NoiseSeed >> 8) / 8388608.0f - 1.0f;
		LowState += LowA * (White - LowState);
		HighState = HighA * (HighState + LowState - HighIn);
		HighIn = LowState;
		float Sample = 2.5f * Hiss * HighState;
		if (Tone > 0.0f)
		{
			Sample += Tone * static_cast<float>(FMath::Sin(TonePhase));
			TonePhase += 2.0 * PI * 1000.0 / SampleRate;
			if (TonePhase > 2.0 * PI)
			{
				TonePhase -= 2.0 * PI;
			}
		}
		Buffer[I] = static_cast<int16>(FMath::Clamp(Sample, -1.0f, 1.0f) * 32000.0f);
	}
	InWave->QueueAudio(reinterpret_cast<const uint8*>(Buffer.GetData()), Buffer.Num() * sizeof(int16));
}
