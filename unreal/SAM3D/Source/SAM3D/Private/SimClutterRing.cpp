#include "SimClutterRing.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "SAM3D.h"
#include "SimCoreIncludes.h"
#include "SimMapAssets.h"
#include "SimWorldSubsystem.h"
#include "simcore/scenery/clutter.hpp"

namespace
{
	// Over this height above the ground the ring is empty: a plant a hand high is nothing from a hundred metres up.
	constexpr double MaxCameraHeightM = 150.0;
}

USimClutterRing::USimClutterRing()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool USimClutterRing::Init(const std::shared_ptr<const simcore::MapData>& InMap, const std::shared_ptr<const simcore::MapFrameLink>& InLink,
	const std::shared_ptr<const simcore::scenery::CoverRaster>& InCover, const std::shared_ptr<const simcore::scenery::DressingRules>& InDressing,
	const std::shared_ptr<const simcore::scenery::GroundLayerRules>& InLayers, const std::shared_ptr<const simcore::scenery::ClutterRules>& InRules,
	int32 InDay)
{
	Map = InMap;
	Link = InLink;
	Cover = InCover;
	Dressing = InDressing;
	Layers = InLayers;
	Rules = InRules;
	Day = InDay;
	if (!Map || !Link || !Cover || !Dressing || !Layers || !Rules)
	{
		return false;
	}
	// The clutter material: the vertex colours times each instance's brightness (tools/ue_build_tree_material.py). Without
	// it the plain vertex colour material draws them, every plant of a kind the same.
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Map/M_SimMapClutter.M_SimMapClutter"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	bTinted = Material != nullptr;
	const simcore::scenery::AssetRegister* Reg = FSimMapAssets::Register();
	if (Material == nullptr)
	{
		Material = FSimMapAssets::Material();
		UE_LOG(LogSAM3D, Log, TEXT("Clutter: no M_SimMapClutter (tools/ue_build_tree_material.py): every plant of a kind the same brightness"));
	}
	for (const std::string& PartStd : Rules->parts)
	{
		const FString Part = UTF8_TO_TCHAR(PartStd.c_str());
		FTransform Fix;
		UStaticMesh* Mesh = FSimMapAssets::MeshFor(Part, 0, Fix);
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(GetOwner(), FName(*FString::Printf(TEXT("Clutter_%s"), *Part)));
		Ism->SetupAttachment(this);
		Ism->SetStaticMesh(Mesh);
		// A registered model (a card with its alpha texture) keeps its own materials, which read each plant's brightness
		// from the instance's custom data (M_SimFoliage); a procedural part wears the vertex-colour clutter material.
		const bool bReal = Reg != nullptr && Reg->AssetFor(PartStd, 0) != nullptr;
		if (Material != nullptr && !bReal)
		{
			Ism->SetMaterial(0, Material);
		}
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCastShadow(false);
		Ism->SetCullDistances(0, static_cast<int32>(Rules->ring_m * 100.0 + 500.0));
		Ism->NumCustomDataFloats = 3;  // each plant's brightness, read by M_SimMapClutter or M_SimFoliage
		Ism->RegisterComponent();
		Parts.Add(Ism);
		Fixes.Add(Fix);
		if (bReal)
		{
			bTinted = true;
		}
	}
	SetComponentTickEnabled(true);
	UE_LOG(LogSAM3D, Log, TEXT("Clutter: a ring of %.0f m, %d parts, the colours of day %d"), Rules->ring_m, Parts.Num(), Day);
	return Parts.Num() > 0;
}

void USimClutterRing::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	for (UInstancedStaticMeshComponent* P : Parts)
	{
		if (P != nullptr)
		{
			P->DestroyComponent();
		}
	}
	Parts.Reset();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USimClutterRing::EmptyRing()
{
	for (UInstancedStaticMeshComponent* P : Parts)
	{
		if (P != nullptr)
		{
			P->ClearInstances();
		}
	}
	LastCount = 0;
	// A build under way was for the ring just emptied: dropped when it lands.
	++Generation;
}

