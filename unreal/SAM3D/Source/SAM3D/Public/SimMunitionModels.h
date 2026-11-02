#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Munition models (docs/fx/README.md 3.2, 4.3): every bomb, rocket and missile a scenario shows, as a static mesh built at
 * run time from simcore's geometry (simcore/scenery/munitions.hpp, data/scenery/munitions.json): a body of revolution with
 * its nose and tail, fins and wings as thin plates, rings and paint bands. Each mesh is built on first use with four
 * levels of detail (24, 12 and 6 sides, then a box), one vertex-coloured material and no collision, and kept for the
 * session (rooted, never collected).
 *
 * Meshes are in Unreal cm: X forward along the body, Y right, Z up (simcore's x, y, z: the same handedness), the origin
 * at the body's middle. Vertex colours reach the GPU as the linear colours themselves (as the map's procedural meshes'
 * do), so the material takes the vertex colour as its base colour unconverted.
 */
class SAM3D_API FSimMunitionModels
{
public:
	/** The library, loaded once from data/scenery/munitions.json; null if it doesn't load (the reason is logged). */
	static const simcore::scenery::MunitionLibrary* Library();

	/** A munition by its own id, a weapon's (weapons.json), an air defence system's, a basic model's (missile_9m33,
	 *  missile_arm, kh22m, tlam_c, agm86c) or an alias (the EM library's ARMs); null if none. */
	static const simcore::scenery::MunitionSpec* Resolve(const FString& AnyId);

	/** The munition's mesh as carried and in flight, LOD 0-3; null for a gun (no model) or an unknown id. */
	static UStaticMesh* Mesh(const FString& AnyId);

	/** An animated part's mesh (simcore BuildPart): "blades_open", "wings_folded", "chute", "ballute"; in the munition's
	 *  own frame. Null when it has no such part. */
	static UStaticMesh* PartMesh(const FString& AnyId, const FString& Part);

	/** The munitions' material: /Game/SAM3D/FX/Materials/M_FX_Munition (vertex colour, lit; tools/ue_make_fx_assets.py),
	 *  else the engine's VertexColorMaterial (logged once). */
	static UMaterialInterface* Material();

	/** Every munition's mesh and parts built now (about 3.5 ms each; the effects call it once, at load): built on first
	 *  use instead, a launch's or a release's first frame took that long. Returns how many were built. */
	static int32 BuildAll();

	/** The screen size (of the bounds' sphere) below which LOD 1, 2 and 3 are drawn. */
	static float LodScreenSize(int32 Lod);

	/** A mesh built from simcore triangle meshes, one per LOD (LOD 0 first; at least one, none empty), metres to cm,
	 *  each triangle wound for Unreal; null if it fails. Not cached: Mesh and PartMesh cache theirs. FSimRuntimeMesh::Build
	 *  with the munitions' material, slot and LOD sizes. */
	static UStaticMesh* BuildStaticMesh(const TArray<simcore::scenery::TriMesh>& Lods, const FString& Name);
};
