#include "SimPositionWork.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Colours: dug earth, a faded olive net, grey-brown poles (SAM3D's).
	const FLinearColor EarthColor(0.16f, 0.12f, 0.075f);
	const FLinearColor NetColor(0.07f, 0.085f, 0.04f);
	const FLinearColor PoleColor(0.12f, 0.1f, 0.08f);
	constexpr double ParapetHeightM = 1.6;   // the parapet's crest above the ground
	constexpr double ParapetBaseM = 3.0;     // its base, across
	constexpr double ClearanceM = 1.5;       // room round the vehicle inside the emplacement
	constexpr double NetHeightM = 4.2;       // the net's poles: over the turret and the search antenna stowed
	constexpr double NetSagM = 0.6;          // how far it sags between the poles
}

ASimPositionWork::ASimPositionWork()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Cube = CubeMesh.Object;
	}
}

UStaticMeshComponent* ASimPositionWork::AddBox(const FVector& SizeM, const FVector& CentreM, const FRotator& Rotation,
	const FLinearColor& Color, float Roughness, TArray<TObjectPtr<UStaticMeshComponent>>& Into)
{
	UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
	Box->SetupAttachment(Root);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube != nullptr)
	{
		Box->SetStaticMesh(Cube);  // 100 cm a side, centred
	}
	Box->RegisterComponent();
	Box->SetRelativeLocationAndRotation(CentreM * 100.0, Rotation);
	Box->SetRelativeScale3D(SizeM);
	if (Surface != nullptr)
	{
		UMaterialInstanceDynamic* M = Box->CreateDynamicMaterialInstance(0, Surface);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	}
	Into.Add(Box);
	return Box;
}

void ASimPositionWork::Build(bool bEmplacement, bool bNets, double LengthM, double WidthM)
{
	if (Surface == nullptr)
	{
		Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Cabin/Materials/M_CabinSurface.M_CabinSurface"), nullptr,
			LOAD_NoWarn | LOAD_Quiet);
	}
	const double HalfL = 0.5 * LengthM + ClearanceM;
	const double HalfW = 0.5 * WidthM + ClearanceM;
	if (bEmplacement)
	{
		// The parapet: a bank of earth along each side and across the front (+X, the vehicle's front), sloped on both
		// faces (two boxes leaning together), the back open. Below ground the pit is implied: the terrain is the sim's.
		const double Slope = FMath::RadiansToDegrees(FMath::Atan2(ParapetHeightM, 0.5 * ParapetBaseM));
		const double Face = FMath::Sqrt(ParapetHeightM * ParapetHeightM + 0.25 * ParapetBaseM * ParapetBaseM);
		auto Bank = [&](const FVector& Centre, double Yaw, double Length)
		{
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				// Each face: a slab tilted about the bank's long axis, its foot on the ground.
				const FVector Out = FRotator(0.0, Yaw + 90.0, 0.0).Vector() * (Side * 0.25 * ParapetBaseM);
				AddBox(FVector(Length, Face, 0.3), Centre + Out + FVector(0.0, 0.0, 0.5 * ParapetHeightM),
					FRotator(0.0, Yaw, Side * Slope), EarthColor, 0.95f, Earth);
			}
		};
		Bank(FVector(0.0, HalfW + 0.5 * ParapetBaseM, 0.0), 0.0, 2.0 * HalfL + ParapetBaseM);
		Bank(FVector(0.0, -HalfW - 0.5 * ParapetBaseM, 0.0), 0.0, 2.0 * HalfL + ParapetBaseM);
		Bank(FVector(HalfL + 0.5 * ParapetBaseM, 0.0, 0.0), 90.0, 2.0 * HalfW);
	}
	if (bNets)
	{
		// Four poles at the corners and a net between them, sagging to the middle; both windings, so it shows from
		// above and below.
		for (int32 I = 0; I < 4; ++I)
		{
			const double X = (I & 1) ? HalfL : -HalfL;
			const double Y = (I & 2) ? HalfW : -HalfW;
			AddBox(FVector(0.08, 0.08, NetHeightM), FVector(X, Y, 0.5 * NetHeightM), FRotator::ZeroRotator, PoleColor, 0.9f, Poles);
		}
		Net = NewObject<UProceduralMeshComponent>(this);
		Net->SetupAttachment(Root);
		Net->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Net->SetCastShadow(true);
		Net->RegisterComponent();
		constexpr int32 N = 12;
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector2D> Uv;
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I <= N; ++I)
			{
				const double U = static_cast<double>(I) / N;
				const double W = static_cast<double>(J) / N;
				const double Sag = NetSagM * FMath::Sin(UE_DOUBLE_PI * U) * FMath::Sin(UE_DOUBLE_PI * W);
				// Ragged edges: the net hangs down past the poles a little.
				const double Edge = (I == 0 || I == N || J == 0 || J == N) ? 0.9 : 0.0;
				V.Add(FVector((-HalfL - 0.6 + U * (2.0 * HalfL + 1.2)) * 100.0, (-HalfW - 0.6 + W * (2.0 * HalfW + 1.2)) * 100.0,
					(NetHeightM - Sag - Edge) * 100.0));
				Uv.Add(FVector2D(U, W));
			}
		}
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const int32 A = J * (N + 1) + I;
				const int32 B = A + 1;
				const int32 C = A + (N + 1);
				const int32 D = C + 1;
				T.Append({A, C, B, B, C, D, A, B, C, B, D, C});
			}
		}
		Net->CreateMeshSection_LinearColor(0, V, T, {}, Uv, {}, {}, false);
		if (Surface != nullptr)
		{
			UMaterialInstanceDynamic* M = Net->CreateDynamicMaterialInstance(0, Surface);
			M->SetVectorParameterValue(TEXT("Color"), NetColor);
			M->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
		}
	}
	SetNets(bNets);
}

void ASimPositionWork::SetNets(bool bOn)
{
	bNetsShown = bOn && Net != nullptr;
	if (Net != nullptr)
	{
		Net->SetVisibility(bOn);
	}
	for (UStaticMeshComponent* P : Poles)
	{
		if (P != nullptr)
		{
			P->SetVisibility(bOn);
		}
	}
}
