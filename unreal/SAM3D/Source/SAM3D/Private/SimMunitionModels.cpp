#include "SimMunitionModels.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
#include "SimMapAssets.h"
#include "SimRuntimeMesh.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace
{
	TUniquePtr<simcore::scenery::MunitionLibrary> GSimFxMunitionLibrary;
	bool bSimFxMunitionLibraryTried = false;
	/** Built meshes by munition id (and "id#part"); a null entry is a mesh that can't be built (asked once). Each mesh is
	 *  rooted when built, so the map's raw pointers stay valid. */
	TMap<FString, UStaticMesh*> GSimFxMunitionMeshes;
	UMaterialInterface* GSimFxMunitionMaterial = nullptr;
	bool bSimFxMunitionMaterialTried = false;

	const FName SimFxMunitionSlot(TEXT("Munition"));

	FString SimFxStd(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

const simcore::scenery::MunitionLibrary* FSimMunitionModels::Library()
{
	if (!bSimFxMunitionLibraryTried)
	{
		bSimFxMunitionLibraryTried = true;
		TUniquePtr<simcore::scenery::MunitionLibrary> Lib = MakeUnique<simcore::scenery::MunitionLibrary>();
		const FString Path = FPaths::Combine(USimAirCampaignSubsystem::DataDirectory(), TEXT("scenery/munitions.json"));
		std::string Error;
		if (Lib->Load(std::string(TCHAR_TO_UTF8(*Path)), &Error))
		{
			GSimFxMunitionLibrary = MoveTemp(Lib);
			UE_LOG(LogSAM3D, Log, TEXT("Munition models: %d loaded from %s"), static_cast<int32>(GSimFxMunitionLibrary->munitions.size()), *Path);
		}
		else
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Munition models don't load (%s): missiles stay basic models and cones"), *SimFxStd(Error));
		}
	}
	return GSimFxMunitionLibrary.Get();
}

const simcore::scenery::MunitionSpec* FSimMunitionModels::Resolve(const FString& AnyId)
{
	const simcore::scenery::MunitionLibrary* Lib = Library();
	if (Lib == nullptr || AnyId.IsEmpty())
	{
		return nullptr;
	}
	return Lib->Resolve(std::string(TCHAR_TO_UTF8(*AnyId)));
}

UMaterialInterface* FSimMunitionModels::Material()
{
	if (!bSimFxMunitionMaterialTried)
	{
		bSimFxMunitionMaterialTried = true;
		GSimFxMunitionMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/FX/Materials/M_FX_Munition.M_FX_Munition"), nullptr,
			LOAD_NoWarn | LOAD_Quiet);
		if (GSimFxMunitionMaterial == nullptr)
		{
			GSimFxMunitionMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
			UE_LOG(LogSAM3D, Log, TEXT("Munition models: no M_FX_Munition (tools/ue_make_fx_assets.py), drawn with the engine's vertex colour material"));
		}
		if (GSimFxMunitionMaterial != nullptr)
		{
			GSimFxMunitionMaterial->AddToRoot();
		}
	}
	return GSimFxMunitionMaterial;
}

float FSimMunitionModels::LodScreenSize(int32 Lod)
{
	// A missile is long and thin: its 24 sides matter only close up. The bounds' sphere filling a quarter of the screen,
	// then 8 %, then 2 % (a 3 m missile about 20 px long at 1080 lines): there the box.
	static const float Sizes[] = {1.0f, 0.25f, 0.08f, 0.02f};
	return Sizes[FMath::Clamp(Lod, 0, 3)];
}

UStaticMesh* FSimMunitionModels::BuildStaticMesh(const TArray<simcore::scenery::TriMesh>& Lods, const FString& Name)
{
	// The runtime mesh builder (FSimRuntimeMesh, shared with the map's meshes) with the munitions' material and LOD sizes.
	const float Sizes[] = {LodScreenSize(0), LodScreenSize(1), LodScreenSize(2), LodScreenSize(3)};
	return FSimRuntimeMesh::Build(Lods, TEXT("FX_") + Name, Material(), MakeArrayView(Sizes, 4), SimFxMunitionSlot);
}

