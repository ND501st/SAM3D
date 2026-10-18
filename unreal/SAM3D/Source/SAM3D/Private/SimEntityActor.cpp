#include "SimEntityActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "SimBasicModels.h"
#include "SimPu12MastComponent.h"
#include "SimWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

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

void ASimEntityActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* MyWorld = GetWorld();
	const USimWorldSubsystem* Sim = MyWorld ? MyWorld->GetSubsystem<USimWorldSubsystem>() : nullptr;
	FSimEntityState State;
	if (Sim == nullptr || !Sim->GetEntityState(EntityId, State))
	{
		return;
	}

	if (State.bAlive)
	{
		SetActorLocationAndRotation(State.Location, State.Rotation);
		if (Mast != nullptr)
		{
			// a battery's post raises its mast and sets the AB-1 out when it deploys, at the mast's own pace
			Mast->SetTargetExtension(State.bDeployed ? 1.f : 0.f);
			Mast->SetGeneratorOut(State.bDeployed);
		}
	}
	else if (!bReportedInactive)
	{
		bReportedInactive = true;
		SetActorLocationAndRotation(State.Location, State.Rotation);
		if (bHideWhenInactive)
		{
			SetActorHiddenInGame(true);
		}
		OnEntityInactive();
	}
}
