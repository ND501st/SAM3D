#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SimEditorScripting.generated.h"

class UStaticMesh;

/**
 * Asset settings the import scripts (tools/ue_import_*.py) need and the editor's Python can't reach from a commandlet:
 * there the static mesh editor subsystem doesn't exist, so EditorStaticMeshLibrary's build settings neither read nor
 * stick. Editor builds only; elsewhere the calls do nothing.
 */
UCLASS()
class SAM3D_API USimEditorScripting : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Sets a mesh's distance field resolution scale (LOD 0's build settings) and rebuilds it. False if it can't. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Editor")
	static bool SetDistanceFieldScale(UStaticMesh* Mesh, float Scale);

	/** A mesh's distance field resolution scale (-1 if it has none to read). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Editor")
	static float GetDistanceFieldScale(UStaticMesh* Mesh);
};
