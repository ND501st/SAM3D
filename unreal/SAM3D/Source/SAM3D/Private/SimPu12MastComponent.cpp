#include "SimPu12MastComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimCoreIncludes.h"

namespace
{
	UStaticMesh* LoadVehicleMesh(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SAM3D/Vehicles/%s.%s"), *Name, *Name));
	}

	float Num(const simcore::json::Value* V, const char* Key, float Fallback)
	{
		const simcore::json::Value* N = V != nullptr ? V->Find(Key) : nullptr;
		return N != nullptr && N->Is(simcore::json::Type::Number) ? static_cast<float>(N->number) : Fallback;
	}

	FString Str(const simcore::json::Value* V, const char* Key, const FString& Fallback)
	{
		const simcore::json::Value* S = V != nullptr ? V->Find(Key) : nullptr;
		return S != nullptr && S->Is(simcore::json::Type::String) ? FString(UTF8_TO_TCHAR(S->string.c_str())) : Fallback;
	}
}

USimPu12MastComponent::USimPu12MastComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// The built-in copy of tools/blender/pu12m_trace.py's numbers (cm), used when the rig file can't be read: a fixed
	// section and eight moving ones.
	for (int32 I = 0; I < 9; ++I)
	{
		FSection S;
		S.Radius = 7.5f - 4.5f * static_cast<float>(I) / 8.f;
		S.Length = I == 0 ? 260.5f : 260.f;
		S.Mesh = I == 0 ? FString() : FString::Printf(TEXT("SM_PU12M_Mast%d"), I);
		Sections.Add(S);
	}
	GuyLevels = {0.5f, 0.95f};
}

void USimPu12MastComponent::OnRegister()
{
	Super::OnRegister();
	if (!bBuilt)
	{
		LoadRig();
		Build();
		bBuilt = true;
	}
	Place();
}

void USimPu12MastComponent::LoadRig()
{
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../art/pu12m/pu12m_rig.json")));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("PU-12M mast: no rig at %s; using the built-in copy of the trace"), *Path);
		return;
	}
	simcore::json::Value Root;
	std::string Error;
	if (!simcore::json::Parse(std::string(TCHAR_TO_UTF8(*Text)), Root, &Error))
	{
		UE_LOG(LogSAM3D, Warning, TEXT("PU-12M mast: %s: %s"), *Path, UTF8_TO_TCHAR(Error.c_str()));
		return;
	}
	const simcore::json::Value* Mast = Root.Find("mast");
	MastX = Num(Mast, "x", MastX);
	MastY = Num(Mast, "y", MastY);
	BaseTopZ = Num(Mast, "base_top_z", BaseTopZ);
	StowedTopZ = Num(Mast, "stowed_top_z", StowedTopZ);
	RaisedTopZ = Num(Mast, "raised_top_z", RaisedTopZ);
	NestStep = Num(Mast, "nest_step", NestStep);
	if (const simcore::json::Value* List = Mast != nullptr ? Mast->Find("sections") : nullptr)
	{
		if (List->Is(simcore::json::Type::Array) && List->items.size() >= 2)
		{
			Sections.Reset();
			for (const simcore::json::Value& V : List->items)
			{
				FSection S;
				S.Radius = Num(&V, "radius", 5.f);
				S.Length = Num(&V, "length", 290.f);
				S.Mesh = Str(&V, "mesh", FString());
				Sections.Add(S);
			}
		}
	}
	const simcore::json::Value* Guys = Root.Find("guys");
	GuyRadius = Num(Guys, "radius", GuyRadius);
	GuyMesh = Str(Guys, "mesh", GuyMesh);
	if (const simcore::json::Value* Levels = Guys != nullptr ? Guys->Find("levels") : nullptr)
	{
		GuyLevels.Reset();
		for (const simcore::json::Value& V : Levels->items)
		{
			GuyLevels.Add(static_cast<float>(V.number));
		}
	}
	const simcore::json::Value* Ab1 = Root.Find("ab1");
	Ab1X = Num(Ab1, "x", Ab1X);
	Ab1Y = Num(Ab1, "y", Ab1Y);
	Ab1MeshName = Str(Ab1, "mesh", Ab1MeshName);
	bRigLoaded = true;
}

