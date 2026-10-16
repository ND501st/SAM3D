#include "SimEntityActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
