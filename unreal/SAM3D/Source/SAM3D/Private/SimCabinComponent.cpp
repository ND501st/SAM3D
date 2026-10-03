#include "SimCabinComponent.h"

#include "Components/PointLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SAM3D.h"
#include "SimCabinDisplayComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	// Layout body frame (x right, y forward, z up, m) -> this component (X forward, Y right, Z up, cm).
	FVector BodyPoint(const simcore::Vec3& V) { return FVector(V.y, V.x, V.z) * 100.0; }
	FVector BodyDir(const simcore::Vec3& V) { return FVector(V.y, V.x, V.z); }

	const FLinearColor PanelColor(0.03f, 0.03f, 0.03f);  // the dark rack behind the blocks
	const FLinearColor PlateColor(0.55f, 0.54f, 0.47f);  // the blocks' cream front plates
	constexpr double PlateDepthCm = 1.0;                  // blocks stand this far proud of the rack
	const FName CabinLightTag(TEXT("SAM3DCabinLight"));
	const FLinearColor MetalColor(0.25f, 0.25f, 0.27f);
	const FLinearColor DarkColor(0.02f, 0.02f, 0.02f);
	const FLinearColor ChromeColor(0.7f, 0.7f, 0.72f);
	const FLinearColor CoverColor(0.6f, 0.03f, 0.02f);
	const FLinearColor RedDotColor(0.75f, 0.0f, 0.0f);
	const FLinearColor BrassColor(0.6f, 0.45f, 0.1f);
	const FLinearColor HighlightColor(1.0f, 0.85f, 0.2f);

	// A look colour name (simcore::CabinLook) as a linear colour for the basic-shape material.
	FLinearColor LookColor(const std::string& Name, const FLinearColor& Fallback)
	{
		if (Name == "black") return FLinearColor(0.015f, 0.015f, 0.015f);
		if (Name == "white") return FLinearColor(0.62f, 0.6f, 0.52f);
		if (Name == "grey") return FLinearColor(0.22f, 0.22f, 0.21f);
		if (Name == "chrome") return FLinearColor(0.6f, 0.6f, 0.62f);
		if (Name == "brass") return FLinearColor(0.55f, 0.38f, 0.1f);
		if (Name == "red") return FLinearColor(0.45f, 0.02f, 0.01f);
		if (Name == "green") return FLinearColor(0.02f, 0.35f, 0.04f);
		if (Name == "amber") return FLinearColor(0.6f, 0.28f, 0.02f);
		if (Name == "yellow") return FLinearColor(0.65f, 0.48f, 0.12f);
		if (Name == "orange") return FLinearColor(0.5f, 0.14f, 0.03f);
		return Fallback;
	}

	FLinearColor LampColor(const std::string& Name)
	{
		if (Name == "green") return FLinearColor(0.1f, 1.0f, 0.1f);
		if (Name == "red") return FLinearColor(1.0f, 0.05f, 0.02f);
		if (Name == "amber") return FLinearColor(1.0f, 0.55f, 0.0f);
		return FLinearColor(1.0f, 1.0f, 0.9f);
	}

	void Place(UStaticMeshComponent* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& SizeM)
	{
		// Engine basic shapes are 1 m across, so the scale is the size in metres.
		Mesh->SetRelativeLocationAndRotation(Location, Rotation);
		Mesh->SetRelativeScale3D(SizeM);
	}

	// Direction in the panel's face at Angle degrees clockwise from "up the face", as the operator sees it.
	FVector FaceDirection(const FSimCabinItem& Item, double AngleDeg)
	{
		const double A = FMath::DegreesToRadians(AngleDeg);
		return Item.Up * FMath::Cos(A) + Item.Right * FMath::Sin(A);
	}
}

USimCabinComponent::USimCabinComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Screen(
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	SphereMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	ShapeMaterial = Material.Succeeded() ? Material.Object : nullptr;
	ScreenMaterial = Screen.Succeeded() ? Screen.Object : nullptr;
}

USimWorldSubsystem* USimCabinComponent::GetSim() const
{
	UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
}

void USimCabinComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const USimWorldSubsystem* Sim = GetSim();
	if (Sim == nullptr || Sim->GetCabinLayout() == nullptr || Sim->GetScenarioRunner() == nullptr)
	{
		return;
	}
	if (BuiltLayout != Sim->GetCabinLayout())
	{
		Build(Sim);
	}
	Animate(Sim, DeltaTime);
}

// ---------------------------------------------------------------------------------------------- building

UStaticMeshComponent* USimCabinComponent::AddMesh(UStaticMesh* Mesh, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(Mesh);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetupAttachment(this);
	// Only the cabin's own traces need to hit the parts.
	Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Part->SetCollisionResponseToAllChannels(ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Part->SetCastShadow(false);
	Part->RegisterComponent();
	if (ShapeMaterial != nullptr)
	{
		Part->CreateDynamicMaterialInstance(0, ShapeMaterial);
	}
	Paint(Part, Color);
	return Part;
}

void USimCabinComponent::Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color) const
{
	if (Mesh == nullptr)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Color);                // basic-shape parts
		Mid->SetVectorParameterValue(TEXT("TintColorAndOpacity"), Color);  // glowing lamp glass
	}
}

