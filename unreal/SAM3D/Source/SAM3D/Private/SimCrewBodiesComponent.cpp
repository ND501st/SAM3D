#include "SimCrewBodiesComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SAM3D.h"
#include "SimCabinComponent.h"
#include "SimWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	// Body frame (x right, y forward, z up; m) to the cabin component's frame (X forward, Y right, Z up; cm).
	FVector BodyPoint(const simcore::Vec3& V) { return FVector(V.y, V.x, V.z) * 100.0; }
	FVector BodyDir(const simcore::Vec3& V) { return FVector(V.y, V.x, V.z); }

	// The crew's colours by material slot (tools/blender/build_crew_body.py), and the basic shapes' by part.
	FLinearColor SlotColor(const FName& Slot)
	{
		const FString S = Slot.ToString();
		if (S.Contains(TEXT("Skin"))) return FLinearColor(0.62f, 0.45f, 0.36f);
		if (S.Contains(TEXT("Helmet"))) return FLinearColor(0.03f, 0.03f, 0.03f);
		if (S.Contains(TEXT("Boot"))) return FLinearColor(0.02f, 0.02f, 0.02f);
		if (S.Contains(TEXT("Belt"))) return FLinearColor(0.25f, 0.14f, 0.07f);
		return FLinearColor(0.20f, 0.22f, 0.13f);  // the olive-khaki uniform
	}

	FLinearColor PartColor(const std::string& Name)
	{
		if (Name == "head") return FLinearColor(0.03f, 0.03f, 0.03f);
		if (Name.rfind("hand", 0) == 0 || Name == "neck") return FLinearColor(0.62f, 0.45f, 0.36f);
		if (Name.rfind("foot", 0) == 0 || Name.rfind("shin", 0) == 0) return FLinearColor(0.02f, 0.02f, 0.02f);
		return FLinearColor(0.20f, 0.22f, 0.13f);
	}

	// A basic shape's radius standing in for a part (m).
	double PartRadius(const std::string& Name)
	{
		if (Name == "pelvis" || Name == "torso") return 0.16;
		if (Name == "head") return 0.11;
		if (Name.rfind("upper_arm", 0) == 0 || Name.rfind("thigh", 0) == 0) return 0.06;
		if (Name.rfind("hand", 0) == 0) return 0.03;
		return 0.045;
	}
}

USimCrewBodiesComponent::USimCrewBodiesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;  // after the world subsystem has stepped the sim
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	SphereMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	ShapeMaterial = Material.Succeeded() ? Material.Object : nullptr;
}

void USimCrewBodiesComponent::SetGhostHand(const FString& Station, const FString& Control)
{
	GhostStation = Station;
	GhostControl = Control;
}

bool USimCrewBodiesComponent::GetBodyHead(const FString& Station, FVector& OutLocation, bool& bOutVisible) const
{
	for (const FBody& B : Bodies)
	{
		if (B.Station == Station && B.Animator)
		{
			OutLocation = BodyPoint(B.Animator->Pose().head);
			bOutVisible = B.bVisible;
			return true;
		}
	}
	return false;
}

UStaticMesh* USimCrewBodiesComponent::LibraryMesh(const FName& Name)
{
	if (const TObjectPtr<UStaticMesh>* Found = Meshes.Find(Name))
	{
		return Found->Get();
	}
	const FString S = Name.ToString();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SAM3D/Crew/%s.%s"), *S, *S), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
	Meshes.Add(Name, Mesh);
	return Mesh;
}

