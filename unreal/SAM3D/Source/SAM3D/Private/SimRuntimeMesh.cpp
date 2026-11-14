#include "SimRuntimeMesh.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "SAM3D.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace
{
	/** The value whose sRGB encoding is L: the static mesh build stores a vertex colour sRGB-encoded (ToFColor(true)), so
	 *  this makes the stored byte the linear colour itself, as the material reads it. */
	float SimRuntimeLinearAsStored(float L)
	{
		L = FMath::Clamp(L, 0.0f, 1.0f);
		return L <= 0.04045f ? L / 12.92f : FMath::Pow((L + 0.055f) / 1.055f, 2.4f);
	}

	/** A unit tangent at right angles to the normal (no normal map: any will do, but it must be one). */
	FVector3f SimRuntimeTangentFor(const FVector3f& N)
	{
		const FVector3f Ref = FMath::Abs(N.X) < 0.9f ? FVector3f(1.0f, 0.0f, 0.0f) : FVector3f(0.0f, 1.0f, 0.0f);
		return (Ref - N * FVector3f::DotProduct(N, Ref)).GetSafeNormal();
	}
}

UStaticMesh* FSimRuntimeMesh::Build(const TArray<simcore::scenery::TriMesh>& Lods, const FString& Name, UMaterialInterface* Material,
	TArrayView<const float> ScreenSizes, FName SlotName)
{
	if (Lods.Num() == 0 || Lods[0].Empty())
	{
		return nullptr;
	}
	const FName Slot = SlotName.IsNone() ? FName(TEXT("Main")) : SlotName;
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
			Tangents[VI] = SimRuntimeTangentFor(N);
			Signs[VI] = 1.0f;
			FVector4f C(1.0f, 1.0f, 1.0f, 1.0f);
			if (bColours)
			{
				const std::array<float, 3>& Lin = T.colours[static_cast<size_t>(I)];
				C = FVector4f(SimRuntimeLinearAsStored(Lin[0]), SimRuntimeLinearAsStored(Lin[1]), SimRuntimeLinearAsStored(Lin[2]), 1.0f);
			}
			Colours[VI] = C;
			Uvs[VI] = FVector2f(0.0f, 0.0f);
		}
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Slots[Group] = Slot;
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
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*(TEXT("SM_") + Name))),
		RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, Slot, Slot));
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
		UE_LOG(LogSAM3D, Warning, TEXT("Runtime mesh: %s did not build"), *Name);
		return nullptr;
	}
	if (FStaticMeshRenderData* Render = Mesh->GetRenderData())
	{
		for (int32 L = 0; L < Lods.Num() && L < MAX_STATIC_MESH_LODS && L < ScreenSizes.Num(); ++L)
		{
			Render->ScreenSize[L].Default = ScreenSizes[L];
		}
	}
	Mesh->AddToRoot();
	return Mesh;
}