void USimCabinComponent::DestroyParts()
{
	for (FSimCabinItem& Item : Items)
	{
		for (UStaticMeshComponent* Part : {Item.Base.Get(), Item.Moving.Get(), Item.Pointer.Get(), Item.Mark.Get()})
		{
			if (Part != nullptr)
			{
				Part->DestroyComponent();
			}
		}
	}
	for (UStaticMeshComponent* Part : PanelMeshes)
	{
		if (Part != nullptr)
		{
			Part->DestroyComponent();
		}
	}
	for (UPrimitiveComponent* Part : DecorParts)
	{
		if (Part != nullptr)
		{
			Part->DestroyComponent();
		}
	}
	DecorParts.Reset();
	for (USimCabinDisplayComponent* Display : Displays)
	{
		if (Display != nullptr)
		{
			Display->DestroyComponent();
		}
	}
	TArray<UPointLightComponent*> Lights;
	if (GetOwner() != nullptr)
	{
		GetOwner()->GetComponents(Lights);
	}
	for (UPointLightComponent* Light : Lights)
	{
		if (Light->ComponentHasTag(CabinLightTag))
		{
			Light->DestroyComponent();
		}
	}
	Items.Reset();
	PanelMeshes.Reset();
	Displays.Reset();
	HighlightedItem = INDEX_NONE;
	BuiltLayout = nullptr;
}

bool USimCabinComponent::Build(const USimWorldSubsystem* Sim)
{
	DestroyParts();
	const simcore::CabinLayout* Layout = Sim->GetCabinLayout();
	if (Layout == nullptr || CubeMesh == nullptr || CylinderMesh == nullptr || SphereMesh == nullptr)
	{
		return false;
	}
	for (const simcore::CabinPanel& Panel : Layout->panels)
	{
		const FVector N = BodyDir(Panel.Normal());
		const FVector U = BodyDir(Panel.Up());
		UStaticMeshComponent* Mesh = AddMesh(CubeMesh, PanelColor);
		// The rack: 2 cm thick, its front face a block plate's depth behind the panel's plane (the plates' faces,
		// where the controls are). A panel without blocks is one plate.
		bool bHasBlocks = false;
		for (const simcore::CabinBlock& Block : Layout->blocks)
		{
			bHasBlocks = bHasBlocks || Block.panel_id == Panel.id;
		}
		const double Back = bHasBlocks ? PlateDepthCm : 0.0;
		Place(Mesh, BodyPoint(Panel.centre_m) - N * (1.0 + Back), FRotationMatrix::MakeFromXZ(N, U).Rotator(),
			FVector(0.02, Panel.width_m, Panel.height_m));
		if (!bHasBlocks)
		{
			Paint(Mesh, PlateColor * 0.6f);
		}
		PanelMeshes.Add(Mesh);
	}
	for (const simcore::CabinBlock& Block : Layout->blocks)
	{
		BuildBlock(*Layout, Block);
	}
	for (const simcore::CabinFixture& Fixture : Layout->fixtures)
	{
		BuildFixture(*Layout, Fixture);
	}
	// A dim lamp above each seat, so the panels read when the sun is outside.
	for (const simcore::CabinStation& Station : Layout->stations)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(GetOwner());
		Light->SetupAttachment(this);
		Light->ComponentTags.Add(CabinLightTag);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(150.0f);
		Light->SetAttenuationRadius(180.0f);
		Light->SetLightColor(FLinearColor(1.0f, 0.92f, 0.8f));
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		Light->SetRelativeLocation(BodyPoint(Station.eye_m) + FVector(0.0, 0.0, 40.0));
	}
	for (int32 I = 0; I < static_cast<int32>(Layout->controls.size()); ++I)
	{
		BuildControl(Sim, I);
	}
	for (int32 I = 0; I < static_cast<int32>(Layout->lamps.size()); ++I)
	{
		BuildLamp(Sim, I);
	}
	for (int32 I = 0; I < static_cast<int32>(Layout->displays.size()); ++I)
	{
		const simcore::CabinDisplay& D = Layout->displays[I];
		const simcore::CabinPanel& Panel = *Layout->FindPanel(D.panel_id);
		USimCabinDisplayComponent* Display = NewObject<USimCabinDisplayComponent>(GetOwner());
		Display->SetupAttachment(this);
		Display->RegisterComponent();
		Display->Setup(Sim, I, BodyPoint(Panel.PointAt(D.u_m, D.v_m)), BodyDir(Panel.Normal()), BodyDir(Panel.Right()),
			BodyDir(Panel.Up()), ScreenMaterial, CubeMesh, ShapeMaterial);
		Displays.Add(Display);
	}
	BuiltLayout = Layout;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin built: %d panels, %d blocks, %d fixtures, %d items, %d displays"), PanelMeshes.Num(),
		static_cast<int32>(Layout->blocks.size()), static_cast<int32>(Layout->fixtures.size()), Items.Num(), Displays.Num());
	return true;
}

