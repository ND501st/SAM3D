#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimModelGallery.generated.h"

class ACameraActor;
class USceneComponent;

/**
 * Scripted checks of the basic models (docs/scenery/README.md):
 *  - -SimModelGallery=<folder>: every model of data/scenery/basic_models.json laid out in rows on a flat ground of its
 *    own, high above the level's terrain, each labelled, photographed row by row (gallery_<n>.png), then the game quits.
 *  - -SimShots=<folder> [-SimShotTimes=20,60,120]: while a scenario plays, at each time (seconds after it started) a
 *    photograph from a chase camera behind the nearest aircraft (shot_<t>_air.png) and one over the battery
 *    (shot_<t>_battery.png); the log lists every entity and the model it is drawn with; after the last, the game quits.
 * ASimScenarioDirector starts them from BeginPlay (StartIfAsked).
 */
UCLASS()
class SAM3D_API ASimModelGallery : public AActor
{
	GENERATED_BODY()

public:
	ASimModelGallery();

	/** Spawn the gallery or the photographer if the command line asks for one. */
	static void StartIfAsked(UWorld* World);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Where the photographs go (empty: this actor does nothing). */
	FString Folder;
	/** True: photograph a running scenario (-SimShots); false: lay out the gallery (-SimModelGallery). */
	bool bScenarioShots = false;
	TArray<float> ShotTimes;
	/** -SimTimeScale=N: the scenario runs N times faster (the world's time dilation), so late shots come sooner. */
	float TimeScale = 1.0f;

private:
	void LayOut();
	void Aim(const FVector& From, const FVector& At);
	bool AimAtScenario(bool bAir);
	/** Where the battery view's camera sits from the vehicle at At (cm): on a map, a side with a clear view past the woods. */
	FVector BatteryViewOffset(const FVector& At) const;
	void LogEntities() const;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;

	/** The gallery's shots: (camera position, what it looks at), in world centimetres. */
	TArray<TPair<FVector, FVector>> Views;
	int32 Step = 0;
	float Clock = 0.0f;
	float NextAt = 0.0f;
	int32 Phase = 0;
	float ScenarioClockStart = -1.0f;

	/** The aircraft the chase camera follows, and where the camera sits from it. */
	TWeakObjectPtr<AActor> Followed;
	FVector FollowOffset = FVector::ZeroVector;
};
