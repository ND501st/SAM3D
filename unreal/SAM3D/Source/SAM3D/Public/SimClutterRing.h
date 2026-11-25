#pragma once

#include <memory>

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimClutterRing.generated.h"

class UInstancedStaticMeshComponent;

namespace simcore
{
	class MapData;
	class MapFrameLink;
	namespace scenery
	{
		struct ClutterRules;
		struct CoverRaster;
		struct DressingRules;
		struct GroundLayerRules;
	}
}

/** A ring's plants as instances, built off the game thread: each part's transforms and custom data. */
struct FSimClutterBuild
{
	TArray<TArray<FTransform>> Xf;
	TArray<TArray<FVector3f>> Data;
	double East = 0.0, North = 0.0;
	double Ms = 0.0;
	int32 Count = 0;
	int32 Generation = 0;
};

/**
 * The ground clutter round the camera (roadmap T6.1, docs/scenery/T5_T8_PLAN.md): tufts of grass, stubble, maize stumps,
 * dry stalks and reeds within about 100 m, scattered by simcore (scenery/clutter.hpp) from the layer under each plant. One
 * instanced mesh per part, no shadows; rebuilt when the camera has moved a rebuild step (16 m), and emptied while the
 * camera is high over the ground. The plants are scattered on a worker thread (A.3.40: 30 000 clumps take about 50 ms)
 * and swapped in when ready, the old ring drawn meanwhile. Created by ASimMapActor with the map's data; presentation only.
 */
UCLASS()
class SAM3D_API USimClutterRing : public USceneComponent
{
	GENERATED_BODY()

public:
	USimClutterRing();

	/** Gives the ring the map's data and makes its meshes. False (nothing drawn) without the rules or the meshes. */
	bool Init(const std::shared_ptr<const simcore::MapData>& InMap, const std::shared_ptr<const simcore::MapFrameLink>& InLink,
		const std::shared_ptr<const simcore::scenery::CoverRaster>& InCover, const std::shared_ptr<const simcore::scenery::DressingRules>& InDressing,
		const std::shared_ptr<const simcore::scenery::GroundLayerRules>& InLayers, const std::shared_ptr<const simcore::scenery::ClutterRules>& InRules,
		int32 InDay);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	/** Starts a ring's build round the point on a worker thread. */
	void Rebuild(double MapEast, double MapNorth);
	/** Swaps a finished build's instances in. */
	void Apply(const FSimClutterBuild& Build);
	void EmptyRing();

	std::shared_ptr<const simcore::MapData> Map;
	std::shared_ptr<const simcore::MapFrameLink> Link;
	std::shared_ptr<const simcore::scenery::CoverRaster> Cover;
	std::shared_ptr<const simcore::scenery::DressingRules> Dressing;
	std::shared_ptr<const simcore::scenery::GroundLayerRules> Layers;
	std::shared_ptr<const simcore::scenery::ClutterRules> Rules;
	int32 Day = 288;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Parts;  // one per distinct part key, in the rules' order

	TArray<FTransform> Fixes;  // each part's asset fix (identity for the procedural meshes)
	bool bTinted = false;      // the clutter material takes each instance's brightness in its custom data
	bool bBuilt = false;
	double LastEast = 0.0, LastNorth = 0.0;
	int32 LastCount = 0;
	TFuture<TSharedPtr<FSimClutterBuild>> Pending;  // the build under way, if any
	int32 Generation = 0;                          // a build started before the ring was last emptied is dropped
};