void USimCabinComponent::BuildControl(const USimWorldSubsystem* Sim, int32 LayoutIndex)
{
	const simcore::CabinLayout& Layout = *Sim->GetCabinLayout();
	const simcore::CabinControl& Control = Layout.controls[LayoutIndex];
	const simcore::CabinPanel& Panel = *Layout.FindPanel(Control.panel_id);

	FSimCabinItem Item;
	Item.ControlId = ToF(Control.control_id);
	Item.LayoutIndex = LayoutIndex;
	Item.Style = static_cast<uint8>(Control.style);
	Item.Location = BodyPoint(Panel.PointAt(Control.u_m, Control.v_m));
	Item.Normal = BodyDir(Panel.Normal());
	Item.Right = BodyDir(Panel.Right());
	Item.Up = BodyDir(Panel.Up());

	// A fixed collar or plate behind the moving part, sized per style (diameter, depth in m).
	FVector2D Collar(0.03, 0.008);
	Item.BaseColor = MetalColor;
	Item.MovingColor = ChromeColor;
	Item.LookKind = static_cast<uint8>(Control.look.kind);
	Item.LookWidthM = Control.look.width_m;
	Item.LookHeightM = Control.look.height_m;
	switch (Control.style)
	{
	case simcore::CabinStyle::Toggle:
		Item.Moving = AddMesh(CylinderMesh, Item.MovingColor);
		break;
	case simcore::CabinStyle::Rotary:
		Collar = FVector2D(0.045, 0.006);
		Item.MovingColor = DarkColor;
		Item.Moving = AddMesh(CylinderMesh, Item.MovingColor);
		Item.Pointer = AddMesh(CubeMesh, FLinearColor::White);
		break;
	case simcore::CabinStyle::Key:
		Collar = FVector2D(0.032, 0.01);
		Item.MovingColor = BrassColor;
		Item.Moving = AddMesh(CubeMesh, Item.MovingColor);
		break;
	case simcore::CabinStyle::Button:
	{
		Collar = FVector2D(0.035, 0.01);
		const bool bLaunch = Sim->GetScenarioRunner()->Console().launch_button == Control.control_id;
		Item.BaseColor = DarkColor;
		Item.MovingColor = bLaunch ? FLinearColor(0.8f, 0.02f, 0.02f) : MetalColor;
		Item.Moving = AddMesh(CylinderMesh, Item.MovingColor);
		break;
	}
	case simcore::CabinStyle::Wheel:
		Collar = FVector2D(0.04, 0.02);
		Item.BaseColor = DarkColor;
		Item.MovingColor = DarkColor;
		Item.Moving = AddMesh(CylinderMesh, Item.MovingColor);
		Item.Pointer = AddMesh(CylinderMesh, MetalColor);
		break;
	case simcore::CabinStyle::Cover:
		Item.MovingColor = CoverColor;
		Item.Moving = AddMesh(CubeMesh, Item.MovingColor);
		break;
	}
	// The layout's look: colours, and the collar scaled to the item's size (a push-button look on a switch draws as
	// a cap in a ring, see AnimateControl).
	const simcore::CabinLookKind Look = Control.look.kind;
	const bool bCapLook = Look == simcore::CabinLookKind::Button || Look == simcore::CabinLookKind::LampButton;
	if (!Control.look.color.empty())
	{
		Item.MovingColor = LookColor(Control.look.color, Item.MovingColor);
		Paint(Item.Moving, Item.MovingColor);
	}
	if (!Control.look.ring.empty() || bCapLook)
	{
		Item.BaseColor = LookColor(Control.look.ring, ChromeColor);
	}
	if (bCapLook && Control.style == simcore::CabinStyle::Toggle && Item.Moving != nullptr)
	{
		Item.Moving->SetStaticMesh(CylinderMesh);
	}
	if (Control.look.width_m > 0.0 && Control.style != simcore::CabinStyle::Wheel)
	{
		Collar.X = Control.look.width_m;
	}
	if (Control.style != simcore::CabinStyle::Cover)
	{
		Item.Base = AddMesh(CylinderMesh, Item.BaseColor);
		Place(Item.Base, Item.Location + Item.Normal * (50.0 * Collar.Y), FRotationMatrix::MakeFromZ(Item.Normal).Rotator(),
			FVector(Collar.X, Collar.X, Collar.Y));
	}
	const simcore::ControlDef* Def = Sim->GetScenarioRunner()->Console().panel.Find(Control.control_id);
	double MarkU = 0.0;
	double MarkV = 0.0;
	if (Def != nullptr && simcore::CabinLayout::RedDot(Control, *Def, MarkU, MarkV))
	{
		// A flat red dot on the panel face (m to cm), clear of the collar.
		Item.Mark = AddMesh(CylinderMesh, RedDotColor);
		Item.Mark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Place(Item.Mark, Item.Location + (Item.Right * MarkU + Item.Up * MarkV) * 100.0 + Item.Normal * 0.15,
			FRotationMatrix::MakeFromZ(Item.Normal).Rotator(), FVector(0.007, 0.007, 0.002));
	}
	Items.Add(Item);
}

