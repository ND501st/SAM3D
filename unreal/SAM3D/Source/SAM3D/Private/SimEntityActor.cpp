#include "SimEntityActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "SimBasicModels.h"
#include "SimPu12MastComponent.h"
#include "SimWorldSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// The effects' subsystem (u1's USimFxSubsystem: the wrecks' falls) arrives with the weapons effects; until its header is
// in, a destroyed aircraft simply hides.
#if __has_include("SimFxSubsystem.h")
#include "SimFxSubsystem.h"
#define SIM_ENTITY_HAS_FX_SUBSYSTEM 1
#else
#define SIM_ENTITY_HAS_FX_SUBSYSTEM 0
#endif

ASimEntityActor::ASimEntityActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));  // cone tip (+Z) -> +X

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BoxMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ConeMesh.Object);
	}
}

void ASimEntityActor::UseGroundStandIn()
{
	if (BoxMesh != nullptr)
	{
		Mesh->SetStaticMesh(BoxMesh);
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
		Mesh->SetRelativeLocation(FVector(0.0, 0.0, 50.0));  // the box stands on the ground
	}
}

bool ASimEntityActor::UseBasicModel(const FString& ModelId)
{
	if (ModelId.IsEmpty() || ModelId == BasicModel)
	{
		return !ModelId.IsEmpty();
	}
	if (FSimBasicModels::Library() == nullptr || FSimBasicModels::Library()->Find(TCHAR_TO_UTF8(*ModelId)) != nullptr)
	{
		FSimBasicModels::RemoveTurningAntennas(this);  // the old model's: its parts go below
	}
	TArray<UStaticMeshComponent*> Parts;
	if (FSimBasicModels::Build(ModelId, Root, &Parts) == 0)
	{
		return false;
	}
	for (TObjectPtr<UStaticMeshComponent>& Old : ModelParts)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	ModelParts.Reset();
	for (UStaticMeshComponent* P : Parts)
	{
		ModelParts.Add(P);
	}
	Mesh->SetVisibility(false);
	BasicModel = ModelId;
	return true;
}

void ASimEntityActor::UseOsaStandIn()
{
	if (BoxMesh == nullptr)
	{
		return;
	}
	// The own vehicle's placeholder proportions (SimOsaVehicle): hull 9.1 x 2.8 x 1.6 m, turret 2.4 x 2.0 x 1.2 m, the
	// search antenna folded down on the turret.
	struct Part
	{
		FVector SizeM;
		FVector CentreM;
	};
	const Part Parts[] = {{FVector(9.1, 2.8, 1.6), FVector(0.0, 0.0, 1.4)}, {FVector(2.4, 2.0, 1.2), FVector(-1.0, 0.0, 2.8)},
		{FVector(1.4, 2.4, 0.2), FVector(-1.6, 0.0, 3.5)}};
	Mesh->SetStaticMesh(BoxMesh);
	Mesh->SetRelativeRotation(FRotator::ZeroRotator);
	Mesh->SetRelativeScale3D(Parts[0].SizeM);
	Mesh->SetRelativeLocation(Parts[0].CentreM * 100.0);
	for (int32 I = 1; I < 3; ++I)
	{
		UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
		Box->SetupAttachment(Root);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetStaticMesh(BoxMesh);
		Box->RegisterComponent();
		Box->SetRelativeScale3D(Parts[I].SizeM);
		Box->SetRelativeLocation(Parts[I].CentreM * 100.0);
	}
}

