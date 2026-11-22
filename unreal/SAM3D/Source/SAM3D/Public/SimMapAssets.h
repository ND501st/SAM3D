#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * The register of third-party art and the map's dressing as static meshes (roadmap R6, docs/scenery/ASSETS.md): a tree by kind, a wood's mass, a house, a barn,
 * flats, an industrial hall, each by its part key (simcore/scenery/map_meshes.hpp MapMeshParts) and variant.
 *
 * The register (data/scenery/assets.json, simcore/scenery/asset_register.hpp) may name a real asset for a part: it is
 * loaded from its Unreal path if it is on this machine (third-party art lives under /Game/ThirdParty/, fetched per
 * machine and ignored by git). Otherwise, or with no register, the part is simcore's procedural mesh, built once with
 * its LODs through FSimRuntimeMesh in the map's vertex colour material. Every mesh is kept for the session (rooted).
 *
 * The frame is the procedural mesh's in cm: a building or a wood's mass on the engine cube's (100 cm, centred), so the
 * map's box transform places it as it places the cube; a tree 100 cm tall from its base with a crown 100 cm across. A
 * real asset comes with its fix into that frame (OutFix), to compose before the instance's own transform.
 */
class SAM3D_API FSimMapAssets
{
public:
	/** The register, loaded once; null if it doesn't load (the reason is logged once): then every part is procedural. */
	static const simcore::scenery::AssetRegister* Register();

	/**
	 * The mesh of a part's variant (for "flats" the variant is its floor count), and in OutFix the transform from the
	 * mesh's frame into the part's (identity for a procedural mesh). Null for an unknown part. Logs once per part and
	 * variant whether it is drawn by a real asset or procedurally.
	 */
	static UStaticMesh* MeshFor(const FString& Part, int32 Variant, FTransform& OutFix);

	/** The map's material: /Game/SAM3D/Map/M_SimMapVertexColor, else the engine's VertexColorMaterial (as the ground). */
	static UMaterialInterface* Material();

	/** The screen size below which LOD 1, 2 and 3 of a procedural part are drawn. */
	static float LodScreenSize(int32 Lod);

	/**
	 * A basic model drawn whole by a real asset (the register's "models": an aircraft from Sketchfab in place of its basic
	 * shapes, docs/scenery/ASSETS.md), loaded if it is on this machine, with its fix from the asset's frame into the
	 * model's in OutFix. Null: the model keeps its basic shapes (FSimBasicModels). Logged once per model.
	 */
	static UStaticMesh* ModelMesh(const FString& ModelId, FTransform& OutFix);

	/**
	 * A munition drawn by a real asset (the register's "munitions", by the munition's own id), in the munition's own frame
	 * (the register refuses a fix for one). Null: its procedural mesh (FSimMunitionModels). Logged once per munition.
	 */
	static UStaticMesh* MunitionMesh(const FString& MunitionId);
};