void USimCabinComponent::BuildLamp(const USimWorldSubsystem* Sim, int32 LayoutIndex)
{
	const simcore::CabinLayout& Layout = *Sim->GetCabinLayout();
	const simcore::CabinLamp& Lamp = Layout.lamps[LayoutIndex];
	const simcore::CabinPanel& Panel = *Layout.FindPanel(Lamp.panel_id);

	FSimCabinItem Item;
	Item.LampId = ToF(Lamp.id);
	Item.LayoutIndex = LayoutIndex;
	Item.Location = BodyPoint(Panel.PointAt(Lamp.u_m, Lamp.v_m));
	Item.Normal = BodyDir(Panel.Normal());
	Item.Right = BodyDir(Panel.Right());
	Item.Up = BodyDir(Panel.Up());
	Item.MovingColor = LampColor(Lamp.color);
	Item.BaseColor = MetalColor;
	Item.LookKind = static_cast<uint8>(Lamp.look.kind);
	Item.Base = AddMesh(CylinderMesh, Item.BaseColor);
	const FRotator Facing = FRotationMatrix::MakeFromZ(Item.Normal).Rotator();
	const double W = Lamp.look.width_m > 0.0 ? Lamp.look.width_m : 0.022;
	const double H = Lamp.look.height_m > 0.0 ? Lamp.look.height_m : W;
	if (Lamp.look.kind == simcore::CabinLookKind::LampButton)
	{
		// An illuminated push-button: the glass cap in a ring.
		Item.BaseColor = LookColor(Lamp.look.ring, ChromeColor);
		Place(Item.Base, Item.Location + Item.Normal * 0.4, Facing, FVector(W, W, 0.008));
		Item.Moving = AddMesh(CylinderMesh, Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location + Item.Normal * 1.0, Facing, FVector(W * 0.65, W * 0.65, 0.01));
	}
	else if (FMath::Abs(W - H) > 1e-6)
	{
		// A rectangular lamp window (e.g. ПУСК ГОТОВ).
		Place(Item.Base, Item.Location + Item.Normal * 0.3, Facing, FVector(W * 1.15, H * 1.4, 0.006));
		Item.Base->SetStaticMesh(CubeMesh);
		Place(Item.Base, Item.Location + Item.Normal * 0.3, FRotationMatrix::MakeFromXZ(Item.Normal, Item.Up).Rotator(),
			FVector(0.006, W * 1.15, H * 1.4));
		Item.Moving = AddMesh(CubeMesh, Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location + Item.Normal * 0.7, FRotationMatrix::MakeFromXZ(Item.Normal, Item.Up).Rotator(),
			FVector(0.006, W, H));
	}
	else
	{
		Place(Item.Base, Item.Location + Item.Normal * 0.3, Facing, FVector(W * 1.4, W * 1.4, 0.006));
		Item.Moving = AddMesh(SphereMesh, Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location + Item.Normal * 0.8, FRotator::ZeroRotator, FVector(W));
	}
	// The glass glows by itself (unlit), so lit and dark lamps read the same in sun or shade: the screens'
	// unlit material showing a white texture, tinted with the lamp's colour.
	if (ScreenMaterial != nullptr)
	{
		// A tiny white texture per lamp; the material instance keeps it alive.
		UTextureRenderTarget2D* LampTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, 4, 4, RTF_RGBA8, FLinearColor::White);
		UMaterialInstanceDynamic* Glass = UMaterialInstanceDynamic::Create(ScreenMaterial, this);
		Glass->SetTextureParameterValue(TEXT("SlateUI"), LampTexture);
		Item.Moving->SetMaterial(0, Glass);
		Paint(Item.Moving, Item.MovingColor * 0.04f);
	}
	Items.Add(Item);
}

UStaticMeshComponent* USimCabinComponent::AddDecor(UStaticMesh* Mesh, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = AddMesh(Mesh, Color);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetMobility(EComponentMobility::Movable);
	DecorParts.Add(Part);
	return Part;
}

void USimCabinComponent::AddTextPlate(const FVector& Centre, const FVector& Normal, const FVector& Right, const FVector& Up,
	double WidthM, double HeightM, const FString& Text, const FLinearColor& Paper)
{
	static UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	if (ScreenMaterial == nullptr || Font == nullptr)
	{
		return;
	}
	// The texture: 1 px per 0.25 mm, dark text on the plate's colour.
	const int32 Tw = FMath::Clamp(FMath::RoundToInt32(WidthM * 4000.0), 16, 512);
	const int32 Th = FMath::Clamp(FMath::RoundToInt32(HeightM * 4000.0), 8, 256);
	UTextureRenderTarget2D* Target = UKismetRenderingLibrary::CreateRenderTarget2D(this, Tw, Th, RTF_RGBA8, Paper);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Target, Canvas, Size, Context);
	if (Canvas != nullptr)
	{
		const float Scale = static_cast<float>(Size.Y) / 30.0f;  // Roboto's default size is about 24 px high
		Canvas->K2_DrawText(Font, Text, Size * 0.5, FVector2D(Scale, Scale), FLinearColor(0.02f, 0.02f, 0.02f), 0.0f,
			FLinearColor::Transparent, FVector2D::ZeroVector, true, true);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	// The quad, 1.3 mm out, texture (0,0) at the reader's top left; both windings.
	UProceduralMeshComponent* Quad = NewObject<UProceduralMeshComponent>(GetOwner());
	Quad->SetupAttachment(this);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	Quad->RegisterComponent();
	const FVector Hr = Right * (50.0 * WidthM);
	const FVector Hu = Up * (50.0 * HeightM);
	const TArray<FVector> Vertices = {Centre - Hr + Hu, Centre + Hr + Hu, Centre - Hr - Hu, Centre + Hr - Hu};
	const TArray<FVector2D> Uv = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1)};
	const TArray<int32> Triangles = {0, 1, 2, 2, 1, 3, 0, 2, 1, 2, 3, 1};
	const TArray<FVector> Normals = {Normal, Normal, Normal, Normal};
	Quad->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, Uv, {}, {}, false);
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ScreenMaterial, this);
	Mid->SetTextureParameterValue(TEXT("SlateUI"), Target);
	Mid->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
	Mid->SetVectorParameterValue(TEXT("BackColor"), Paper);
	Mid->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
	Quad->SetMaterial(0, Mid);
	DecorParts.Add(Quad);
}

