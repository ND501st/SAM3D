#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCoreIncludes.h"
#include "SimCrewVoiceComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/**
 * The crew's voices (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): every line the crew speaks on the intercom or the
 * battery net (simcore's callout bus) is heard, in its seat's voice (data/crew/voices.json): a recorded line when there is
 * one (art/crew_voices/<language>/<seat>/<callout>.wav, then art/crew_voices/<language>/<callout>.wav), otherwise the
 * speech engine of the operating system (FSimSpeech). Intercom lines go through the intercom's band, radio lines through
 * the radio's, with a little noise and the squelch's tail.
 *
 * The simulation keeps its own timing (the bus's speaking time); a voice plays alongside it. Under time compression
 * (above 1.5x) the crew is not voiced. Console: sam3d.Voice 0/1, sam3d.VoiceLanguage auto|ru|en, sam3d.VoiceVolume.
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
	bool PlayRecorded(const simcore::SpokenCallout& Line, const simcore::VoiceSettings& Settings, const FString& Channel);
	void Play(TArray<int16>&& Pcm, int32 SampleRate, const FString& Channel, const simcore::VoiceSettings& Settings);
	UAudioComponent* ChannelComponent(const FString& Channel);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAudioComponent>> Channels;

	TMap<int32, FPendingLine> Pending;
	const simcore::ScenarioRunner* LastRunner = nullptr;
	size_t LastSeq = 0;
	int32 NextRequest = 1;
	int32 LinesVoiced = 0;
	FString Language;
	bool bListedVoices = false;
};
