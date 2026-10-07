#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

#include <vector>

class UWorld;

/**
 * The electromagnetic layer (simcore::em) inside Unreal: radar, passive receivers, jamming, radio nets and
 * per-unit knowledge, stepped at a fixed 10 Hz from USimWorldSubsystem::Tick and drawn as debug overlays.
 *
 * For now it runs the EM demo scenario (simcore/em/demo_scenario.hpp, generic placeholder equipment) at the
 * current sim origin and terrain; linking it to the Osa scenario is the next step (docs/EM_HANDOFF.md).
 *
 * Console:
 *   sam3d.em.Start / sam3d.em.Stop   start or stop the demo
 *   sam3d.em.Debug <mask>            overlay layers (0 = none, -1 = all; bits in simcore/em/debug_draw.hpp)
 *   sam3d.em.Owner <node id>         whose knowledge to draw (0 = command posts, -1 = every node)
 */
class SAM3D_API FSimEmLayer
{
public:
	FSimEmLayer();
	~FSimEmLayer();

	/** Build the demo scenario. Terrain must outlive the layer (pass the active sim world's terrain). */
	bool StartDemo(const FString& DataDirectory, const simcore::GeoPoint& Origin, const simcore::Terrain* Terrain,
		FString& OutError);
	void Stop();
	bool IsRunning() const;

	/** Advance by frame time in fixed 0.1 s steps (capped after hitches). */
	void Advance(double DeltaSeconds);

	/** Draw the overlays selected by sam3d.em.Debug for one frame. */
	void Draw(UWorld* World, TFunctionRef<FVector(const simcore::Vec3&)> ToUnreal);

	double GetTimeSeconds() const;

	/** One line per node: name, C2 status, radar states. For HUDs and scripted checks. */
	FString GetStatusText() const;

	const simcore::em::EmScene* GetScene() const;

private:
	TUniquePtr<simcore::em::EmDemoScenario> Demo;
	double Accumulator = 0.0;
	static constexpr double StepSeconds = 0.1;

	// Overlays are rebuilt after each EM step (or a console change) and drawn every frame.
	std::vector<simcore::em::DebugPrim> Prims;
	bool bPrimsDirty = true;
	int32 PrimsMask = 0;
	int32 PrimsOwner = 0;
};