void USimCabinComponent::BuildBlock(const simcore::CabinLayout& Layout, const simcore::CabinBlock& Block)
{
	const simcore::CabinPanel& Panel = *Layout.FindPanel(Block.panel_id);
	const FVector N = BodyDir(Panel.Normal());
	const FVector U = BodyDir(Panel.Up());
	// The plate's front face on the panel's plane, a little smaller than its outline so neighbours show a gap.
	UStaticMeshComponent* Plate = AddDecor(CubeMesh, PlateColor);
	Place(Plate, BodyPoint(Panel.PointAt(Block.u_m, Block.v_m)) - N * (0.5 * PlateDepthCm),
		FRotationMatrix::MakeFromXZ(N, U).Rotator(), FVector(0.01 * PlateDepthCm, Block.width_m - 0.004, Block.height_m - 0.004));
}

void USimCabinComponent::BuildFixture(const simcore::CabinLayout& Layout, const simcore::CabinFixture& Fixture)
{
	using simcore::CabinLookKind;
	const simcore::CabinPanel& Panel = *Layout.FindPanel(Fixture.panel_id);
	const simcore::CabinLook& Look = Fixture.look;
	const FVector N = BodyDir(Panel.Normal());
	const FVector R = BodyDir(Panel.Right());
	const FVector U = BodyDir(Panel.Up());
	const FVector L = BodyPoint(Panel.PointAt(Fixture.u_m, Fixture.v_m));
	const FRotator Round = FRotationMatrix::MakeFromZX(N, U).Rotator();  // cylinders: axis out of the face
	const FRotator Flat = FRotationMatrix::MakeFromXZ(N, U).Rotator();   // cubes: X out of the face, Z up it
	const double W = Look.width_m;
	const double H = Look.height_m;
	const FLinearColor Color = LookColor(Look.color, DarkColor);
	// A cylinder standing out of the face: diameter (m) across, depth (cm) out, scaled to an ellipse for w != h.
	auto Disc = [&](double Dw, double Dh, double DepthCm, const FLinearColor& C, double BaseCm = 0.0)
	{
		UStaticMeshComponent* M = AddDecor(CylinderMesh, C);
		// The rotation puts the cylinder's X up the face: X takes the height, Y the width.
		Place(M, L + N * (BaseCm + 0.5 * DepthCm), Round, FVector(Dh, Dw, 0.01 * DepthCm));
		return M;
	};
	auto Box = [&](const FVector& At, double Bw, double Bh, double DepthCm, const FLinearColor& C, double BaseCm = 0.0)
	{
		UStaticMeshComponent* M = AddDecor(CubeMesh, C);
		Place(M, At + N * (BaseCm + 0.5 * DepthCm), Flat, FVector(0.01 * DepthCm, Bw, Bh));
		return M;
	};
	switch (Look.kind)
	{
	case CabinLookKind::Knob:
		Disc(W, W, FMath::Min(2.0, 60.0 * W), Color);
		break;
	case CabinLookKind::PointerKnob:
		Disc(W, W, FMath::Min(2.0, 60.0 * W), Color);
		Box(L + U * (25.0 * W), 0.003, 0.45 * W, 0.2, FLinearColor::White, FMath::Min(2.0, 60.0 * W));
		break;
	case CabinLookKind::Trim:
		Disc(W, W, 0.4, Color);
		Box(L, 0.75 * W, 0.002, 0.1, DarkColor, 0.4);
		break;
	case CabinLookKind::Toggle:
	{
		Disc(W, W, 0.6, ChromeColor);
		// Shown thrown down (off).
		const FVector Lever = (N * FMath::Cos(FMath::DegreesToRadians(-35.0)) + U * FMath::Sin(FMath::DegreesToRadians(-35.0)));
		UStaticMeshComponent* M = AddDecor(CylinderMesh, ChromeColor);
		Place(M, L + N * 0.6 + Lever * 1.6, FRotationMatrix::MakeFromZ(Lever).Rotator(), FVector(0.007, 0.007, 0.032));
		break;
	}
	case CabinLookKind::Lever:
		Box(L, W, H, 0.6, Color);
		break;
	case CabinLookKind::Lamp:
		Disc(W * 1.4, W * 1.4, 0.4, MetalColor);
		Disc(W, W, 0.6, Color * 0.5f, 0.4);
		break;
	case CabinLookKind::LampButton:
	case CabinLookKind::Button:
		Disc(W, W, 0.8, LookColor(Look.ring, ChromeColor));
		Disc(0.65 * W, 0.65 * W, 0.8, Color, 0.8);
		break;
	case CabinLookKind::Jack:
		Disc(W, W, 0.6, Color);
		Disc(0.4 * W, 0.4 * W, 0.1, DarkColor, 0.6);
		break;
	case CabinLookKind::Socket:
		Disc(W, W, 1.0, Color);
		Disc(0.45 * W, 0.45 * W, 0.1, DarkColor, 1.0);
		break;
	case CabinLookKind::Latch:
		Disc(W, W, 0.8, ChromeColor);
		Box(L, 0.25 * W, 0.6 * W, 0.4, DarkColor, 0.8);
		break;
	case CabinLookKind::Nameplate:
		Box(L, W, H, 0.15, FLinearColor(0.7f, 0.69f, 0.62f));
		AddTextPlate(L + N * 0.2, N, R, U, W, H, ToF(Look.label), FLinearColor(0.8f, 0.79f, 0.72f));
		break;
	case CabinLookKind::Bezel:
		Box(L, W, H, 1.0, Color);
		break;
	case CabinLookKind::Window:
		Box(L, W, H, 0.8, Color);
		break;
	case CabinLookKind::Oval:
		Disc(W, H, 0.8, Color);
		break;
	case CabinLookKind::Diagram:
	{
		// A printed outline: four thin lines.
		const FLinearColor Ink(0.25f, 0.2f, 0.08f);
		Box(L + U * (50.0 * H), W, 0.0015, 0.05, Ink);
		Box(L - U * (50.0 * H), W, 0.0015, 0.05, Ink);
		Box(L + R * (50.0 * W), 0.0015, H, 0.05, Ink);
		Box(L - R * (50.0 * W), 0.0015, H, 0.05, Ink);
		break;
	}
	case CabinLookKind::Default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------- animation

void USimCabinComponent::Animate(const USimWorldSubsystem* Sim, float DeltaTime)
{
	const simcore::CabinLayout& Layout = *Sim->GetCabinLayout();
	simcore::ConsoleData& Console = Sim->GetScenarioRunner()->Console();
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		FSimCabinItem& Item = Items[I];
		const bool bHighlighted = I == HighlightedItem;
		if (!Item.LampId.IsEmpty())
		{
			const bool bLit = simcore::CabinLayout::LampLit(Layout.lamps[Item.LayoutIndex], Console.panel, Console.equipment);
			const FLinearColor Glass = bLit ? Item.MovingColor : Item.MovingColor * 0.04f;
			Paint(Item.Moving, bHighlighted ? FMath::Lerp(Glass, HighlightColor, 0.4f) : Glass);
			Paint(Item.Base, bHighlighted ? HighlightColor : Item.BaseColor);
			continue;
		}
		Item.PressedFor = FMath::Max(0.0f, Item.PressedFor - DeltaTime);
		AnimateControl(Sim, Item);
		Paint(Item.Base, bHighlighted ? FMath::Lerp(Item.BaseColor, HighlightColor, 0.6f) : Item.BaseColor);
		Paint(Item.Moving, bHighlighted ? FMath::Lerp(Item.MovingColor, HighlightColor, 0.6f) : Item.MovingColor);
	}
}

void USimCabinComponent::AnimateControl(const USimWorldSubsystem* Sim, FSimCabinItem& Item)
{
	const simcore::CabinLayout& Layout = *Sim->GetCabinLayout();
	const simcore::CabinControl& Control = Layout.controls[Item.LayoutIndex];
	const simcore::ControlPanel& Panel = Sim->GetScenarioRunner()->Console().panel;
	const simcore::ControlDef* Def = Panel.Find(Control.control_id);
	if (Def == nullptr)
	{
		return;
	}
	const int32 Position = Panel.Get(Control.control_id);
	const FVector& L = Item.Location;
	const FVector& N = Item.Normal;
	switch (Control.style)
	{
	case simcore::CabinStyle::Toggle:
	{
		if (Item.LookKind == static_cast<uint8>(simcore::CabinLookKind::Button) ||
			Item.LookKind == static_cast<uint8>(simcore::CabinLookKind::LampButton))
		{
			// Drawn as a push-button (e.g. ВЫСОКОЕ ВКЛ.): the cap sits pressed in while the switch is on.
			const double D = (Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.025) * 0.65;
			Place(Item.Moving, L + N * (Position >= 1 ? 1.0 : 1.6), FRotationMatrix::MakeFromZ(N).Rotator(), FVector(D, D, 0.012));
			break;
		}
		// Lever thrown 35 degrees up the face for position 1 ("on"), down for 0.
		const double Face = simcore::CabinLayout::PositionAngleDeg(Control.style, Def->positions, Position);
		const double A = FMath::DegreesToRadians(Face < 90.0 ? 35.0 : -35.0);
		const FVector Lever = N * FMath::Cos(A) + Item.Up * FMath::Sin(A);
		Place(Item.Moving, L + N * 0.8 + Lever * 1.75, FRotationMatrix::MakeFromZ(Lever).Rotator(), FVector(0.008, 0.008, 0.035));
		break;
	}
	case simcore::CabinStyle::Rotary:
	{
		// Positions spread symmetrically about "up", at most 30 degrees apart.
		const FVector Dir = FaceDirection(Item, simcore::CabinLayout::PositionAngleDeg(Control.style, Def->positions, Position));
		const double D = Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.03;
		Place(Item.Moving, L + N * 1.0, FRotationMatrix::MakeFromZX(N, Dir).Rotator(), FVector(D, D, 0.02));
		Place(Item.Pointer, L + N * 2.05 + Dir * (30.0 * D), FRotationMatrix::MakeFromXZ(Dir, N).Rotator(),
			FVector(0.47 * D, 0.003, 0.002));
		break;
	}
	case simcore::CabinStyle::Key:
	{
		// No key in position 0; inserted upright; turned a quarter turn clockwise.
		Item.Moving->SetVisibility(Position >= 1);
		const FVector Dir = Position >= 2 ? Item.Right : Item.Up;
		Place(Item.Moving, L + N * 2.0, FRotationMatrix::MakeFromZX(N, Dir).Rotator(), FVector(0.025, 0.005, 0.03));
		break;
	}
	case simcore::CabinStyle::Button:
	{
		const bool bPressed = Position == 1 || Item.PressedFor > 0.0f;
		const double D = Item.LookWidthM > 0.0 ? 0.65 * Item.LookWidthM : 0.025;
		Place(Item.Moving, L + N * (bPressed ? 1.0 : 1.6), FRotationMatrix::MakeFromZ(N).Rotator(), FVector(D, D, 0.012));
		break;
	}
	case simcore::CabinStyle::Wheel:
	{
		const double Turns = Panel.GetValue(Control.control_id) / Control.units_per_turn;
		const FVector Dir = FaceDirection(Item, 360.0 * Turns);
		const double D = Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.12;
		Place(Item.Moving, L + N * 2.0, FRotationMatrix::MakeFromZX(N, Dir).Rotator(), FVector(D, D, 0.012));
		Place(Item.Pointer, L + N * 4.0 + Dir * (42.0 * D), FRotationMatrix::MakeFromZ(N).Rotator(), FVector(0.012, 0.012, 0.04));
		break;
	}
	case simcore::CabinStyle::Cover:
	{
		// A flap hinged along its top edge, 4.5 cm out from the panel; it swings up 110 degrees when open.
		const double Phi = FMath::DegreesToRadians(Position >= 1 ? 110.0 : 0.0);
		const FVector Hinge = L + Item.Up * 3.5 + N * 4.5;
		const FVector Down = -Item.Up * FMath::Cos(Phi) + N * FMath::Sin(Phi);
		const FVector FlapNormal = N * FMath::Cos(Phi) + Item.Up * FMath::Sin(Phi);
		Place(Item.Moving, Hinge + Down * 3.5, FRotationMatrix::MakeFromZX(FlapNormal, Item.Right).Rotator(), FVector(0.05, 0.07, 0.004));
		break;
	}
	}
}

// ---------------------------------------------------------------------------------------------- interaction

int32 USimCabinComponent::GetNumStations() const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	return Layout != nullptr ? static_cast<int32>(Layout->stations.size()) : 0;
}