FName USimCrewBodiesComponent::MeshNameFor(const simcore::BodyPart& P) const
{
	const std::string& N = P.name;
	auto Starts = [&N](const char* Prefix) { return N.rfind(Prefix, 0) == 0; };
	if (Starts("hand"))
	{
		const TCHAR* Shape = P.hand == simcore::HandShape::Point ? TEXT("Point") : P.hand == simcore::HandShape::Grip ? TEXT("Grip") : TEXT("Rest");
		return FName(*FString::Printf(TEXT("SM_Crew_Hand%s_%s"), Shape, N.back() == 'l' ? TEXT("L") : TEXT("R")));
	}
	if (N == "pelvis") return FName(TEXT("SM_Crew_Pelvis"));
	if (N == "torso") return FName(TEXT("SM_Crew_Torso"));
	if (N == "neck") return FName(TEXT("SM_Crew_Neck"));
	if (N == "head") return FName(TEXT("SM_Crew_Head"));
	if (Starts("upper_arm")) return FName(TEXT("SM_Crew_UpperArm"));
	if (Starts("forearm")) return FName(TEXT("SM_Crew_Forearm"));
	if (Starts("thigh")) return FName(TEXT("SM_Crew_Thigh"));
	if (Starts("shin")) return FName(TEXT("SM_Crew_Shin"));
	return FName(TEXT("SM_Crew_Foot"));
}

void USimCrewBodiesComponent::PaintPart(UStaticMeshComponent* Mesh, bool bFallback, const FLinearColor& FallbackColor)
{
	if (bFallback)
	{
		if (ShapeMaterial != nullptr)
		{
			UMaterialInstanceDynamic* Mid = Mesh->CreateDynamicMaterialInstance(0, ShapeMaterial);
			Mid->SetVectorParameterValue(TEXT("Color"), FallbackColor);
		}
		return;
	}
	if (SurfaceMaterial == nullptr)
	{
		return;  // the imported materials as they are
	}
	const TArray<FName> Slots = Mesh->GetMaterialSlotNames();
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		UMaterialInstanceDynamic* Mid = Mesh->CreateDynamicMaterialInstance(I, SurfaceMaterial);
		Mid->SetVectorParameterValue(TEXT("Color"), SlotColor(Slots[I]));
		Mid->SetScalarParameterValue(TEXT("Roughness"), Slots[I].ToString().Contains(TEXT("Boot")) ? 0.45f : 0.8f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
	}
}

bool USimCrewBodiesComponent::Build(USimWorldSubsystem* Sim)
{
	Clear();
	const simcore::CabinLayout* Layout = Sim->GetCabinLayout();
	const simcore::BodyData* BodyData = Sim->GetBodyData();
	const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
	if (Layout == nullptr || BodyData == nullptr || Runner == nullptr)
	{
		return false;
	}
	if (SurfaceMaterial == nullptr)
	{
		SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Cabin/Materials/M_CabinSurface.M_CabinSurface"), nullptr,
			LOAD_NoWarn | LOAD_Quiet);
	}
	bool bAnyFallback = false;
	for (const simcore::SeatActivity& A : Runner->CrewActivity())
	{
		FBody Body;
		Body.Station = ToF(A.station);
		Body.Animator = MakeUnique<simcore::BodyAnimator>(Layout, BodyData, A.station);
		if (!Body.Animator->Valid())
		{
			continue;  // a crew member with no seat in the cabin
		}
		for (const simcore::BodyPart& P : Body.Animator->Parts())
		{
			FPart Part;
			Part.MeshName = MeshNameFor(P);
			UStaticMesh* Mesh = LibraryMesh(Part.MeshName);
			Part.bFallback = Mesh == nullptr;
			Part.RadiusM = PartRadius(P.name);
			bAnyFallback = bAnyFallback || Part.bFallback;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(GetOwner());
			C->SetStaticMesh(Part.bFallback ? (P.name == "head" ? SphereMesh.Get() : CylinderMesh.Get()) : Mesh);
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(this);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			USimCabinComponent::UseCabinLighting(C);
			C->RegisterComponent();
			PaintPart(C, Part.bFallback, PartColor(P.name));
			Part.Mesh = C;
			AllParts.Add(C);
			Body.Parts.Add(Part);
		}
		Bodies.Add(MoveTemp(Body));
	}
	if (bAnyFallback)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Crew bodies: some meshes aren't imported (tools/ue_import_crew_body.py); basic shapes stand in"));
	}
	BuiltLayout = Layout;
	BuiltRunner = Runner;
	UE_LOG(LogSAM3D, Log, TEXT("Crew bodies: %d built"), Bodies.Num());
	return Bodies.Num() > 0;
}

