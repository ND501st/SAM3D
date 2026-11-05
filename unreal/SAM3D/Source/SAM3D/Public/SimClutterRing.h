#pragma once

#include <memory>

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

/**
 * The ground clutter round the camera (roadmap T6.1, docs/scenery/T5_T8_PLAN.md): tufts of grass, stubble, maize stumps,
 * dry stalks and reeds within about 100 m, scattered by simcore (scenery/clutter.hpp) from the layer under each plant. One
 * instanced mesh per part, no shadows; rebuilt when the camera has moved a rebuild step (16 m), and emptied while the
 * camera is high over the ground. Created by ASimMapActor with the map's data; presentation only.
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
	void Rebuild(double MapEast, double MapNorth);
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
};
