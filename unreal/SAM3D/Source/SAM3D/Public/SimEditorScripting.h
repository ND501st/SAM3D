#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SimEditorScripting.generated.h"

class UStaticMesh;
class UTexture2D;
class UTexture2DArray;

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

	/** A texture array asset at a package path (/Game/...), its slices the textures in order, built and marked dirty
	 *  for the caller to save (roadmap T1: the ground's layers, tools/ue_build_ground_material.py). An asset already
	 *  there is reused. Null if the slices differ in size or format, or outside the editor. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Editor")
	static UTexture2DArray* MakeTextureArray(const FString& PackagePath, const TArray<UTexture2D*>& Slices);
};