void USimCrewBodiesComponent::Clear()
{
	for (UStaticMeshComponent* C : AllParts)
	{
		if (C != nullptr)
		{
			C->DestroyComponent();
		}
	}
	AllParts.Reset();
	Bodies.Reset();
	BuiltLayout = nullptr;
	BuiltRunner = nullptr;
}

void USimCrewBodiesComponent::ShowPart(FPart& Part, const simcore::BodyPart& P)
{
	UStaticMeshComponent* C = Part.Mesh;
	if (C == nullptr)
	{
		return;
	}
	const FVector Bone = BodyDir(P.bone);
	const FVector Up = BodyDir(P.up);
	if (Part.bFallback)
	{
		// A basic shape along the bone: the cylinder is 1 m tall along its Z, centred; the sphere 1 m across.
		const double D = 2.0 * Part.RadiusM;
		const bool bSphere = P.name == "head";
		const FVector Centre = BodyPoint(P.origin) + Bone * (bSphere ? 8.0 : P.length_m * 50.0);
		C->SetRelativeLocationAndRotation(Centre, FRotationMatrix::MakeFromZX(Bone, Up).Rotator());
		C->SetRelativeScale3D(bSphere ? FVector(D) : FVector(D, D, FMath::Max(P.length_m, 0.02)));
		return;
	}
	const FName Want = MeshNameFor(P);
	if (Want != Part.MeshName)  // a hand changing its shape
	{
		if (UStaticMesh* Mesh = LibraryMesh(Want))
		{
			C->SetStaticMesh(Mesh);
			PaintPart(C, false, FLinearColor::Black);
		}
		Part.MeshName = Want;
	}
	C->SetRelativeLocationAndRotation(BodyPoint(P.origin), FRotationMatrix::MakeFromXZ(Bone, Up).Rotator());
}

void USimCrewBodiesComponent::PoseBody(FBody& Body)
{
	const std::vector<simcore::BodyPart> Parts = Body.Animator->Parts();
	for (int32 I = 0; I < Body.Parts.Num() && I < static_cast<int32>(Parts.size()); ++I)
	{
		ShowPart(Body.Parts[I], Parts[static_cast<size_t>(I)]);
	}
}

