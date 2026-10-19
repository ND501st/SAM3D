#include "SimCrewVoiceComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimSpeech.h"
#include "SimWorldSubsystem.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	TAutoConsoleVariable<int32> CVarVoice(TEXT("sam3d.Voice"), 1, TEXT("Voice the crew's callouts: 1 on, 0 off."), ECVF_Default);
	TAutoConsoleVariable<FString> CVarVoiceLanguage(TEXT("sam3d.VoiceLanguage"), TEXT("auto"),
		TEXT("The crew's language: auto (the first of data/crew/voices.json's languages with a voice installed), ru or en."), ECVF_Default);
	TAutoConsoleVariable<float> CVarVoiceVolume(TEXT("sam3d.VoiceVolume"), 1.0f, TEXT("The crew's voices' volume, 0 to 2."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarInstructorVoice(TEXT("sam3d.InstructorVoice"), 1,
		TEXT("A lesson's instructor reads each new step aloud (an English voice): 1 on, 0 off."), ECVF_Default);

	FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	// 16-bit PCM from a WAV file (mono, or stereo mixed down); false if it isn't plain PCM.
	bool ReadWav(const FString& Path, TArray<int16>& Out, int32& OutRate)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes.Num() < 44 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0)
		{
			return false;
		}
		int32 Channels = 0, Bits = 0, At = 12;
		OutRate = 0;
		while (At + 8 <= Bytes.Num())
		{
			const uint8* C = Bytes.GetData() + At;
			const int32 Size = static_cast<int32>(C[4] | (C[5] << 8) | (C[6] << 16) | (C[7] << 24));
			if (FMemory::Memcmp(C, "fmt ", 4) == 0 && Size >= 16)
			{
				const int32 Format = C[8] | (C[9] << 8);
				Channels = C[10] | (C[11] << 8);
				OutRate = static_cast<int32>(C[12] | (C[13] << 8) | (C[14] << 16) | (C[15] << 24));
				Bits = C[22] | (C[23] << 8);
				if (Format != 1 || Bits != 16 || Channels < 1 || Channels > 2)
				{
					return false;
				}
			}
			else if (FMemory::Memcmp(C, "data", 4) == 0 && Channels > 0)
			{
				const int32 Frames = FMath::Min(Size, Bytes.Num() - At - 8) / (2 * Channels);
				const int16* S = reinterpret_cast<const int16*>(C + 8);
				Out.SetNumUninitialized(Frames);
				for (int32 I = 0; I < Frames; ++I)
				{
					Out[I] = Channels == 1 ? S[I] : static_cast<int16>((static_cast<int32>(S[2 * I]) + S[2 * I + 1]) / 2);
				}
				return Frames > 0;
			}
			At += 8 + Size + (Size & 1);
		}
		return false;
	}

	// Over the radio: a little hiss under the voice and the squelch's tail after it.
	void AddRadioNoise(TArray<int16>& Pcm, int32 SampleRate)
	{
		FRandomStream Rng(Pcm.Num());
		const int32 Tail = SampleRate / 6;
		const int32 Start = Pcm.Num();
		Pcm.AddZeroed(Tail);
		for (int32 I = 0; I < Pcm.Num(); ++I)
		{
			const float Hiss = (Rng.FRand() * 2.0f - 1.0f) * (I < Start ? 500.0f : 2200.0f);
			Pcm[I] = static_cast<int16>(FMath::Clamp(static_cast<float>(Pcm[I]) + Hiss, -32767.0f, 32767.0f));
		}
	}
}