bool ASimEntityActor::UseVehicleModel(const FString& Kind)
{
	const TCHAR* Path = Kind == TEXT("reload") ? TEXT("/Game/SAM3D/Vehicles/SM_TZM_9T217.SM_TZM_9T217")
		: Kind == TEXT("fuel") ? TEXT("/Game/SAM3D/Vehicles/SM_ATZ_38_131.SM_ATZ_38_131")
		: Kind == TEXT("command_post") ? TEXT("/Game/SAM3D/Vehicles/SM_PU12M.SM_PU12M")
		: nullptr;
	if (Path == nullptr)
	{
		return false;
	}
	UStaticMesh* Model = LoadObject<UStaticMesh>(nullptr, Path);
	if (Model == nullptr)
	{
		return false;
	}
	Mesh->SetStaticMesh(Model);
	Mesh->SetRelativeRotation(FRotator::ZeroRotator);
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	if (Kind == TEXT("command_post") && Mast == nullptr)
	{
		// The PU-12M's moving parts: the mast's sections, the guys, the AB-1 set (tools/blender/pu12m_model.py).
		Mast = NewObject<USimPu12MastComponent>(this, TEXT("Pu12Mast"));
		Mast->SetupAttachment(Mesh);
		Mast->RegisterComponent();
	}
	return true;
}

FVector ASimEntityActor::GroundBelow(const FVector& At, const USimWorldSubsystem& Sim)
{
	// One ground (roadmap R5): on a map's level the map's drawn ground at this east and north, the very triangles the sim
	// samples, so a battery row's ground point is already on it.
	FVector Ground;
	if (Sim.GetGroundMap() != nullptr && Sim.GroundAt(At, Ground, nullptr, this))
	{
		return Ground;
	}
	// A level without a map actor (Cesium's tiles, which load late): a ground-only trace from 3 km over (GroundAt's
	// fallback), made again every half second or once it has moved more than a metre; between, the offset found is kept.
	UWorld* MyWorld = GetWorld();
	const double Now = MyWorld != nullptr ? MyWorld->GetTimeSeconds() : 0.0;
	if (MyWorld != nullptr && (LastGroundTraceSeconds < 0.0 || Now - LastGroundTraceSeconds >= 0.5 ||
								  FVector::DistSquared(At, LastGroundTraceFrom) > 100.0 * 100.0))
	{
		LastGroundTraceSeconds = Now;
		LastGroundTraceFrom = At;
		GroundOffset = FVector::ZeroVector;
		if (Sim.GroundAt(At, Ground, nullptr, this))
		{
			GroundOffset = Ground - At;
		}
	}
	return At + GroundOffset;
}

void ASimEntityActor::DarkenAsWreck()
{
	if (bDarkened)
	{
		return;
	}
	bDarkened = true;
	UMaterialInterface* Dark = FSimBasicModels::ColourMaterial(FString(TEXT("#2b2a28")));
	if (Dark == nullptr)
	{
		return;
	}
	auto Paint = [Dark](UStaticMeshComponent* C)
	{
		if (C != nullptr)
		{
			for (int32 I = 0; I < FMath::Max(1, C->GetNumMaterials()); ++I)
			{
				C->SetMaterial(I, Dark);
			}
		}
	};
	Paint(Mesh);
	for (TObjectPtr<UStaticMeshComponent>& Part : ModelParts)
	{
		Paint(Part.Get());
	}
}

void ASimEntityActor::FollowWreck(const USimWorldSubsystem& Sim)
{
#if SIM_ENTITY_HAS_FX_SUBSYSTEM
	const USimFxSubsystem* Fx = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimFxSubsystem>() : nullptr;
	FVector Loc;
	FRotator Rot;
	bool bVisible = false;
	if (Fx != nullptr && Fx->GetWreckPose(EntityId, Loc, Rot, bVisible))
	{
		bWreckSeen = true;
		if (bVisible)
		{
			// Falling, burning: drawn where its wreck is, blackened.
			DarkenAsWreck();
			SetActorLocationAndRotation(Loc, Rot);
			SetActorHiddenInGame(false);
		}
		else if (bHideWhenInactive)
		{
			SetActorHiddenInGame(true);  // down: the impact's explosion and smoke take its place
		}
		return;
	}
#endif
	// No wreck (yet): the effects may hear of the kill a frame after this actor does (their subsystem ticks after the
	// actors), so it stays where it was drawn for two frames before it hides; not a blink out and back.
	if (bHideWhenInactive && (bWreckSeen || GFrameCounter - InactiveSinceFrame > 2))
	{
		SetActorHiddenInGame(true);
	}
	// Its wreck has come and gone, or none came in 5 s of sim time (no effects): nothing more to draw, ever (an aircraft
	// destroyed stays destroyed in its world), so it stops ticking. Sim time: a paused game still waits for the wreck.
	if (bWreckSeen || Sim.GetSimTimeSeconds() - InactiveSinceSimSeconds > 5.0)
	{
		SetActorTickEnabled(false);
	}
}

void ASimEntityActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* MyWorld = GetWorld();
	const USimWorldSubsystem* Sim = MyWorld ? MyWorld->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr)
	{
		return;
	}
	// The frame's one snapshot (no copy); an id it has never listed falls back to GetEntityState's not-alive answer.
	const FSimEntityState* State = Sim->FindEntityStateCached(EntityId);
	FSimEntityState Unlisted;
	if (State == nullptr)
	{
		if (!Sim->GetEntityState(EntityId, Unlisted))
		{
			// No longer in the simulation at all (the testbed's waves take the long gone out of the world): hidden.
			if (bHideWhenInactive && !IsHidden())
			{
				SetActorHiddenInGame(true);
			}
			return;
		}
		State = &Unlisted;
	}

	if (State->bAlive)
	{
		if (bReportedInactive)
		{
			// Back in the list: a mover come into sight again, a visitor come back.
			bReportedInactive = false;
			SetActorHiddenInGame(false);
		}
#if SIM_ENTITY_HAS_FX_SUBSYSTEM
		// Down in the effects while the simulation still lists it (their kill event come a step early, or the effects'
		// photographer's shoot-down of a live aircraft): its wreck is drawn, not the aircraft flying on.
		if (!State->bIsMissile && !State->bIsGround)
		{
			const USimFxSubsystem* Fx = MyWorld->GetSubsystem<USimFxSubsystem>();
			FVector WreckAt;
			FRotator WreckRot;
			bool bWreckVisible = false;
			if (Fx != nullptr && Fx->GetWreckPose(EntityId, WreckAt, WreckRot, bWreckVisible))
			{
				FollowWreck(*Sim);
				return;
			}
		}
#endif
		// Drawn at the render time, between sim steps: carried on by its velocity (ground vehicles stand still in a step).
		FVector At = Sim->GetRenderLocation(*State);
		if (State->bIsGround)
		{
			At = GroundBelow(At, *Sim);
		}
		SetActorLocationAndRotation(At, State->Rotation);
		if (Mast != nullptr)
		{
			// a battery's post raises its mast and sets the AB-1 out when it deploys, at the mast's own pace
			Mast->SetTargetExtension(State->bDeployed ? 1.f : 0.f);
			Mast->SetGeneratorOut(State->bDeployed);
		}
		return;
	}
	const bool bAircraft = !State->bIsMissile && !State->bIsGround;
	if (!bReportedInactive)
	{
		bReportedInactive = true;
		InactiveSinceSimSeconds = Sim->GetSimTimeSeconds();
		InactiveSinceFrame = GFrameCounter;
		// Hidden where it was last drawn: a finished missile where it burst, not at the origin. A destroyed aircraft is
		// left to its wreck (FollowWreck).
		if (bHideWhenInactive && !bAircraft)
		{
			SetActorHiddenInGame(true);
		}
		OnEntityInactive();
		if (State->bIsMissile && GetClass() == ASimEntityActor::StaticClass())
		{
			SetActorTickEnabled(false);  // a missile's number never flies again
			return;
		}
	}
	if (bAircraft)
	{
		FollowWreck(*Sim);  // shown again, darkened, while its wreck falls
	}
}
