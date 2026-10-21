#pragma once

#include <atomic>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCrewAudioComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/**
 * The commander's headset between the words (roadmap 7.7): the R-123M his A-1 is on hisses, the noise suppressor shuts
 * the hiss off, a carrier on the net quietens it, and ТОН. ВЫЗОВ sounds its call tone (simcore::HeadsetSoundNow),
 * synthesised here (band-limited noise, a 1 kHz tone). The words themselves are USimCrewVoiceComponent's (roadmap 8.10).
 * Console: sam3d.Audio (0 off).
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

	static constexpr int32 SampleRate = 22050;
};