void USimPu12MastComponent::Build()
{
	AActor* Owner = GetOwner();
	auto MakeMesh = [this, Owner](const FString& MeshName, const TCHAR* Label) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner != nullptr ? static_cast<UObject*>(Owner) : this, Label);
		C->SetupAttachment(this);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(LoadVehicleMesh(MeshName));
		C->RegisterComponent();
		return C;
	};
	for (int32 I = 1; I < Sections.Num(); ++I)
	{
		SectionMeshes.Add(MakeMesh(Sections[I].Mesh, *FString::Printf(TEXT("MastSection%d"), I)));
	}
	for (int32 I = 0; I < GuyLevels.Num() * 4; ++I)
	{
		GuyMeshes.Add(MakeMesh(GuyMesh, *FString::Printf(TEXT("MastGuy%d"), I)));
	}
	Ab1Mesh = MakeMesh(Ab1MeshName, TEXT("AB1"));
}

float USimPu12MastComponent::SectionTopZ(int32 Section) const
{
	if (Section <= 0 || Sections.Num() < 2)
	{
		return BaseTopZ;
	}
	// As tools/blender/pu12m_trace.py mast_section_tops: each moving section rises an equal share of the travel.
	const int32 Moving = Sections.Num() - 1;
	const float Travel = (RaisedTopZ - StowedTopZ) / static_cast<float>(Moving);
	const float StowedTop = StowedTopZ - static_cast<float>(Moving - Section) * NestStep;
	return StowedTop + FMath::Clamp(Extension, 0.f, 1.f) * Travel * static_cast<float>(Section);
}

bool USimPu12MastComponent::AreGuysShown() const
{
	return Extension >= 0.98f;
}

void USimPu12MastComponent::Place()
{
	for (int32 I = 0; I < SectionMeshes.Num(); ++I)
	{
		if (SectionMeshes[I] != nullptr)
		{
			SectionMeshes[I]->SetRelativeLocation(FVector(MastX, MastY, SectionTopZ(I + 1)));  // each mesh's origin is its top
		}
	}
	const bool bGuys = AreGuysShown();
	for (int32 L = 0; L < GuyLevels.Num(); ++L)
	{
		const FVector Top(MastX, MastY, GuyLevels[L] * RaisedTopZ);
		for (int32 K = 0; K < 4; ++K)
		{
			UStaticMeshComponent* G = GuyMeshes.IsValidIndex(L * 4 + K) ? GuyMeshes[L * 4 + K].Get() : nullptr;
			if (G == nullptr)
			{
				continue;
			}
			const float A = PI / 4.f + static_cast<float>(K) * PI / 2.f;
			const FVector Stake(MastX + GuyRadius * FMath::Cos(A), MastY + GuyRadius * FMath::Sin(A), 0.f);
			const FVector Along = Top - Stake;
			// the guy mesh is 1 m along +Z from its origin: stand it at the stake, point it at the mast, stretch it
			G->SetRelativeLocation(Stake);
			G->SetRelativeRotation(FRotationMatrix::MakeFromZ(Along.GetSafeNormal()).Rotator());
			G->SetRelativeScale3D(FVector(1.f, 1.f, Along.Size() / 100.f));
			G->SetVisibility(bGuys);
		}
	}
	if (Ab1Mesh != nullptr)
	{
		Ab1Mesh->SetRelativeLocation(FVector(Ab1X, Ab1Y, 0.f));
		Ab1Mesh->SetVisibility(bGeneratorOut);
	}
}

void USimPu12MastComponent::SetExtension(float NewExtension)
{
	Extension = FMath::Clamp(NewExtension, 0.f, 1.f);
	Target = Extension;
	bMoving = false;
	Place();
}

void USimPu12MastComponent::SetTargetExtension(float NewTarget)
{
	Target = FMath::Clamp(NewTarget, 0.f, 1.f);
	bMoving = !FMath::IsNearlyEqual(Target, Extension);
}

void USimPu12MastComponent::SetGeneratorOut(bool bOut)
{
	if (bGeneratorOut != bOut)
	{
		bGeneratorOut = bOut;
		Place();
	}
}

void USimPu12MastComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bMoving)
	{
		return;
	}
	const float Rate = Target > Extension ? 1.f / FMath::Max(RaiseSeconds, 1.f) : 1.f / FMath::Max(LowerSeconds, 1.f);
	const float Step = Rate * DeltaTime;
	Extension = FMath::Abs(Target - Extension) <= Step ? Target : Extension + (Target > Extension ? Step : -Step);
	bMoving = !FMath::IsNearlyEqual(Target, Extension);
	Place();
}
