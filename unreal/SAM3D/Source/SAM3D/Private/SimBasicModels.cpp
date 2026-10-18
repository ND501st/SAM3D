#include "SimBasicModels.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
#include "UObject/Package.h"

namespace
{
	TUniquePtr<simcore::scenery::ModelLibrary> GLibrary;
	bool bTriedLibrary = false;
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> GColours;
	TObjectPtr<UStaticMesh> GShapes[4] = {nullptr, nullptr, nullptr, nullptr};
}

const simcore::scenery::ModelLibrary* FSimBasicModels::Library()
{
	if (!bTriedLibrary)
	{
		bTriedLibrary = true;
		TUniquePtr<simcore::scenery::ModelLibrary> Lib = MakeUnique<simcore::scenery::ModelLibrary>();
		const FString Path = FPaths::Combine(USimAirCampaignSubsystem::DataDirectory(), TEXT("scenery/basic_models.json"));
		std::string Error;
		if (simcore::scenery::LoadModelLibrary(std::string(TCHAR_TO_UTF8(*Path)), *Lib, &Error))
		{
			GLibrary = MoveTemp(Lib);
			UE_LOG(LogSAM3D, Log, TEXT("Basic models: %d loaded from %s"), static_cast<int32>(GLibrary->models.size()), *Path);
		}
		else
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Basic models don't load (%s): placeholders stay cones and boxes"), UTF8_TO_TCHAR(Error.c_str()));
		}
	}
	return GLibrary.Get();
}

UStaticMesh* FSimBasicModels::ShapeMesh(simcore::scenery::Shape Shape)
{
	const int32 I = static_cast<int32>(Shape);
	if (GShapes[I] == nullptr)
	{
		const TCHAR* Paths[] = {TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("/Engine/BasicShapes/Sphere.Sphere")};
		GShapes[I] = LoadObject<UStaticMesh>(nullptr, Paths[I]);
		if (GShapes[I] != nullptr)
		{
			GShapes[I]->AddToRoot();
		}
	}
	return GShapes[I];
}

UMaterialInterface* FSimBasicModels::ColourMaterial(const FLinearColor& Colour)
{
	return ColourMaterial(TEXT("#") + Colour.ToFColorSRGB().ToHex().Left(6));
}

UMaterialInterface* FSimBasicModels::ColourMaterial(const FString& Hex)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = GColours.Find(Hex))
	{
		return Found->Get();
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Base == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	const FColor C = FColor::FromHex(Hex);
	Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(C));
	Mid->AddToRoot();
	GColours.Add(Hex, Mid);
	return Mid;
}

FTransform FSimBasicModels::PartTransform(const simcore::scenery::Part& Part, const UStaticMesh* Mesh)
{
	using simcore::scenery::Shape;
	// Cylinders and cones run along their local Z: turn Z onto the part's axis first.
	FQuat Axis = FQuat::Identity;
	if (Part.shape == Shape::Cylinder || Part.shape == Shape::Cone)
	{
		if (Part.axis == 'x')
		{
			Axis = FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector);
		}
		else if (Part.axis == 'y')
		{
			Axis = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector);
		}
	}
	const FQuat Tilt = FRotator(Part.pitch_deg, Part.yaw_deg, Part.roll_deg).Quaternion();
	const FQuat Rotation = Tilt * Axis;
	// The basic shapes are 1 m across: the scale is the size in metres (a cylinder's or cone's Z its length).
	const FVector Scale(Part.size.x, Part.size.y, Part.size.z);
	// Centre the shape on the part's centre whatever the mesh's pivot.
	const FVector MeshCentre = Mesh != nullptr ? Mesh->GetBounds().Origin : FVector::ZeroVector;
	const FVector At(Part.at.x * 100.0, Part.at.y * 100.0, Part.at.z * 100.0);
	const FVector Location = At - Rotation.RotateVector(MeshCentre * Scale);
	return FTransform(Rotation, Location, Scale);
}

int32 FSimBasicModels::Build(const FString& ModelId, USceneComponent* Parent, TArray<UStaticMeshComponent*>* OutParts,
	const FLinearColor* Override)
{
	const simcore::scenery::ModelLibrary* Lib = Library();
	if (Lib == nullptr || Parent == nullptr)
	{
		return 0;
	}
	const simcore::scenery::ModelDef* Model = Lib->Find(std::string(TCHAR_TO_UTF8(*ModelId)));
	if (Model == nullptr)
	{
		return 0;
	}
	AActor* Owner = Parent->GetOwner();
	int32 Built = 0;
	for (const simcore::scenery::Part& Part : Model->parts)
	{
		UStaticMesh* Mesh = ShapeMesh(Part.shape);
		if (Mesh == nullptr)
		{
			continue;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner != nullptr ? static_cast<UObject*>(Owner) : static_cast<UObject*>(Parent));
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		C->SetupAttachment(Parent);
		C->SetRelativeTransform(PartTransform(Part, Mesh));
		UMaterialInterface* Mat = Override != nullptr ? ColourMaterial(*Override) : ColourMaterial(UTF8_TO_TCHAR(Part.colour.c_str()));
		if (Mat != nullptr)
		{
			C->SetMaterial(0, Mat);
		}
		C->RegisterComponent();
		if (OutParts != nullptr)
		{
			OutParts->Add(C);
		}
		++Built;
	}
	return Built;
}
