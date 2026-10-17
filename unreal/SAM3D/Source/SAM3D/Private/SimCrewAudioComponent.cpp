#include "SimCrewAudioComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundWaveProcedural.h"
#include "TextToSpeechEngineSubsystem.h"
#include "SimCoreIncludes.h"
#include "SimWorldSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAudio(TEXT("sam3d.Audio"), 1,
		TEXT("Cabin audio: the radio's hiss and call tone in the commander's headset (0 off)."));
	TAutoConsoleVariable<int32> CVarVoices(TEXT("sam3d.Voices"), 1,
		TEXT("The crew's words and the designations spoken by text-to-speech (0 off)."));

	const FName IntercomChannel(TEXT("SAM3D_Intercom"));
	const FName RadioChannel(TEXT("SAM3D_Radio"));
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
	if (bChannelsReady && GEngine != nullptr)
	{
		if (UTextToSpeechEngineSubsystem* Tts = GEngine->GetEngineSubsystem<UTextToSpeechEngineSubsystem>())
		{
			Tts->StopSpeakingOnAllChannels();
		}
	}
	Super::EndPlay(Reason);
}

void USimCrewAudioComponent::Speak(const FName& Channel, const FString& Text)
{
	if (CVarVoices.GetValueOnGameThread() == 0 || GEngine == nullptr || Text.IsEmpty())
	{
		return;
	}
	UTextToSpeechEngineSubsystem* Tts = GEngine->GetEngineSubsystem<UTextToSpeechEngineSubsystem>();
	if (Tts == nullptr)
	{
		return;
	}
	if (!bChannelsReady)
	{
		for (const FName& C : {IntercomChannel, RadioChannel})
		{
			if (!Tts->DoesChannelExist(C))
			{
				Tts->AddDefaultChannel(C);
			}
			Tts->ActivateChannel(C);
		}
		Tts->SetRateOnChannel(RadioChannel, 0.55f);
		bChannelsReady = true;
	}
	Tts->SetVolumeOnChannel(Channel, Volume);
	Tts->SpeakOnChannel(Channel, Text);
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

	// The words: each callout once, as it starts. The net's only when the headset is on the set.
	if (const simcore::CalloutBus* Bus = Runner->Callouts())
	{
		for (const simcore::SpokenCallout& S : Bus->SpeakingNow())
		{
			const FString Key = FString::Printf(TEXT("%.3f|%s|%s"), S.start_s, UTF8_TO_TCHAR(S.from.c_str()), UTF8_TO_TCHAR(S.id.c_str()));
			if (Spoken.Contains(Key))
			{
				continue;
			}
			Spoken.Add(Key);
			const bool bNet = S.channel != "intercom";
			if (bNet && !H.on_radio)
			{
				continue;
			}
			Speak(bNet ? RadioChannel : IntercomChannel, UTF8_TO_TCHAR(S.en.c_str()));
		}
	}
	// The designations the commander hears on the radio.
	const std::vector<simcore::C2Network::Event>& Heard = Runner->DesignationsHeard();
	for (; DesignationsSpoken < static_cast<int32>(Heard.size()); ++DesignationsSpoken)
	{
		const simcore::C2Network::Event& E = Heard[static_cast<size_t>(DesignationsSpoken)];
		if (!H.on_radio)
		{
			continue;
		}
		Speak(RadioChannel, FString::Printf(TEXT("Target %04d, azimuth %02d %02d, range %.0f, height %d."), E.number,
			E.azimuth_mils / 100, E.azimuth_mils % 100, E.range_m / 1000.0, E.height_hm));
	}
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
