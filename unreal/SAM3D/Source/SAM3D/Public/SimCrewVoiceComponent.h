#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCoreIncludes.h"
#include "SimCrewVoiceComponent.generated.h"

class FSimSpeech;
class UAudioComponent;
class USoundWaveProcedural;
class USimWorldSubsystem;

/**
 * The crew's voices (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): every line the crew speaks on the intercom or the
 * battery net (simcore's callout bus) is heard, in its seat's voice (data/crew/voices.json): a recorded line when there is
 * one (art/crew_voices/<language>/<seat>/<callout>.wav, then art/crew_voices/<language>/<callout>.wav), otherwise the
 * speech engine of the operating system (FSimSpeech). Intercom lines go through the intercom's band; radio lines sound
 * like a radio: the squelch opening, the voice squeezed into the radio's band and driven into its limiter, hiss, the
 * squelch's tail. The radio log's own lines (the command net's designations, the air-picture net's reports, the battery's
 * traffic) are read out the same way, in an English voice.
 *
 * The simulation keeps its own timing (the bus's speaking time); a voice plays alongside it. Under time compression
 * (above 1.5x) the crew is not voiced. Console: sam3d.Voice 0/1, sam3d.VoiceLanguage auto|ru|en, sam3d.VoiceVolume.
 *
 * A lesson's instructor (data/tutorials/, docs/TUTORIALS.md) reads each new step's title, and an action step's first line,
 * in an English voice, outside the intercom's band and whatever the crew's voices are doing (sam3d.InstructorVoice 0/1).
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimCrewVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimCrewVoiceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The language the crew speaks ("ru", "en"; "" until the speech engine has listed its voices or with none). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	FString GetLanguage() const { return Language; }

	/** Lines voiced so far (recorded or synthesised). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	int32 GetLinesVoiced() const { return LinesVoiced; }

private:
	struct FPendingLine
	{
		FString Channel;
		simcore::VoiceSettings Settings;
		double SpokenAt = 0.0;  // sim time the bus started it
	};

	void ChooseLanguage(const simcore::VoiceData& Voices);
	/** The lesson's new step, aloud (once the speech engine has its voices). */
	void SpeakInstructor(USimWorldSubsystem& Sim, FSimSpeech& Speech);
	bool PlayRecorded(const simcore::SpokenCallout& Line, const simcore::VoiceSettings& Settings, const FString& Channel);
	void Play(TArray<int16>&& Pcm, int32 SampleRate, const FString& Channel, const simcore::VoiceSettings& Settings);
	UAudioComponent* ChannelComponent(const FString& Channel);
	/** The radio log's lines that are not callouts (designations, air-picture reports, the battery's traffic), aloud. */
	void VoiceRadioLog(USimWorldSubsystem& Sim, const simcore::ScenarioRunner& Runner, const simcore::VoiceData& Voices, FSimSpeech& Speech);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAudioComponent>> Channels;

	TMap<int32, FPendingLine> Pending;
	const simcore::ScenarioRunner* LastRunner = nullptr;
	size_t LastSeq = 0;
	int32 NextRequest = 1;
	int32 InstructorRequest = 0;  // the instructor's line being synthesised (0 none)
	int32 LinesVoiced = 0;
	double LastRadioS = -1.0;    // the radio log's newest line already taken
	TSet<FString> RadioAtLast;   // the lines at that time already taken
	FString Language;
	bool bListedVoices = false;
};