void USimClutterRing::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Map || !Link || Parts.Num() == 0 || GetWorld() == nullptr)
	{
		return;
	}
	// A finished build swaps in (unless the ring was emptied since it started).
	if (Pending.IsValid() && Pending.IsReady())
	{
		const TSharedPtr<FSimClutterBuild> Done = Pending.Get();
		Pending = TFuture<TSharedPtr<FSimClutterBuild>>();
		if (Done.IsValid() && Done->Generation == Generation)
		{
			Apply(*Done);
		}
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (Camera == nullptr)
	{
		return;
	}
	const FVector Eye = Camera->GetCameraLocation();
	const simcore::Vec3 P = USimWorldSubsystem::UnrealToEnu(Eye);
	const simcore::MapPoint Mp = Link->FrameToMap(P.x, P.y);
	// Over the map's own ground only, and low over it.
	const double Ground = Link->GroundPoint(Mp.east_m, Mp.north_m, Map->GroundHeight(Mp.east_m, Mp.north_m)).z;
	if (!Cover->GridAt(Mp.east_m, Mp.north_m) || P.z - Ground > MaxCameraHeightM)
	{
		if (bBuilt || Pending.IsValid())
		{
			EmptyRing();
			bBuilt = false;
		}
		return;
	}
	// One build at a time; the ring drawn stays until the next lands.
	if (Pending.IsValid())
	{
		return;
	}
	if (bBuilt && FMath::Square(Mp.east_m - LastEast) + FMath::Square(Mp.north_m - LastNorth) < FMath::Square(Rules->rebuild_m))
	{
		return;
	}
	Rebuild(Mp.east_m, Mp.north_m);
}

void USimClutterRing::Rebuild(double MapEast, double MapNorth)
{
	// Everything the worker reads is the map's immutable data (held by the lambda, so it outlives the component) and
	// copies; nothing of the component is touched off the game thread.
	LastEast = MapEast;
	LastNorth = MapNorth;
	const int32 NumParts = Parts.Num();
	const TArray<FTransform> PartFixes = Fixes;
	const bool bWithData = bTinted;
	const int32 Gen = Generation;
	Pending = Async(EAsyncExecution::ThreadPool,
		[MapP = Map, LinkP = Link, CoverP = Cover, DressingP = Dressing, LayersP = Layers, RulesP = Rules, D = Day, MapEast, MapNorth, NumParts, PartFixes,
			bWithData, Gen]() -> TSharedPtr<FSimClutterBuild>
		{
			const double T0 = FPlatformTime::Seconds();
			TSharedPtr<FSimClutterBuild> B = MakeShared<FSimClutterBuild>();
			B->East = MapEast;
			B->North = MapNorth;
			B->Generation = Gen;
			std::vector<simcore::scenery::ClutterPlant> Plants;
			simcore::scenery::ClutterAround(*RulesP, *CoverP, *DressingP, *LayersP, D, MapEast, MapNorth, Plants);
			B->Xf.SetNum(NumParts);
			B->Data.SetNum(NumParts);
			for (const simcore::scenery::ClutterPlant& Plant : Plants)
			{
				// The part the plant drew among its kind's variants.
				const int32 Part = Plant.part;
				if (Part < 0 || Part >= NumParts)
				{
					continue;
				}
				const double H = MapP->GroundHeight(Plant.east_m, Plant.north_m);
				const FVector At = USimWorldSubsystem::EnuToUnreal(LinkP->GroundPoint(Plant.east_m, Plant.north_m, H - 0.02));
				const FTransform T(FRotator(0.0, FMath::RadiansToDegrees(Plant.yaw_rad), 0.0), At, FVector(Plant.width_m, Plant.width_m, Plant.height_m));
				B->Xf[Part].Add(PartFixes[Part] * T);
				if (bWithData)
				{
					// Its brightness times its colour (A.3.40: the kind's tints), read by M_SimMapClutter or M_SimFoliage.
					B->Data[Part].Add(FVector3f(Plant.brightness * Plant.tint[0], Plant.brightness * Plant.tint[1], Plant.brightness * Plant.tint[2]));
				}
			}
			B->Count = static_cast<int32>(Plants.size());
			B->Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
			return B;
		});
}

void USimClutterRing::Apply(const FSimClutterBuild& Build)
{
	const double T0 = FPlatformTime::Seconds();
	const TArray<TArray<FTransform>>& Xf = Build.Xf;
	const TArray<TArray<FVector3f>>& Data = Build.Data;
	for (int32 K = 0; K < Parts.Num() && K < Xf.Num(); ++K)
	{
		UInstancedStaticMeshComponent* Ism = Parts[K];
		if (Ism == nullptr)
		{
			continue;
		}
		Ism->ClearInstances();
		if (Xf[K].Num() == 0)
		{
			continue;
		}
		Ism->AddInstances(Xf[K], false, true);
		if (bTinted && K < Data.Num())
		{
			for (int32 I = 0; I < Data[K].Num(); ++I)
			{
				const FVector3f& C = Data[K][I];
				const float V[3] = {C.X, C.Y, C.Z};
				Ism->SetCustomData(I, MakeArrayView(V, 3), false);
			}
			Ism->MarkRenderStateDirty();
		}
	}
	bBuilt = true;
	LastEast = Build.East;
	LastNorth = Build.North;
	LastCount = Build.Count;
	UE_LOG(LogSAM3D, Verbose, TEXT("Clutter: %d plants round (%.0f, %.0f), scattered in %.1f ms on a worker, swapped in in %.1f ms"), LastCount, Build.East,
		Build.North, Build.Ms, (FPlatformTime::Seconds() - T0) * 1000.0);
}