bool USimCabinComponent::GetStationView(int32 Station, FVector& OutLocation, FRotator& OutRotation, FString& OutLabel) const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	if (Layout == nullptr || Station < 0 || Station >= static_cast<int32>(Layout->stations.size()))
	{
		return false;
	}
	const simcore::CabinStation& S = Layout->stations[Station];
	OutLocation = BodyPoint(S.eye_m);
	// Body-frame yaw is clockwise from forward, as Unreal's yaw is from +X.
	OutRotation = FRotator(FMath::RadiansToDegrees(S.pitch_rad), FMath::RadiansToDegrees(S.yaw_rad), 0.0);
	OutLabel = ToF(S.label);
	return true;
}

int32 USimCabinComponent::GetNumDisplays() const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	return Layout != nullptr ? static_cast<int32>(Layout->displays.size()) : 0;
}

bool USimCabinComponent::GetDisplayView(int32 Display, int32& OutStation, FVector& OutCentre, FString& OutLabel) const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	if (Layout == nullptr || Display < 0 || Display >= static_cast<int32>(Layout->displays.size()))
	{
		return false;
	}
	const simcore::CabinDisplay& D = Layout->displays[Display];
	const simcore::CabinPanel& Panel = *Layout->FindPanel(D.panel_id);
	OutStation = INDEX_NONE;
	for (int32 I = 0; I < static_cast<int32>(Layout->stations.size()); ++I)
	{
		if (Layout->stations[I].id == Panel.station)
		{
			OutStation = I;
		}
	}
	OutCentre = BodyPoint(Panel.PointAt(D.u_m, D.v_m));
	OutLabel = ToF(D.label);
	return OutStation != INDEX_NONE;
}

