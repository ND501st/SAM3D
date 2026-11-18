#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "SimFxAudio.generated.h"

class AActor;
class UAudioComponent;
class USoundWave;
struct FSimFxAudioState;

/**
 * The weapons' and aircraft's sound (docs/fx/README.md section 4.2): every aircraft's engines by its kind, our missiles
 * while their motors burn, falling and burning wrecks, and the presentation feed's one-shots (launches, bursts, bombs,
 * rockets, guns, booms), each heard where it was when the sound now arriving left it, at the speed of sound in the
 * weather's air, shifted by Doppler, quieter and duller with distance, muffled in the closed cabin.
 *
 * Presentation only: it reads the simulation (USimWorldSubsystem's entity snapshot and the runner's fx::Feed) and never
 * changes it. The sound logic is plain C++ (SimFxAudioScene.h); this subsystem gives it the sources and the listener (the
 * player's camera) each frame and plays its voices: a fixed pool of audio components made once on a transient actor,
 * placed, pitched, filtered and set in volume here (no engine attenuation, Doppler or occlusion). Data:
 * data/fx/sounds.json; samples: /Game/SAM3D/FX/Sounds (tools/ue_make_fx_assets.py from tools/fx_sounds.py's files).
 *
 * Console: sam3d.fx.Audio (0 off), sam3d.fx.AudioVolume, sam3d.fx.MaxVoices (32), sam3d.fx.AudioRange (m).
 * "stat SAM3DFxAudio" shows its time and counts.
 */
UCLASS()
class SAM3D_API USimFxAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	USimFxAudioSubsystem();

	// USubsystem / FTickableGameObject
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Voices playing now (for the stats and the tests). */
	int32 GetActiveVoiceCount() const;

	/** The game thread's time in the last frame's Tick, ms (the photographer logs it beside the effects'). */
	float GetLastFrameMs() const { return LastFrameMs; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadData();
	void EnsureVoices(int32 Count);
	void Present();
	void StopAll();
	void PauseAll(bool bPause);
	USoundWave* WaveFor(FName Sample) const;

	/** The actor the voices live on (transient, made once). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> VoiceHost;

	/** The fixed pool of voices. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> VoicePool;

	/** The samples, loaded once (a missing one is absent). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundWave>> Waves;

	/** Which sound each voice component last started (its voice's generation). */
	TArray<uint32> AppliedGeneration;

	bool bVoicesPaused = false;
	bool bDataLoaded = false;
	float LastFrameMs = 0.0f;

	TPimplPtr<FSimFxAudioState> State;
};
