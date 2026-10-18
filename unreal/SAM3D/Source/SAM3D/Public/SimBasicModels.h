#pragma once

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Ultra-basic models (docs/scenery/README.md): every aircraft, vehicle, site and building as a few of the engine's basic
 * shapes, from data/scenery/basic_models.json (simcore/scenery/basic_models.hpp). The library is loaded once; a model is
 * built as one static mesh component per part, coloured by a dynamic instance of the engine's BasicShapeMaterial.
 * Placeholders until real models exist: the right size, a recognisable silhouette, a colour per side.
 */
class SAM3D_API FSimBasicModels
{
public:
	/** The library, loaded once from the data folder; null if it doesn't load (the reason is logged). */
	static const simcore::scenery::ModelLibrary* Library();

	/** Build a model under Parent (its origin at Parent's, x forward); one component per part. Returns the parts built
	 *  (0 if there is no such model). Override, if given, colours every part (a wreck, a highlight). */
	static int32 Build(const FString& ModelId, USceneComponent* Parent, TArray<UStaticMeshComponent*>* OutParts = nullptr,
		const FLinearColor* Override = nullptr);

	/** A part's transform in its model (Unreal centimetres: X forward, Y right, Z up), for this mesh's bounds. */
	static FTransform PartTransform(const simcore::scenery::Part& Part, const UStaticMesh* Mesh);

	/** The engine's basic shape for a part. */
	static UStaticMesh* ShapeMesh(simcore::scenery::Shape Shape);

	/** A material of one colour ("#rrggbb"), shared by every part of that colour. */
	static UMaterialInterface* ColourMaterial(const FString& Hex);
	static UMaterialInterface* ColourMaterial(const FLinearColor& Colour);
};