int32 USimCabinComponent::TraceItem(const FVector& WorldStart, const FVector& WorldEnd) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SimCabinTrace), false);
	double BestDistance = TNumericLimits<double>::Max();
	int32 Best = INDEX_NONE;
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		for (UStaticMeshComponent* Part : {Items[I].Base.Get(), Items[I].Moving.Get(), Items[I].Pointer.Get()})
		{
			FHitResult Hit;
			if (Part != nullptr && Part->IsVisible() && Part->LineTraceComponent(Hit, WorldStart, WorldEnd, Params) &&
				Hit.Distance < BestDistance)
			{
				BestDistance = Hit.Distance;
				Best = I;
			}
		}
	}
	return Best;
}

bool USimCabinComponent::IsWheel(int32 Item) const
{
	return Items.IsValidIndex(Item) && Items[Item].LampId.IsEmpty() &&
		   Items[Item].Style == static_cast<uint8>(simcore::CabinStyle::Wheel);
}

FString USimCabinComponent::DescribeItem(int32 Item) const
{
	const USimWorldSubsystem* Sim = GetSim();
	if (!Items.IsValidIndex(Item) || Sim == nullptr || Sim->GetCabinLayout() == nullptr || Sim->GetScenarioRunner() == nullptr)
	{
		return FString();
	}
	const FSimCabinItem& It = Items[Item];
	const simcore::CabinLayout& Layout = *Sim->GetCabinLayout();
	simcore::ConsoleData& Console = Sim->GetScenarioRunner()->Console();
	if (!It.LampId.IsEmpty())
	{
		const simcore::CabinLamp& Lamp = Layout.lamps[It.LayoutIndex];
		const bool bLit = simcore::CabinLayout::LampLit(Lamp, Console.panel, Console.equipment);
		return FString::Printf(TEXT("%s  (lamp: %s)"), *ToF(Lamp.label), bLit ? TEXT("lit") : TEXT("off"));
	}
	const simcore::ControlDef* Def = Console.panel.Find(ToStd(It.ControlId));
	if (Def == nullptr)
	{
		return It.ControlId;
	}
	FString State;
	if (Def->kind == simcore::ControlKind::Continuous)
	{
		State = FString::Printf(TEXT("%.1f  (drag or scroll to turn)"), Console.panel.GetValue(Def->id));
	}
	else if (Def->kind == simcore::ControlKind::Momentary)
	{
		State = TEXT("push-button");
	}
	else
	{
		const int32 Position = Console.panel.Get(Def->id);
		State = Position >= 0 && Position < static_cast<int32>(Def->position_labels.size())
					? ToF(Def->position_labels[Position])
					: FString::Printf(TEXT("position %d"), Position);
	}
	FString Guard;
	if (!Def->guard_id.empty() && Console.panel.Get(Def->guard_id) < 1)
	{
		const simcore::ControlDef* GuardDef = Console.panel.Find(Def->guard_id);
		Guard = FString::Printf(TEXT("  [closed: %s]"), GuardDef != nullptr ? *ToF(GuardDef->label) : *ToF(Def->guard_id));
	}
	return FString::Printf(TEXT("%s: %s%s"), *ToF(Def->label), *State, *Guard);
}