USimCrewVoiceComponent::USimCrewVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USimCrewVoiceComponent::ChooseLanguage(const simcore::VoiceData& Voices)
{
	FSimSpeech& Speech = FSimSpeech::Get();
	if (!Speech.IsReady())
	{
		return;
	}
	if (!bListedVoices)
	{
		bListedVoices = true;
		for (const FString& V : Speech.DescribeVoices())
		{
			UE_LOG(LogSAM3D, Log, TEXT("Crew voice available: %s"), *V);
		}
	}
	const FString Want = CVarVoiceLanguage.GetValueOnGameThread();
	FString Chosen;
	if (Want == TEXT("ru") || Want == TEXT("en"))
	{
		Chosen = Want;
	}
	else
	{
		for (const std::string& L : Voices.languages)
		{
			if (Speech.NumVoices(ToF(L)) > 0)
			{
				Chosen = ToF(L);
				break;
			}
		}
	}
	if (Chosen != Language)
	{
		Language = Chosen;
		UE_LOG(LogSAM3D, Log, TEXT("Crew voices: %s"), Language.IsEmpty() ? TEXT("none installed (the crew is silent)") : *Language);
		if (Language == TEXT("en") && Voices.languages.size() > 0 && Voices.languages[0] == "ru")
		{
			UE_LOG(LogSAM3D, Log, TEXT("Crew voices: no Russian voice installed; Windows: Settings > Time & language > Speech > Add voices > Russian"));
		}
	}
}

UAudioComponent* USimCrewVoiceComponent::ChannelComponent(const FString& Channel)
{
	if (TObjectPtr<UAudioComponent>* Found = Channels.Find(Channel))
	{
		return Found->Get();
	}
	UAudioComponent* Audio = NewObject<UAudioComponent>(GetOwner());
	Audio->bAutoActivate = false;
	Audio->bAllowSpatialization = false;  // the headset
	Audio->bIsUISound = false;
	Audio->RegisterComponent();
	Channels.Add(Channel, Audio);
	return Audio;
}

void USimCrewVoiceComponent::Play(TArray<int16>&& Pcm, int32 SampleRate, const FString& Channel, const simcore::VoiceSettings& Settings)
{
	if (Pcm.Num() == 0)
	{
		return;
	}
	if (Settings.radio)
	{
		AddRadioNoise(Pcm, SampleRate);
	}
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = static_cast<float>(Pcm.Num()) / static_cast<float>(SampleRate);
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * static_cast<int32>(sizeof(int16)));
	// One voice per channel, as on the bus: a new line keys over what is still playing.
	UAudioComponent* Audio = ChannelComponent(Channel);
	Audio->Stop();
	Audio->SetSound(Wave);
	Audio->SetVolumeMultiplier(FMath::Clamp(CVarVoiceVolume.GetValueOnGameThread(), 0.0f, 2.0f));
	Audio->SetLowPassFilterEnabled(true);
	Audio->SetLowPassFilterFrequency(static_cast<float>(Settings.high_hz));
	Audio->SetHighPassFilterEnabled(true);
	Audio->SetHighPassFilterFrequency(static_cast<float>(Settings.low_hz));
	Audio->Play();
	++LinesVoiced;
}

bool USimCrewVoiceComponent::PlayRecorded(const simcore::SpokenCallout& Line, const simcore::VoiceSettings& Settings, const FString& Channel)
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
	// Only fixed lines: a line with filled-in words (a contact's azimuth) is never the catalogue's text.
	const simcore::CalloutDef* Def = Runner->Def().has_crew_data ? Runner->Def().crew_data.FindCallout(Line.id) : nullptr;
	if (Def == nullptr || Def->ru != Line.ru)
	{
		return false;
	}
	const FString Root = FPaths::Combine(Sim->GetDataDirectory(), TEXT(".."), TEXT("art"), TEXT("crew_voices"), Language);
	for (const FString& Path : {FPaths::Combine(Root, ToF(Line.from), ToF(Line.id) + TEXT(".wav")), FPaths::Combine(Root, ToF(Line.id) + TEXT(".wav"))})
	{
		TArray<int16> Pcm;
		int32 Rate = 0;
		if (ReadWav(Path, Pcm, Rate))
		{
			Play(MoveTemp(Pcm), Rate, Channel, Settings);
			return true;
		}
	}
	return false;
}

