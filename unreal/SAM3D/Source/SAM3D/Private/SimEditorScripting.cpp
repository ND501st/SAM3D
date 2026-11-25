#include "SimEditorScripting.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

bool USimEditorScripting::SetDistanceFieldScale(UStaticMesh* Mesh, float Scale)
{
#if WITH_EDITOR
	if (Mesh == nullptr || Mesh->GetNumSourceModels() == 0)
	{
		return false;
	}
	Mesh->Modify();
	Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale = Scale;
	Mesh->Build(true);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

UTexture2DArray* USimEditorScripting::MakeTextureArray(const FString& PackagePath, const TArray<UTexture2D*>& Slices)
{
#if WITH_EDITOR
	if (Slices.Num() == 0 || !FPackageName::IsValidLongPackageName(PackagePath))
	{
		return nullptr;
	}
	for (const UTexture2D* T : Slices)
	{
		if (T == nullptr)
		{
			return nullptr;
		}
	}
	UPackage* Package = CreatePackage(*PackagePath);
	const FString Name = FPackageName::GetLongPackageAssetName(PackagePath);
	UTexture2DArray* Array = FindObject<UTexture2DArray>(Package, *Name);
	const bool bNew = Array == nullptr;
	if (bNew)
	{
		Array = NewObject<UTexture2DArray>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	}
	Array->Modify();
	Array->SourceTextures.Reset();
	for (UTexture2D* T : Slices)
	{
		Array->SourceTextures.Add(T);
	}
	if (!Array->CheckArrayTexturesCompatibility())
	{
		return nullptr;
	}
	// The slices' own settings (they agree: CheckArrayTexturesCompatibility).
	Array->SRGB = Slices[0]->SRGB;
	Array->CompressionSettings = Slices[0]->CompressionSettings;
	Array->UpdateSourceFromSourceTextures(bNew);
	// Their mips too, set after the update (which leaves a new array without any): a texture seen far off without them
	// shimmers.
	Array->MipGenSettings = Slices[0]->MipGenSettings;
	Array->LODGroup = Slices[0]->LODGroup;
	Array->PostEditChange();
	Array->MarkPackageDirty();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Array);
	}
	return Array;
#else
	return nullptr;
#endif
}

float USimEditorScripting::GetDistanceFieldScale(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	if (Mesh != nullptr && Mesh->GetNumSourceModels() > 0)
	{
		return Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale;
	}
#endif
	return -1.0f;
}