ESimPlayerActionResult USimCabinComponent::ClickItem(int32 Item, int32 Direction)
{
	USimWorldSubsystem* Sim = GetSim();
	if (!Items.IsValidIndex(Item) || Items[Item].ControlId.IsEmpty() || IsWheel(Item) || Sim == nullptr ||
		Sim->GetScenarioRunner() == nullptr)
	{
		return ESimPlayerActionResult::Invalid;
	}
	FSimCabinItem& It = Items[Item];
	const simcore::ControlPanel& Panel = Sim->GetScenarioRunner()->Console().panel;
	const simcore::ControlDef* Def = Panel.Find(ToStd(It.ControlId));
	if (Def == nullptr)
	{
		return ESimPlayerActionResult::Invalid;
	}
	const int32 Target = simcore::CabinClickTarget(*Def, Panel.Get(Def->id), Direction);
	const ESimPlayerActionResult Result = Sim->PlayerSetControl(It.ControlId, Target);
	if (Def->kind == simcore::ControlKind::Momentary && Result != ESimPlayerActionResult::Blocked &&
		Result != ESimPlayerActionResult::NotYet && Result != ESimPlayerActionResult::Invalid)
	{
		It.PressedFor = 0.15f;
	}
	return Result;
}

bool USimCabinComponent::TurnWheel(int32 Item, double Turns)
{
	USimWorldSubsystem* Sim = GetSim();
	if (!IsWheel(Item) || Sim == nullptr || Sim->GetCabinLayout() == nullptr)
	{
		return false;
	}
	const simcore::CabinControl& Control = Sim->GetCabinLayout()->controls[Items[Item].LayoutIndex];
	const double Value = Sim->GetWheelValue(Items[Item].ControlId) + Turns * Control.units_per_turn;
	return Sim->PlayerTurnWheel(Items[Item].ControlId, Value);
}
