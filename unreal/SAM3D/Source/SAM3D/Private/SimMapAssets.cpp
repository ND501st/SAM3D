#include "SimMapAssets.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
#include "SimRuntimeMesh.h"

namespace
{
	TUniquePtr<simcore::scenery::AssetRegister> GSimMapAssetRegister;
	bool bSimMapAssetRegisterTried = false;
	UMaterialInterface* GSimMapAssetMaterial = nullptr;
	bool bSimMapAssetMaterialTried = false;

	/** A built or loaded mesh with its fix into the part's frame; a null mesh is a part that can't be had (asked once). */
	struct FSimMapAssetMesh
	{
		UStaticMesh* Mesh = nullptr;
		FTransform Fix = FTransform::Identity;
	};
	/** By "part#variant". Each mesh is rooted when built or loaded, so the raw pointers stay valid. */
	TMap<FString, FSimMapAssetMesh> GSimMapAssetMeshes;

	FString SimMapStd(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

const simcore::scenery::AssetRegister* FSimMapAssets::Register()
{
	if (!bSimMapAssetRegisterTried)
	{
		bSimMapAssetRegisterTried = true;
		TUniquePtr<simcore::scenery::AssetRegister> Reg = MakeUnique<simcore::scenery::AssetRegister>();
		const FString Path = FPaths::Combine(USimAirCampaignSubsystem::DataDirectory(), TEXT("scenery/assets.json"));
		std::string Error;
		if (Reg->Load(std::string(TCHAR_TO_UTF8(*Path)), &Error))
		{
			GSimMapAssetRegister = MoveTemp(Reg);
			UE_LOG(LogSAM3D, Log, TEXT("Map assets: %d registered in %s"), static_cast<int32>(GSimMapAssetRegister->assets.size()), *Path);
		}
		else
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Map assets: the register doesn't load (%s): every part is procedural"), *SimMapStd(Error));
		}
	}
	return GSimMapAssetRegister.Get();
}

UMaterialInterface* FSimMapAssets::Material()
{
	if (!bSimMapAssetMaterialTried)
	{
		bSimMapAssetMaterialTried = true;
		GSimMapAssetMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Map/M_SimMapVertexColor.M_SimMapVertexColor"), nullptr,
			LOAD_NoWarn | LOAD_Quiet);
		if (GSimMapAssetMaterial == nullptr)
		{
			GSimMapAssetMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
			UE_LOG(LogSAM3D, Log, TEXT("Map assets: no M_SimMapVertexColor, the procedural parts drawn with the engine's vertex colour material"));
		}
		if (GSimMapAssetMaterial != nullptr)
		{
			GSimMapAssetMaterial->AddToRoot();
		}
	}
	return GSimMapAssetMaterial;
}

float FSimMapAssets::LodScreenSize(int32 Lod)
{
	// A tree or a house fills little of the screen: its full shape while its bounds' sphere is a tenth of the screen or
	// more, then 3 %, then 1 % (a 15 m tree about 10 px tall at 1080 lines): there the crown alone. Game values, to be
	// tuned against -SimMapShots.
	static const float Sizes[] = {1.0f, 0.1f, 0.03f, 0.01f};
	return Sizes[FMath::Clamp(Lod, 0, 3)];
}

UStaticMesh* FSimMapAssets::MeshFor(const FString& Part, int32 Variant, FTransform& OutFix)
{
	OutFix = FTransform::Identity;
	const FString Key = FString::Printf(TEXT("%s#%d"), *Part, Variant);
	if (const FSimMapAssetMesh* Found = GSimMapAssetMeshes.Find(Key))
	{
		OutFix = Found->Fix;
		return Found->Mesh;
	}
	FSimMapAssetMesh Entry;
	const std::string PartStd(TCHAR_TO_UTF8(*Part));

	// A real asset, if the register names one and it is on this machine.
	const simcore::scenery::AssetRegister* Reg = Register();
	const simcore::scenery::AssetEntry* Asset = Reg != nullptr ? Reg->AssetFor(PartStd, Variant) : nullptr;
	if (Asset != nullptr)
	{
		const FString Path = SimMapStd(Asset->unreal_path);
		if (UStaticMesh* Real = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Real->AddToRoot();
			Entry.Mesh = Real;
			Entry.Fix = FTransform(FRotator(0.0, Asset->yaw_deg, 0.0), FVector(0.0, 0.0, Asset->z_offset_m * 100.0), FVector(Asset->scale));
			UE_LOG(LogSAM3D, Log, TEXT("Map assets: %s drawn by %s (%s, %s), %d LODs, %d material slots"), *Key, *SimMapStd(Asset->id), *Path,
					*SimMapStd(Asset->licence), Real->GetNumLODs(), Real->GetStaticMaterials().Num());
		}
		else
		{
			UE_LOG(LogSAM3D, Log, TEXT("Map assets: %s's asset %s is not on this machine (%s; docs/scenery/ASSETS.md): procedural"), *Key,
				*SimMapStd(Asset->id), *Path);
		}
	}

	// Else simcore's own mesh, LOD 0 down to the first empty one.
	if (Entry.Mesh == nullptr)
	{
		TArray<simcore::scenery::TriMesh> Lods;
		for (int32 L = 0; L < 4; ++L)
		{
			simcore::scenery::TriMesh T = simcore::scenery::MapPartMesh(PartStd, Variant, L);
			if (T.Empty())
			{
				break;
			}
			Lods.Add(MoveTemp(T));
		}
		if (Lods.Num() > 0)
		{
			const float Sizes[] = {LodScreenSize(0), LodScreenSize(1), LodScreenSize(2), LodScreenSize(3)};
			Entry.Mesh = FSimRuntimeMesh::Build(Lods, FString::Printf(TEXT("Map_%s_%d"), *Part, Variant), Material(), MakeArrayView(Sizes, 4));
			UE_LOG(LogSAM3D, Log, TEXT("Map assets: %s procedural (%d LODs)"), *Key, Lods.Num());
		}
		else
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Map assets: no part %s"), *Part);
		}
	}
	GSimMapAssetMeshes.Add(Key, Entry);
	OutFix = Entry.Fix;
	return Entry.Mesh;
}
