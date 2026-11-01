#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ACameraActor;
class UWorld;
class USimFxSubsystem;

/**
 * The effects' photographer (-SimFxShots=<folder> [-SimFxShotsNight], docs/fx/README.md 4.1): once a scenario runs, it
 * puts synthetic events in the runner's fx feed (presentation only: the feed is never read back by the simulation) at
 * known places round the player's unit, and photographs each from a camera framing it, at fixed delays on the
 * simulation's clock (paused for each picture): shot_<what>_<delay>.png in the folder. Then it quits the game.
 *
 * Plain C++, owned by USimFxSubsystem (started from its OnWorldBeginPlay) and ticked by it once a frame.
 */
class SAM3D_API FSimFxShots
{
public:
	FSimFxShots(UWorld* World, const FString& Folder, bool bNight);
	~FSimFxShots();

	FSimFxShots(const FSimFxShots&) = delete;
	FSimFxShots& operator=(const FSimFxShots&) = delete;

	/** One frame: RealDt is the frame's real (wall) time; the shots' delays run on the simulation's clock. */
	void Tick(USimFxSubsystem& Fx, float RealDt);

	/** True once every shot is taken (or skipped) and the game was asked to quit. */
	bool IsDone() const;

	/** The shot list's state (SimFxShots.cpp). */
	struct FState;

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ACameraActor> Camera;
	FString Folder;
	bool bNight = false;
	TUniquePtr<FState> State;
};
