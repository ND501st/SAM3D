#include "SimMunitionModels.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
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

	/** The value whose sRGB encoding is L: the static mesh build stores a vertex colour sRGB-encoded (ToFColor(true)), so
	 *  this makes the stored byte the linear colour itself, as the material reads it. */
	float SimFxLinearAsStored(float L)
	{
		L = FMath::Clamp(L, 0.0f, 1.0f);
		return L <= 0.04045f ? L / 12.92f : FMath::Pow((L + 0.055f) / 1.055f, 2.4f);
	}

	/** A unit tangent at right angles to the normal (no normal map: any will do, but it must be one). */
	FVector3f SimFxTangentFor(const FVector3f& N)
	{
		const FVector3f Ref = FMath::Abs(N.X) < 0.9f ? FVector3f(1.0f, 0.0f, 0.0f) : FVector3f(0.0f, 1.0f, 0.0f);
		return (Ref - N * FVector3f::DotProduct(N, Ref)).GetSafeNormal();
	}

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
	if (Lods.Num() == 0 || Lods[0].Empty())
	{
		return nullptr;
	}
	TArray<FMeshDescription> Descriptions;
	Descriptions.SetNum(Lods.Num());
	for (int32 L = 0; L < Lods.Num(); ++L)
	{
		const simcore::scenery::TriMesh& T = Lods[L];
		FMeshDescription& D = Descriptions[L];
		FStaticMeshAttributes Attributes(D);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colours = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> Uvs = Attributes.GetVertexInstanceUVs();
		if (Uvs.GetNumChannels() < 1)
		{
			Uvs.SetNumChannels(1);
		}
		TPolygonGroupAttributesRef<FName> Slots = Attributes.GetPolygonGroupMaterialSlotNames();

		const int32 NumVertices = static_cast<int32>(T.positions.size());
		const bool bNormals = T.normals.size() == T.positions.size();
		const bool bColours = T.colours.size() == T.positions.size();
		D.ReserveNewVertices(NumVertices);
		D.ReserveNewVertexInstances(NumVertices);
		D.ReserveNewTriangles(static_cast<int32>(T.Triangles()));
		TArray<FVertexInstanceID> Instances;
		Instances.SetNumUninitialized(NumVertices);
		for (int32 I = 0; I < NumVertices; ++I)
		{
			// simcore's x forward, y right, z up (m) is Unreal's X forward, Y right, Z up (cm): the same handedness, so
			// nothing is mirrored (the basic models map their parts the same way).
			const simcore::Vec3& P = T.positions[static_cast<size_t>(I)];
			const FVertexID V = D.CreateVertex();
			Positions[V] = FVector3f(static_cast<float>(P.x * 100.0), static_cast<float>(P.y * 100.0), static_cast<float>(P.z * 100.0));
			const FVertexInstanceID VI = D.CreateVertexInstance(V);
			Instances[I] = VI;
			FVector3f N = FVector3f(0.0f, 0.0f, 1.0f);
			if (bNormals)
			{
				const simcore::Vec3& Nm = T.normals[static_cast<size_t>(I)];
				N = FVector3f(static_cast<float>(Nm.x), static_cast<float>(Nm.y), static_cast<float>(Nm.z)).GetSafeNormal(1e-8f, FVector3f(0.0f, 0.0f, 1.0f));
			}
			Normals[VI] = N;
			Tangents[VI] = SimFxTangentFor(N);
			Signs[VI] = 1.0f;
			FVector4f C(1.0f, 1.0f, 1.0f, 1.0f);
			if (bColours)
			{
				const std::array<float, 3>& Lin = T.colours[static_cast<size_t>(I)];
				C = FVector4f(SimFxLinearAsStored(Lin[0]), SimFxLinearAsStored(Lin[1]), SimFxLinearAsStored(Lin[2]), 1.0f);
			}
			Colours[VI] = C;
			Uvs[VI] = FVector2f(0.0f, 0.0f);
		}
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Slots[Group] = SimFxMunitionSlot;
		const size_t NumIndices = T.indices.size() - T.indices.size() % 3;
		for (size_t K = 0; K < NumIndices; K += 3)
		{
			const uint32 A = T.indices[K];
			const uint32 B = T.indices[K + 1];
			const uint32 C = T.indices[K + 2];
			if (A >= static_cast<uint32>(NumVertices) || B >= static_cast<uint32>(NumVertices) || C >= static_cast<uint32>(NumVertices) || A == B ||
				B == C || A == C)
			{
				continue;
			}
			// simcore winds (b - a) x (c - a) out of the surface; Unreal's front face is (c - a) x (b - a) (its left-handed
			// frame: GeometryCore VectorUtil::Normal), so two indices swap.
			const FVertexInstanceID Tri[3] = {Instances[static_cast<int32>(A)], Instances[static_cast<int32>(C)], Instances[static_cast<int32>(B)]};
			D.CreateTriangle(Group, MakeArrayView(Tri, 3));
		}
	}

	UPackage* Package = GetTransientPackage();
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*(TEXT("SM_FX_") + Name))),
		RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material(), SimFxMunitionSlot, SimFxMunitionSlot));
	TArray<const FMeshDescription*> Pointers;
	for (const FMeshDescription& D : Descriptions)
	{
		Pointers.Add(&D);
	}
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bUseHashAsGuid = true;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;  // no source models kept: a runtime mesh, drawn only
	Params.bFastBuild = true;
	Params.bAllowCpuAccess = false;
	if (!Mesh->BuildFromMeshDescriptions(Pointers, Params))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Munition models: %s did not build"), *Name);
		return nullptr;
	}
	if (FStaticMeshRenderData* Render = Mesh->GetRenderData())
	{
		for (int32 L = 0; L < Lods.Num() && L < MAX_STATIC_MESH_LODS; ++L)
		{
			Render->ScreenSize[L].Default = LodScreenSize(L);
		}
	}
	Mesh->AddToRoot();
	return Mesh;
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
			Built = SimFxBuildMunition(*Spec, std::string(), Key);
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