void USimCrewVoiceComponent::SpeakInstructor(USimWorldSubsystem& Sim, FSimSpeech& Speech)
{
	// Taken only once the engine can speak it: the first step's words wait for the voices to be listed.
	FString Line;
	if (!Speech.IsReady() || !Sim.TakeTutorialSpeech(Line) || CVarInstructorVoice.GetValueOnGameThread() == 0 ||
		Speech.NumVoices(TEXT("en")) == 0)
	{
		return;
	}
	FSimSpeechRequest R;
	R.Id = NextRequest++;
	R.Text = Line;
	R.Language = TEXT("en");
	R.Rate = -1;  // a touch slower than the crew: he is explaining
	InstructorRequest = R.Id;
	Speech.Request(R);
	UE_LOG(LogSAM3D, Log, TEXT("Instructor: %s"), *Line);
}

void USimCrewVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr)
	{
		return;
	}
	const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
	const simcore::VoiceData* Voices = Sim->GetVoiceData();
	FSimSpeech& Speech = FSimSpeech::Get();
	// The lesson's instructor talks whatever the crew's voices are doing.
	SpeakInstructor(*Sim, Speech);
	const bool bCrew = Runner != nullptr && Voices != nullptr && Runner->Callouts() != nullptr && CVarVoice.GetValueOnGameThread() != 0;
	if (bCrew && Runner != LastRunner)  // a new scenario: start from its first line
	{
		LastRunner = Runner;
		LastSeq = 0;
		Pending.Reset();
	}
	if (bCrew)
	{
		ChooseLanguage(*Voices);
	}
	// Lines synthesised since the last frame.
	FSimSpeechResult Done;
	while (Speech.TakeResult(Done))
	{
		if (Done.Id == InstructorRequest)
		{
			InstructorRequest = 0;
			if (Done.bOk)
			{
				// In the room, not on the intercom: the whole band, no hiss.
				simcore::VoiceSettings Room;
				Room.low_hz = 80.0;
				Room.high_hz = 12000.0;
				const float Seconds = static_cast<float>(Done.Pcm.Num()) / static_cast<float>(FMath::Max(1, Done.SampleRate));
				Play(MoveTemp(Done.Pcm), Done.SampleRate, TEXT("instructor"), Room);
				UE_LOG(LogSAM3D, Log, TEXT("Instructor: spoken (%.1f s)"), Seconds);
			}
			continue;
		}
		FPendingLine Line;
		if (!bCrew || !Pending.RemoveAndCopyValue(Done.Id, Line) || !Done.bOk)
		{
			continue;
		}
		if (Runner->Time() - Line.SpokenAt > 4.0)
		{
			continue;  // too late to matter (the engine was slow, or time ran fast)
		}
		Play(MoveTemp(Done.Pcm), Done.SampleRate, Line.Channel, Line.Settings);
	}
	if (!bCrew)
	{
		return;
	}
	// New lines on the bus.
	const std::vector<simcore::SeatActivity> Activity = Runner->CrewActivity();
	for (const simcore::SpokenCallout& Line : Runner->Callouts()->Now())
	{
		if (Line.seq <= LastSeq)
		{
			continue;
		}
		LastSeq = Line.seq;
		if (Language.IsEmpty() || Sim->GetTimeScale() > 1.5f || Sim->IsSimPaused())
		{
			continue;
		}
		bool bStressed = false;
		for (const simcore::SeatActivity& A : Activity)
		{
			bStressed = bStressed || (A.station == Line.from && A.stressed);
		}
		const simcore::VoiceSettings Settings = simcore::VoiceFor(*Voices, Line.from, bStressed, Line.channel);
		const FString Channel = ToF(Line.channel);
		if (PlayRecorded(Line, Settings, Channel))
		{
			continue;
		}
		int32 SeatIndex = 0;
		for (size_t I = 0; I < Voices->seats.size(); ++I)
		{
			if (Voices->seats[I].station == Line.from) SeatIndex = static_cast<int32>(I);
		}
		FSimSpeechRequest R;
		R.Id = NextRequest++;
		R.Text = ToF(simcore::SpeechText(Line, ToStd(Language), *Voices));
		R.Language = Language;
		R.Pitch = Settings.pitch;
		R.Rate = Settings.rate;
		R.VoiceIndex = SeatIndex;
		Pending.Add(R.Id, FPendingLine{Channel, Settings, Line.start_s});
		Speech.Request(R);
	}
}
