#pragma once

#include <atomic>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCrewAudioComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/**
 * What the crew hears in the cabin (roadmap 7.7). The commander's headset carries the R-123M his A-1 is on: the set's
 * hiss, shut off by the noise suppressor and quietened under a carrier, and the call tone (simcore::HeadsetSoundNow),
 * synthesised here (band-limited noise, a 1 kHz tone). The words are the crew's callouts on the intercom and the net,
 * and the designations the commander hears on the radio, spoken through the engine's text-to-speech (English, its Flite
 * voice: placeholders until recorded voice lines, roadmap 8.10). A player's own words come back as the R-124's
 * sidetone. Console: sam3d.Audio (0 off), sam3d.Voices (0 off).
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimCrewAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimCrewAudioComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The headset's loudness, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Audio")
	float Volume = 0.6f;

private:
	void Generate(USoundWaveProcedural* Wave, int32 SamplesRequired);
	void Speak(const FName& Channel, const FString& Text);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Player;

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> Wave;

	// Set on the game thread, read on the audio thread.
	std::atomic<float> HissLevel{0.0f};
	std::atomic<float> ToneLevel{0.0f};
	// Generator state (audio thread only).
	float LowState = 0.0f;
	float HighState = 0.0f;
	float HighIn = 0.0f;
	double TonePhase = 0.0;
	uint32 NoiseSeed = 22222u;

	// What has been spoken already: callouts by their start and speaker, designations by count.
	TSet<FString> Spoken;
	int32 DesignationsSpoken = 0;
	bool bChannelsReady = false;
	static constexpr int32 SampleRate = 22050;
};
