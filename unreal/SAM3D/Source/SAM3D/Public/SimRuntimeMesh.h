#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * A static mesh built at run time from simcore's triangle meshes (simcore/scenery/trimesh.hpp), one per LOD: the munitions
 * (FSimMunitionModels) and the map's trees, buildings and wood masses (FSimMapAssets) are drawn through it.
 *
 * simcore's metres become cm; its x, y, z are Unreal's X, Y, Z (the same handedness: nothing is mirrored); each triangle
 * is rewound for Unreal's front face; the vertex colours reach the GPU as the linear colours themselves (the material
 * takes the vertex colour as its base colour unconverted). One material slot, no collision, no CPU access; the mesh is
 * transient and rooted (kept for the session, never collected).
 */
class SAM3D_API FSimRuntimeMesh
{
public:
	/**
	 * The mesh of `Lods` (LOD 0 first; at least one, none empty) with `Material` in its one slot, named "SM_" + Name;
	 * LOD L drawn below screen size ScreenSizes[L] (missing sizes keep the engine's). The slot is named SlotName ("Main" if
	 * none). Null if it does not build (logged).
	 */
	static UStaticMesh* Build(const TArray<simcore::scenery::TriMesh>& Lods, const FString& Name, UMaterialInterface* Material,
		TArrayView<const float> ScreenSizes, FName SlotName = NAME_None);
};
