#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ACameraActor;
class UWorld;
class USimFxSubsystem;

/**
 * The air battle's photographer (-SimAirShots=<folder>, roadmap B13; unreal/README.md): while a scenario's air battle
 * runs (scenarios/air_battle_shots.json when none is named), it watches the simulation's own fx feed and entities and
 * photographs, close to the aircraft, the first of each: a banked aircraft, a flight in formation, one of our fighters, a
 * flare salvo, a chaff salvo, a jettison, an air-to-air missile off its rail and at its end, a fighter's gun burst. It
 * puts nothing into the simulation or the feed: what it photographs is what simcore did. The simulation is paused for
 * each picture (shot_air_<what>_<delay>s.png); the log names each aircraft photographed, its model, side and bank. When
 * every subject is taken, or after -SimAirShotsMaxS=<s> of the scenario (default 900), it quits the game.
 *
 * Plain C++, owned by USimFxSubsystem and ticked by it once a frame.
 */
class SAM3D_API FSimAirShots
{
public:
	FSimAirShots(UWorld* World, const FString& Folder);
	~FSimAirShots();

	FSimAirShots(const FSimAirShots&) = delete;
	FSimAirShots& operator=(const FSimAirShots&) = delete;

	/** One frame: RealDt is the frame's real (wall) time; the delays run on the simulation's render clock. */
	void Tick(USimFxSubsystem& Fx, float RealDt);

	bool IsDone() const;

	struct FState;

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ACameraActor> Camera;
	FString Folder;
	TUniquePtr<FState> State;
};
