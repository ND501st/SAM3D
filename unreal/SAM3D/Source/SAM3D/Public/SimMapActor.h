#pragma once

#include <memory>
#include <vector>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimMapActor.generated.h"

class ACameraActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UTexture2D;
class USimWorldSubsystem;
class UTexture2D;

namespace simcore
{
	class CountryMap;
	class MapData;
	class MapFrameLink;
	struct MapGrid;
	namespace scenery
	{
		struct CoverGrid;
		struct CoverRaster;
		struct DressingRules;
		struct GroundLayerRules;
		struct MapDressing;
		class StandingCrops;
	}
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
 * 1 km beyond the map's far ground and the land beyond its borders at 4 km (levels of the map's own ground, which the
 * sim samples), its rivers, roads, railways and oil pipelines as ribbons, its towns as blocks on ground tinted a town's,
 * its border. And the nation's assets (bDrawAssets): every object of the theatre as the basic models of its system's or
 * its kind's site.
 *
 * One ground (roadmap R5): every vertex is simcore's (MapData::DrawnLevels, MapFrameLink::GroundPoint), every cell split
 * as simcore samples it (MapGrid::SampleTri), so what the sim has on the ground stands on what is drawn; GroundAt gives
 * it without a trace. SAM3D.Map.GroundAgrees checks the built meshes against the sim's ground (DrawnGroundAt).
 *
 * The map's dressing (roadmap R6, simcore/scenery/map_dressing.hpp, data/scenery/map_dressing.json): the ground of the
 * map proper and the detail areas coloured from the land cover (fields by parcel and crop, woods, towns, military
 * ground); the woods as trees (the map proper's and the detail areas', instanced by kind) and masses (the rest), never
 * taller than what blocks the sim's sight lines; houses, barns, flats and halls as their meshes (FSimMapAssets: a
 * registered asset if it is on this machine, else simcore's procedural one); a bridge as its deck, parapets and piers
 * over an invisible deck the traces land on; roads with shoulders, rivers with banks.
 *
 * -SimMapShots=<folder> photographs the map from a few views (the battery's position, over the town, the bridges, the
 * woods, high above with the far ground and the cloud, the country from very high, the refinery and the capital's
 * airfield; a tree close up, a street in Kesmark, a bridge's pier, the capital from 150 m) once it is drawn, then quits:
 * a run checked from its pictures.
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

	/** Deprecated (roadmap R5): the ground is always drawn whole, every level simcore samples (the map proper, its far
	 *  ground, the country's lattices), so nothing the sim has on the ground stands over a hole or under the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bDrawFarGround = true;

	/** Deprecated (roadmap R5): a vertex at every sample, as simcore samples the ground (1 is the only value used). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map", meta = (ClampMin = "1", ClampMax = "1"))
	int32 GroundStep = 1;

	/** Collision on the map's ground, bridges, runways and pads (the vehicle finds the ground by tracing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bCollision = true;

	/** The country the map lies in (data/maps/<id>.json, "kind": "country"), drawn at strategic scale: its lines and towns
	 *  beyond the map, its ground beyond the map's far ground (which is the country's, the map's "far_ground"). "" draws
	 *  none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	FString CountryFile = TEXT("maps/pannonia.json");

	/** The map's woods as trees where simcore plants them (the map proper and its detail areas; roadmap R6). Off: the woods
	 *  are masses only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bDrawTrees = true;

	/** The map proper's and the detail areas' ground drawn with its layers' textures (roadmap T1,
	 *  data/scenery/ground_layers.json, docs/scenery/TERRAIN_ROADMAP.md): a mask of each land cover cell's layer and the
	 *  ground material MI_SimGround, whose texture arrays are fetched and imported per machine. Off, or without them: the
	 *  land cover's colours alone, as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map")
	bool bTexturedGround = true;

	/** How far a tree is drawn (m): beyond it the wood's mass and the trees' crowns at their lowest LOD are gone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map", meta = (ClampMin = "500", ClampMax = "30000"))
	float TreeCullDistanceM = 8000.0f;

	/** How far the trees inside the map proper's woods are drawn (m; terrain roadmap A.3.19): nearer, the woods' masses are
	 *  hidden (by 2 km tiles) and the woods are trees throughout; farther, the edge rows and the masses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Map", meta = (ClampMin = "500", ClampMax = "30000"))
	float InteriorTreeDistanceM = 2500.0f;

	/** The nation's assets on the map: every theatre object as its basic models where it stands, the running campaign's
	 *  (where it has moved them; the wrecked dark) or else TheatreFile's. Not what the scenario draws itself: its own
	 *  battery, its dummies and its neighbours. With them the chain of command's places that are not theatre objects
	 *  (the posts, offices, works and villages of data/command/places_pannonia.json, roadmap P7) and, with a campaign
	 *  running, each post's holder where he is at its hour (simcore/command/district.hpp). */
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

	/** The drawn ground under (or over) a point of the level (roadmap R5): the map's height there on the triangle it is
	 *  drawn as, straight down the scenario frame's up, as simcore's GroundEnu has it (no trace, so nothing standing on
	 *  the ground is hit). The triangle's normal too, if asked. False before the map is drawn or beyond its ground. */
	bool GroundAt(const FVector& UnrealPos, FVector& OutGround, FVector* OutNormal = nullptr) const;

	/** A vertex of the drawn ground: node (C, R) of a level's lattice in the frame the map is drawn in (BuildGround's). */
	static FVector GroundVertex(const simcore::MapFrameLink& Link, const simcore::MapGrid& Grid, int32 C, int32 R);

	/** The ground as built (roadmap R5's checks): the highest face of the ground tiles' meshes straight under (or over)
	 *  a point, read from their sections' vertices and triangles, nothing simcore computes. GroundAt places things;
	 *  this is what they are judged against (SAM3D.Map.GroundAgrees, the ground check), so a wrong vertex, split or
	 *  tile, or a coarse cell drawn over a finer level, shows. False where no tile has a face over the point. */
	bool DrawnGroundAt(const FVector& UnrealPos, FVector& OutGround) const;

	/** Draw the map for a frame at (lon, lat, alt) (degrees, m) from the data folder FindDataDirectory finds. */
	bool BuildForFrame(const FVector& FrameOriginLonLatAlt, FString& OutError);

	/** The repository's data folder, from the project's (or the game's) folder. */
	static FString FindDataDirectory();

	/** The map drawn now (null before). */
	const simcore::MapData* GetMapData() const { return Map.get(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool Build(const FVector& FrameOriginLonLatAlt, const FString& DataDirectory, FString& OutError);
	void Clear();
	void RunShots(float DeltaSeconds);
	void BuildGround(const simcore::MapFrameLink& Link);
	void BuildStrips(const simcore::MapFrameLink& Link);
	void BuildBoxes(const simcore::MapFrameLink& Link);
	void BuildTrees(const simcore::MapFrameLink& Link);
	void UpdateMassTiles();
	void BuildBridges(const simcore::MapFrameLink& Link);
	void BuildCrops(const simcore::MapFrameLink& Link, const FString& DataDirectory);
	void LoadDressing(const FString& DataDirectory);
	/** The textured ground's material for each drawn level (null: that level wears the plain one). */
	TArray<UMaterialInterface*> TexturedGround(const simcore::MapFrameLink& Link, const std::vector<const simcore::MapGrid*>& Levels);
	void BuildCountry(const FVector& FrameOriginLonLatAlt);
	void BuildAssets(const FVector& FrameOriginLonLatAlt);
	UMaterialInterface* ColourMaterial(const FLinearColor& Colour);
	// A ribbon's material (roadmap S9, M_SimMapRibbon, tools/ue_make_map_materials.py): its colour, the texture's
	// channel it is drawn with (R asphalt, G dirt, B railway, A water) and its roughness; the flat colour without it.
	UMaterialInterface* RibbonMaterial(const FLinearColor& Colour, const FLinearColor& Channel, float Roughness);
	// The ground's material for a tile (roadmap S9, M_SimMapGround): with the land cover's grid drawn cell by cell as a
	// texture where the tile has one (null: the vertices' colours), the vertex colour material without it.
	UMaterialInterface* GroundMaterialFor(const simcore::scenery::CoverGrid* Grid);

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Map")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> GroundTiles;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Strips;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Bridges;

	/** The crops standing on the session's day (roadmap L5): a block per 1 km tile, from the sim's own (BuildCrops). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Crops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;

	/** The map proper's woods' masses by 2 km tile (also in Parts), hidden near the camera (UpdateMassTiles). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MassTiles;
	TArray<FBox> MassTileBounds;

	std::shared_ptr<const simcore::MapData> Map;
	std::shared_ptr<const simcore::CountryMap> Country;
	// The map's dressing (roadmap R6): its rules, the land cover and the trees, built once per map (LoadDressing).
	std::shared_ptr<const simcore::scenery::DressingRules> DressRules;
	std::shared_ptr<const simcore::scenery::CoverRaster> Cover;
	// The far ground's land cover (roadmap T3.2, simcore/scenery/far_cover.hpp): the country's, at 250 m, with the map's fields.
	std::shared_ptr<const simcore::scenery::CoverGrid> FarCover;
	std::shared_ptr<const simcore::scenery::MapDressing> Dressing;
	// The ground's layers (roadmap T1): which layer each land cover cell is drawn with (LoadDressing).
	std::shared_ptr<const simcore::scenery::GroundLayerRules> GroundLayers;
	// The crops standing on the session's day: the same the sim's sight lines meet (scenery/standing_crops.hpp).
	std::shared_ptr<const simcore::scenery::StandingCrops> Standing;
	// The map's frame to the frame it is drawn in (set by Build): GroundAt's.
	std::shared_ptr<const simcore::MapFrameLink> GroundLink;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CountryStrips;

	// The textured ground (roadmap T1): a mask per land cover square and the material made from each (TexturedGround).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> GroundMasks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TexturedMaterials;

	// The land cover's grids as textures, and the ground's material for each (GroundMaterialFor), by the grid's id.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInterface>> GroundMaterials;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> CoverTextures;

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
	// -SimShotSet=views: the frames' times summed over the wait before each picture, for its cost file (ms).
	double ShotGpuMs = 0.0, ShotGameMs = 0.0, ShotRenderMs = 0.0;
	int32 ShotSamples = 0;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ShotCamera;
};
