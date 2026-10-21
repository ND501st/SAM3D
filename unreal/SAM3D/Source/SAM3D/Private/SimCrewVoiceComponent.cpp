#include "SimCrewVoiceComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimOsaVehicle.h"
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

	// A second-order filter (RBJ biquad), high-pass or low-pass, run over the samples in place.
	void Biquad(TArray<float>& X, float SampleRate, float Hz, bool bHighPass)
	{
		const float W = 2.0f * PI * FMath::Clamp(Hz, 20.0f, SampleRate * 0.45f) / SampleRate;
		const float Cw = FMath::Cos(W), Alpha = FMath::Sin(W) / (2.0f * 0.7071f);
		const float B0 = bHighPass ? (1.0f + Cw) * 0.5f : (1.0f - Cw) * 0.5f;
		const float B1 = bHighPass ? -(1.0f + Cw) : 1.0f - Cw;
		const float A0 = 1.0f + Alpha, A1 = -2.0f * Cw, A2 = 1.0f - Alpha;
		float X1 = 0.0f, X2 = 0.0f, Y1 = 0.0f, Y2 = 0.0f;
		for (float& S : X)
		{
			const float Y = (B0 * S + B1 * X1 + B0 * X2 - A1 * Y1 - A2 * Y2) / A0;
			X2 = X1;
			X1 = S;
			Y2 = Y1;
			Y1 = Y;
			S = Y;
		}
	}

	// Over the radio (an R-123M's FM voice channel): the squelch opening with a burst of noise, the voice squeezed into
	// the radio's band (two filters each side, steeper than the audio component's), driven hard into the set's limiter
	// so it is loud and a little rough, hiss under it, and the squelch's tail after it.
	void MakeRadioVoice(TArray<int16>& Pcm, int32 SampleRate, double LowHz, double HighHz)
	{
		FRandomStream Rng(Pcm.Num());
		const int32 Open = SampleRate / 14;  // ~70 ms
		const int32 Tail = SampleRate / 5;   // ~200 ms
		TArray<float> X;
		X.SetNumZeroed(Open + Pcm.Num() + Tail);
		float Peak = 1.0f;
		for (int32 I = 0; I < Pcm.Num(); ++I)
		{
			Peak = FMath::Max(Peak, FMath::Abs(static_cast<float>(Pcm[I])));
		}
		for (int32 I = 0; I < Pcm.Num(); ++I)
		{
			X[Open + I] = static_cast<float>(Pcm[I]) / Peak;
		}
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Biquad(X, static_cast<float>(SampleRate), static_cast<float>(LowHz), true);
			Biquad(X, static_cast<float>(SampleRate), static_cast<float>(HighHz), false);
		}
		const float Drive = 3.5f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const bool bVoice = I >= Open && I < Open + Pcm.Num();
			const float Noise = (Rng.FRand() * 2.0f - 1.0f) * (bVoice ? 0.03f : (I < Open ? 0.35f * (1.0f - static_cast<float>(I) / Open) + 0.05f : 0.3f));
			const float V = FMath::Tanh(Drive * X[I]) / FMath::Tanh(Drive);
			X[I] = FMath::Clamp(0.8f * V + Noise, -1.0f, 1.0f);
		}
		// The tail's noise shaped like the radio's too, then out as 16-bit.
		Biquad(X, static_cast<float>(SampleRate), static_cast<float>(LowHz), true);
		Biquad(X, static_cast<float>(SampleRate), static_cast<float>(HighHz), false);
		Pcm.SetNumUninitialized(X.Num());
		for (int32 I = 0; I < X.Num(); ++I)
		{
			Pcm[I] = static_cast<int16>(FMath::Clamp(X[I] * 30000.0f, -32767.0f, 32767.0f));
		}
	}

	// A radio log line as it is read out: the four-digit numbers in pairs ("62 01"), the way a report is spoken.
	FString RadioSpeech(const FString& Text)
	{
		FString Out;
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			const bool bFour = I + 3 < Text.Len() && FChar::IsDigit(Text[I]) && FChar::IsDigit(Text[I + 1]) && FChar::IsDigit(Text[I + 2]) &&
							   FChar::IsDigit(Text[I + 3]) && (I == 0 || !FChar::IsDigit(Text[I - 1])) &&
							   (I + 4 >= Text.Len() || !FChar::IsDigit(Text[I + 4]));
			if (bFour)
			{
				Out += Text.Mid(I, 2) + TEXT(" ") + Text.Mid(I + 2, 2);
				I += 3;
				continue;
			}
			Out.AppendChar(Text[I]);
		}
		return Out;
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
		MakeRadioVoice(Pcm, SampleRate, Settings.low_hz, Settings.high_hz);
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
	// New lines on the bus: only those the player's seat hears (his headset and box, or the cabin).
	const ASimOsaVehicle* Vehicle = Cast<ASimOsaVehicle>(GetOwner());
	const std::string Seat = Vehicle != nullptr ? std::string(TCHAR_TO_UTF8(*Vehicle->GetStationId())) : std::string();
	const std::vector<simcore::SeatActivity> Activity = Runner->CrewActivity();
	for (const simcore::SpokenCallout& Line : Runner->Callouts()->Now())
	{
		if (Line.seq <= LastSeq)
		{
			continue;
		}
		LastSeq = Line.seq;
		if (!Seat.empty() && !Runner->CalloutHeardAt(Line, Seat))
		{
			continue;  // not on his headset, not in his cabin
		}
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
	VoiceRadioLog(*Sim, *Runner, *Voices, Speech);
}