namespace
{
	/** The LODs of a munition or of one of its parts, from LOD 0 down to the first empty one. */
	UStaticMesh* SimFxBuildMunition(const simcore::scenery::MunitionSpec& Spec, const std::string& Part, const FString& Key)
	{
		TArray<simcore::scenery::TriMesh> Lods;
		for (int32 L = 0; L < 4; ++L)
		{
			simcore::scenery::TriMesh T = Part.empty() ? simcore::scenery::BuildMesh(Spec, L) : simcore::scenery::BuildPart(Spec, Part, L);
			if (T.Empty())
			{
				break;
			}
			Lods.Add(MoveTemp(T));
		}
		const double Start = FPlatformTime::Seconds();
		UStaticMesh* Mesh = FSimMunitionModels::BuildStaticMesh(Lods, Key.Replace(TEXT("#"), TEXT("_")));
		UE_LOG(LogSAM3D, Verbose, TEXT("Munition models: %s built, %d LODs, %.1f ms"), *Key, Lods.Num(), (FPlatformTime::Seconds() - Start) * 1000.0);
		return Mesh;
	}
}

UStaticMesh* FSimMunitionModels::Mesh(const FString& AnyId)
{
	if (UStaticMesh** Found = GSimFxMunitionMeshes.Find(AnyId))
	{
		return *Found;
	}
	const simcore::scenery::MunitionSpec* Spec = Resolve(AnyId);
	UStaticMesh* Built = nullptr;
	if (Spec != nullptr && Spec->HasMesh())
	{
		const FString Key = SimFxStd(Spec->id);
		if (UStaticMesh** Same = GSimFxMunitionMeshes.Find(Key))
		{
			Built = *Same;  // the same munition asked for by another of its names
		}
		else
		{
			// A real asset if the register names one for this munition and it is on this machine (docs/scenery/ASSETS.md),
			// else the munition's own shape.
			Built = FSimMapAssets::MunitionMesh(Key);
			if (Built == nullptr)
			{
				Built = SimFxBuildMunition(*Spec, std::string(), Key);
			}
			GSimFxMunitionMeshes.Add(Key, Built);
		}
	}
	GSimFxMunitionMeshes.Add(AnyId, Built);
	return Built;
}

UStaticMesh* FSimMunitionModels::PartMesh(const FString& AnyId, const FString& Part)
{
	const FString AskedKey = AnyId + TEXT("#") + Part;
	if (UStaticMesh** Found = GSimFxMunitionMeshes.Find(AskedKey))
	{
		return *Found;
	}
	const simcore::scenery::MunitionSpec* Spec = Resolve(AnyId);
	UStaticMesh* Built = nullptr;
	if (Spec != nullptr && Spec->HasMesh() && !Part.IsEmpty())
	{
		const FString Key = SimFxStd(Spec->id) + TEXT("#") + Part;
		if (UStaticMesh** Same = GSimFxMunitionMeshes.Find(Key))
		{
			Built = *Same;
		}
		else if (simcore::scenery::PartReplacesBody(std::string(TCHAR_TO_UTF8(*Part))) &&
				 FSimMapAssets::MunitionMesh(SimFxStd(Spec->id)) != nullptr)
		{
			// The whole munition in another state (the Snakeye's blades open, the wings folded): a real asset has no such
			// state, so it stays as it is; Built stays null.
			GSimFxMunitionMeshes.Add(Key, Built);
		}
		else
		{
			Built = SimFxBuildMunition(*Spec, std::string(TCHAR_TO_UTF8(*Part)), Key);
			GSimFxMunitionMeshes.Add(Key, Built);
		}
	}
	GSimFxMunitionMeshes.Add(AskedKey, Built);
	return Built;
}

int32 FSimMunitionModels::BuildAll()
{
	const simcore::scenery::MunitionLibrary* Lib = Library();
	if (Lib == nullptr)
	{
		return 0;
	}
	const double Start = FPlatformTime::Seconds();
	int32 Built = 0;
	for (const simcore::scenery::MunitionSpec& Spec : Lib->munitions)
	{
		if (!Spec.HasMesh())
		{
			continue;
		}
		const FString Id = SimFxStd(Spec.id);
		const bool bNew = !GSimFxMunitionMeshes.Contains(Id);
		if (Mesh(Id) != nullptr && bNew)
		{
			++Built;
		}
		for (const std::string& Part : simcore::scenery::PartsOf(Spec))
		{
			const FString PartName = SimFxStd(Part);
			const bool bNewPart = !GSimFxMunitionMeshes.Contains(Id + TEXT("#") + PartName);
			if (PartMesh(Id, PartName) != nullptr && bNewPart)
			{
				++Built;
			}
		}
	}
	UE_LOG(LogSAM3D, Log, TEXT("Munition models: %d meshes built ahead of use in %.0f ms"), Built, (FPlatformTime::Seconds() - Start) * 1000.0);
	return Built;
}
