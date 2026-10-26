#pragma once

#include <memory>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimMapActor.generated.h"

class ACameraActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USimWorldSubsystem;

namespace simcore
{
	class CountryMap;
	class MapData;
	class MapFrameLink;
}

/**
 * A generated map (data/maps/<id>.json, simcore/terrain_map.hpp) drawn at the least detail that reads: the ground as
 * one mesh (vertex-coloured fields, the river valley, the hills), roads, the railway and the river as ribbons, and
 * every house, wood block, revetment, hangar and runway as one box (instanced, one component per kind).
 *
 * The map is laid out in its own frame; the actor draws it in the running scenario's frame (through the Earth-centred
 * frame, so the ground falls away with the Earth's curvature as the sim's does), and rebuilds when a scenario with
 * another origin starts. The sim's line of sight uses the same ground when the scenario names the map
 * ("terrain": {"type": "map"}); a flat scenario played in this level gets a warning in the log.
 *
 * Place one in a level with no Cesium (Content/Maps/KesmarkRange, tools/ue_build_kesmark_range.py). Editor: "Build
 * Preview" draws it at the map's own origin.
 *
 * Beyond the map, the whole country it lies in (CountryFile, simcore/country_map.hpp) at strategic scale: its ground at
 * 1 km in place of the map's far ground (the same ground function, so the two agree), the land beyond its borders
 * coarser, its rivers, roads, railways and oil pipelines as ribbons, its towns as blocks on a patch, its border. And the
 * nation's assets (bDrawAssets): every object of the theatre as the basic models of its system's or its kind's site.
 *
 * -SimMapShots=<folder> photographs the map from a few views (the battery's position, over the town, the bridges, the
 * woods, high above with the far ground and the cloud, the country from very high, the refinery and the capital's
 * airfield) once it is drawn, then quits: a run checked from its pictures.
 */
UCLASS()
class SAM3D_API ASimMapActor : public AActor
{
	GENERATED_BODY()

public:
	ASimMapActor();

	/** The map, relative to the data folder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	FString MapFile = TEXT("maps/kesmark_range.json");

	/** Draw the ground beyond the map (out to its far_km, coarser), so aircraft from the borders fly over land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bDrawFarGround = true;

	/** Every how many grid samples the map's ground gets a vertex (1: every 50 m sample of kesmark_range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map", meta = (ClampMin = "1", ClampMax = "8"))
	int32 GroundStep = 1;

	/** Collision on the map's ground, bridges, runways and pads (the vehicle finds the ground by tracing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bCollision = true;

	/** The country the map lies in (data/maps/<id>.json, "kind": "country"), drawn beyond the map at strategic scale. ""
	 *  draws none (and the map's own far ground instead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	FString CountryFile = TEXT("maps/pannonia.json");

	/** The nation's assets on the map: every theatre object as its basic models where it stands, the running campaign's
	 *  (where it has moved them; the wrecked dark) or else TheatreFile's. Not what the scenario draws itself: its own
	 *  battery, its dummies and its neighbours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bDrawAssets = true;

	/** The theatre whose objects are drawn when no campaign runs (relative to the data folder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	FString TheatreFile = TEXT("air_war/theatre_pannonia_nation.json");

	/** Draw the map for the scenario that is running now (or at the map's own origin with none). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Map")
	bool BuildMap(FString& OutError);

	UFUNCTION(CallInEditor, Category = "SAM3D|Map")
	void BuildPreview();

	UFUNCTION(BlueprintPure, Category = "SAM3D|Map")
	int32 GetBoxCount() const { return BoxCount; }

	/** The assets' vehicles and buildings drawn (basic models). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Map")
	int32 GetAssetCount() const { return AssetCount; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool Build(const FVector& FrameOriginLonLatAlt, const FString& DataDirectory, FString& OutError);
	void Clear();
	void RunShots(float DeltaSeconds);
	void BuildGround(const simcore::MapFrameLink& Link);
	void BuildStrips(const simcore::MapFrameLink& Link);
	void BuildBoxes(const simcore::MapFrameLink& Link);
	void BuildCountry(const FVector& FrameOriginLonLatAlt);
	void BuildAssets(const FVector& FrameOriginLonLatAlt);
	// The ground's height (above sea level) at a point of the country: the map's own where the map is, the country's else.
	double CountryGround(double East, double North) const;
	UMaterialInterface* ColourMaterial(const FLinearColor& Colour);
	static FString FindDataDirectory();

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Map")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> GroundTiles;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Strips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;

	std::shared_ptr<const simcore::MapData> Map;
	std::shared_ptr<const simcore::CountryMap> Country;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CountryStrips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> AssetParts;  // the assets' holders and their models' parts
	int32 AssetCount = 0;
	FVector BuiltFor = FVector(1e9);  // the frame origin (lon, lat, alt) the meshes are in
	int32 BoxCount = 0;
	float CheckTimer = 0.0f;
	bool bWarnedFlat = false;

	// -SimMapShots
	FString ShotFolder;
	int32 ShotIndex = -1;
	float ShotTimer = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ShotCamera;
};