void USimCrewVoiceComponent::VoiceRadioLog(USimWorldSubsystem& Sim, const simcore::ScenarioRunner& Runner, const simcore::VoiceData& Voices,
	FSimSpeech& Speech)
{
	// The radio traffic that is not on the callout bus: the command net's designations, the air-picture net's reports, the
	// battery's own messages. Each is read through the radio (one voice per net), in an English voice: the log is English.
	const bool bVoice = Speech.IsReady() && Speech.NumVoices(TEXT("en")) > 0 && Sim.GetTimeScale() <= 1.5f && !Sim.IsSimPaused();
	const ASimOsaVehicle* Vehicle = Cast<ASimOsaVehicle>(GetOwner());
	const FString Seat = Vehicle != nullptr ? Vehicle->GetStationId() : FString();
	for (const FSimRadioMessage& M : Sim.GetRadioMessages(16))
	{
		const FString Key = M.Net + TEXT("|") + M.From + TEXT("|") + M.Text;
		if (M.TimeSeconds < LastRadioS || (M.TimeSeconds == LastRadioS && RadioAtLast.Contains(Key)))
		{
			continue;
		}
		if (M.TimeSeconds > LastRadioS)
		{
			LastRadioS = M.TimeSeconds;
			RadioAtLast.Reset();
		}
		RadioAtLast.Add(Key);
		if (!bVoice || M.bCallout || M.Net.StartsWith(TEXT("cabin")) || M.Net.EndsWith(TEXT(" signals")) || Runner.Time() - M.TimeSeconds > 4.0 ||
			(!Seat.IsEmpty() && !M.HeardAt(Seat)) || M.Text.StartsWith(TEXT("(")))
		{
			continue;  // a callout (voiced from the bus), shouted in the cabin, a flag or lamp signal (seen), not heard at his
					   // seat, noise, or too late
		}
		simcore::VoiceSettings Settings;
		const bool bLine = M.Net.StartsWith(TEXT("line "));
		Settings.radio = !bLine;  // a telephone line: the narrow band, no squelch
		Settings.low_hz = bLine ? 300.0 : Voices.radio_low_hz;
		Settings.high_hz = bLine ? 3400.0 : Voices.radio_high_hz;
		FSimSpeechRequest R;
		R.Id = NextRequest++;
		R.Text = RadioSpeech(M.Text);
		R.Language = TEXT("en");
		R.Rate = 1;  // a report read out briskly
		R.VoiceIndex = M.Net == TEXT("air picture") ? 1 : 0;  // the regiment's radar battery: another man
		Pending.Add(R.Id, FPendingLine{TEXT("radio:") + M.Net, Settings, M.TimeSeconds});
		Speech.Request(R);
	}
}