void USimCrewBodiesComponent::UpdateGhost(float DeltaTime)
{
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	const simcore::BodyData* BodyData = Sim != nullptr ? Sim->GetBodyData() : nullptr;
	simcore::ControlTouch Touch;
	const simcore::CabinStation* St = Layout != nullptr ? Layout->FindStation(ToStd(GhostStation)) : nullptr;
	if (GhostControl.IsEmpty() || St == nullptr || BodyData == nullptr || !simcore::TouchOf(*Layout, ToStd(GhostControl), Touch))
	{
		if (Ghost != nullptr)
		{
			Ghost->SetVisibility(false);
		}
		return;
	}
	if (Ghost == nullptr)
	{
		Ghost = NewObject<UStaticMeshComponent>(GetOwner());
		Ghost->SetMobility(EComponentMobility::Movable);
		Ghost->SetupAttachment(this);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		USimCabinComponent::UseCabinLighting(Ghost);
		Ghost->RegisterComponent();
	}
	// The hand from the seat's right shoulder, as the drill master's move would be made, pulsing green.
	const simcore::SeatFrame Seat = simcore::SeatFrameOf(*St, BodyData->dims);
	const FString PlanKey = GhostStation + TEXT("|") + GhostControl;
	if (PlanKey != GhostPlanKey || Layout != GhostPlanLayout)
	{
		GhostPlan = simcore::PlanReach(Seat, BodyData->dims, Touch, Layout);
		GhostPlanKey = PlanKey;
		GhostPlanLayout = Layout;
	}
	const simcore::ReachPlan& Plan = GhostPlan;
	const simcore::TorsoFrame Torso = simcore::TorsoFrameOf(Seat.hip, simcore::PelvisForward(Seat.forward, Plan.lean),
		simcore::TorsoLean{0.0, Plan.lean.forward_rad, Plan.lean.side_rad, Plan.lean.twist_rad, 0.0}, BodyData->dims);
	const simcore::WristTarget W =
		simcore::WristFor(Touch.point, Touch.normal, Touch.shape, Torso.Shoulder(Plan.side, BodyData->dims), BodyData->dims);
	const FName Name(*FString::Printf(TEXT("SM_Crew_Hand%s_%s"), Touch.shape == simcore::HandShape::Grip ? TEXT("Grip") : TEXT("Point"),
		Plan.side < 0 ? TEXT("L") : TEXT("R")));
	UStaticMesh* Mesh = LibraryMesh(Name);
	if (Mesh == nullptr)
	{
		Mesh = SphereMesh;
	}
	if (Ghost->GetStaticMesh() != Mesh)
	{
		Ghost->SetStaticMesh(Mesh);
		if (ShapeMaterial != nullptr)
		{
			for (int32 I = 0; I < FMath::Max(1, Ghost->GetNumMaterials()); ++I)
			{
				UMaterialInstanceDynamic* Mid = Ghost->CreateDynamicMaterialInstance(I, ShapeMaterial);
				Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.9f, 0.2f));
			}
		}
	}
	GhostClock += DeltaTime;
	// It comes in towards the control and fades back, once a second.
	const double Phase = FMath::Fmod(static_cast<double>(GhostClock), 1.0);
	const simcore::Vec3 Back = Touch.normal * (0.06 * (1.0 - Phase));
	Ghost->SetRelativeLocationAndRotation(BodyPoint(W.wrist + Back), FRotationMatrix::MakeFromXZ(BodyDir(W.dir), BodyDir(W.up)).Rotator());
	Ghost->SetRelativeScale3D(Mesh == SphereMesh ? FVector(0.05) : FVector(1.0));
	Ghost->SetVisibility(Phase < 0.8);
}

void USimCrewBodiesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim == nullptr || !Sim->IsScenarioRunning() || Sim->GetCabinLayout() == nullptr || Sim->GetBodyData() == nullptr)
	{
		if (Bodies.Num() > 0)
		{
			Clear();
		}
		return;
	}
	if (Sim->GetCabinLayout() != BuiltLayout || Sim->GetScenarioRunner() != BuiltRunner)
	{
		Build(Sim);
	}
	UpdateGhost(DeltaTime);
	const simcore::ScenarioRunner* Runner = Sim->GetScenarioRunner();
	// The bodies move in sim time (they keep up with the crew under time compression) and stop when it is paused.
	const double Dt = Sim->IsSimPaused() ? 0.0 : static_cast<double>(DeltaTime) * Sim->GetTimeScale();
	const std::vector<simcore::SeatActivity> Activity = Runner->CrewActivity();
	for (FBody& Body : Bodies)
	{
		simcore::SeatActivity A;
		A.station = ToStd(Body.Station);
		for (const simcore::SeatActivity& X : Activity)
		{
			if (X.station == A.station)
			{
				A = X;
			}
		}
		bool bShow = Body.Station != HiddenStation;
		for (int32 I = 0; bShow && ClearRadiusCm > 0.0f && I < Body.Parts.Num(); ++I)
		{
			const UStaticMeshComponent* Mesh = Body.Parts[I].Mesh;
			bShow = Mesh == nullptr || Mesh->Bounds.GetBox().ComputeSquaredDistanceToPoint(ClearAt) >= FMath::Square(static_cast<double>(ClearRadiusCm));
		}
		if (bShow != Body.bVisible)
		{
			for (FPart& P : Body.Parts)
			{
				if (P.Mesh != nullptr)
				{
					P.Mesh->SetVisibility(bShow);
				}
			}
			Body.bVisible = bShow;
		}
		Body.Animator->Update(Dt, A, &Runner->Console().panel);
		if (bShow)
		{
			PoseBody(Body);
		}
	}
}
