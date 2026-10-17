#include "SimCabinComponent.h"

#include "Components/PointLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
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
	const FLinearColor CabinPaint(0.15f, 0.2f, 0.18f);   // the compartment's grey-green paint
	const FLinearColor FloorColor(0.04f, 0.04f, 0.04f);
	const FLinearColor GlassColor(0.01f, 0.012f, 0.015f);
	const FName CabinLightTag(TEXT("SAM3DCabinLight"));
	const FLinearColor MetalColor(0.25f, 0.25f, 0.27f);
	const FLinearColor DarkColor(0.02f, 0.02f, 0.02f);
	const FLinearColor ChromeColor(0.7f, 0.7f, 0.72f);
	const FLinearColor CoverColor(0.5f, 0.49f, 0.43f);   // guards: painted metal like the plates (samsim_panels)
	const FLinearColor LegendUnlit(0.22f, 0.22f, 0.21f); // a legend lamp's letters when dark: white glass, unlit
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
		// the PU-12M's equipment paints (rwd_mb3's colour photographs): the ASPD-U's and the power blocks' blue-grey, the
		// IT-45's, the ДХ's and the IT-2M's light grey, the ДХ's ivory keys
		if (Name == "bluegrey") return FLinearColor(0.16f, 0.2f, 0.24f);
		if (Name == "lightgrey") return FLinearColor(0.36f, 0.37f, 0.38f);
		if (Name == "ivory") return FLinearColor(0.7f, 0.66f, 0.52f);
		return Fallback;
	}

	// How shiny a part is, from the colour it is built with: chrome and brass are polished metal, the plates satin
	// paint, the rack matt, anything else glossy bakelite or enamel. (Roughness, Metallic.)
	FVector2D FinishFor(const FLinearColor& C)
	{
		if (C.Equals(ChromeColor) || C.Equals(LookColor("chrome", FLinearColor::Black))) return FVector2D(0.22, 1.0);
		if (C.Equals(BrassColor) || C.Equals(LookColor("brass", FLinearColor::Black))) return FVector2D(0.35, 1.0);
		if (C.Equals(MetalColor)) return FVector2D(0.4, 0.8);
		if (C.Equals(PlateColor)) return FVector2D(0.55, 0.0);
		if (C.Equals(PanelColor)) return FVector2D(0.85, 0.0);
		if (C.Equals(CabinPaint)) return FVector2D(0.65, 0.0);
		if (C.Equals(FloorColor)) return FVector2D(0.9, 0.0);
		if (C.Equals(GlassColor)) return FVector2D(0.05, 0.0);
		return FVector2D(0.28, 0.0);
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

	// The parts library's meshes stand along +Z with "up the face" along their -Y (as imported from Blender).
	FRotator PartRotation(const FVector& Out, const FVector& UpTheFace)
	{
		return FRotationMatrix::MakeFromZY(Out, -UpTheFace).Rotator();
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
		bHiddenApplied = !bHidden;  // the new parts take the cabin's state
	}
	if (bHiddenApplied != bHidden)
	{
		SetHiddenInGame(bHidden, true);
		bHiddenApplied = bHidden;
	}
	Animate(Sim, DeltaTime);
}

// ---------------------------------------------------------------------------------------------- building

void USimCabinComponent::UseCabinLighting(UPrimitiveComponent* Part)
{
	Part->LightingChannels.bChannel0 = false;
	Part->LightingChannels.bChannel1 = true;
}

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
	UseCabinLighting(Part);
	Part->RegisterComponent();
	if (UMaterialInterface* Base = SurfaceMaterial != nullptr ? SurfaceMaterial.Get() : ShapeMaterial.Get())
	{
		UMaterialInstanceDynamic* Mid = Part->CreateDynamicMaterialInstance(0, Base);
		const FVector2D Finish = FinishFor(Color);
		Mid->SetScalarParameterValue(TEXT("Roughness"), static_cast<float>(Finish.X));
		Mid->SetScalarParameterValue(TEXT("Metallic"), static_cast<float>(Finish.Y));
	}
	Paint(Part, Color);
	return Part;
}

void USimCabinComponent::Paint(UMeshComponent* Mesh, const FLinearColor& Color) const
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
	FacedPanels.Reset();
	HighlightedItem = INDEX_NONE;
	BuiltLayout = nullptr;
}

bool USimCabinComponent::Build(const USimWorldSubsystem* Sim)
{
	DestroyParts();
	const simcore::CabinLayout* Layout = Sim->GetCabinLayout();
	if (SurfaceMaterial == nullptr)
	{
		SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Cabin/Materials/M_CabinSurface.M_CabinSurface"),
			nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (LabelMaterial == nullptr)
	{
		LabelMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Cabin/Materials/M_CabinLabel.M_CabinLabel"),
			nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (CrtMaterial == nullptr)
	{
		CrtMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Cabin/Materials/M_CabinCRT.M_CabinCRT"), nullptr,
			LOAD_NoWarn | LOAD_Quiet);
	}
	if (PanelFaceMaterial == nullptr)
	{
		PanelFaceMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/SAM3D/Cabin/Materials/MI_Cabin_PanelFace.MI_Cabin_PanelFace"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Layout == nullptr || CubeMesh == nullptr || CylinderMesh == nullptr || SphereMesh == nullptr)
	{
		return false;
	}
	for (const simcore::CabinPanel& Panel : Layout->panels)
	{
		if (Panel.bare)
		{
			continue;  // the front of a mesh with a plate of its own (an R-123M): nothing to draw behind its controls
		}
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
			Paint(Mesh, LookColor(Panel.color, PlateColor * 0.6f));
		}
		PanelMeshes.Add(Mesh);
	}
	for (const simcore::CabinBlock& Block : Layout->blocks)
	{
		BuildBlock(*Layout, Block);
	}
	for (const simcore::CabinPanel& Panel : Layout->panels)
	{
		if (!Panel.bare && BuildPanelFace(*Layout, Panel))
		{
			FacedPanels.Add(ToF(Panel.id));
		}
	}
	for (const simcore::CabinShellPart& Part : Layout->shell)
	{
		BuildShell(Part);
	}
	for (const simcore::CabinFixture& Fixture : Layout->fixtures)
	{
		BuildFixture(*Layout, Fixture);
	}
	// A faint fill above each seat (no source: it stands in for the panels' scale lights and the light the screens
	// throw), so a console reads even with the dome lamps' shadows on it.
	for (const simcore::CabinStation& Station : Layout->stations)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(GetOwner());
		Light->SetupAttachment(this);
		Light->ComponentTags.Add(CabinLightTag);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(40.0f);
		Light->SetAttenuationRadius(120.0f);
		Light->SetSourceRadius(15.0f);
		Light->SetLightColor(FLinearColor(1.0f, 0.92f, 0.8f));
		Light->SetCastShadows(false);
		Light->LightingChannels.bChannel0 = false;  // the cabin's channel only (UseCabinLighting)
		Light->LightingChannels.bChannel1 = true;
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
			BodyDir(Panel.Up()), CrtMaterial != nullptr ? CrtMaterial.Get() : ScreenMaterial.Get(), CubeMesh, ShapeMaterial);
		Displays.Add(Display);
	}
	BuiltLayout = Layout;
	UE_LOG(LogSAM3D, Log, TEXT("Cabin built: %d panels (%d with baked faces), %d blocks, %d fixtures, %d shell parts, %d items, %d displays"),
		PanelMeshes.Num(), FacedPanels.Num(), static_cast<int32>(Layout->blocks.size()), static_cast<int32>(Layout->fixtures.size()),
		static_cast<int32>(Layout->shell.size()), Items.Num(), Displays.Num());
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
	// The parts library, where it has the meshes: a toggle's nut and lever, a push-button's ring and cap, a knob,
	// a hand wheel's dish and handle (AnimateControl places them).
	UStaticMesh* PartBase = nullptr;
	double PartBaseSize = 0.0;
	if (Control.style == simcore::CabinStyle::Toggle && !bCapLook && PartMesh(TEXT("SM_ToggleBase")) && PartMesh(TEXT("SM_ToggleLever")))
	{
		Item.Moving->SetStaticMesh(PartMesh(TEXT("SM_ToggleLever")));
		PartBase = PartMesh(TEXT("SM_ToggleBase"));
		PartBaseSize = Control.look.width_m > 0.0 ? Control.look.width_m : 0.016;
	}
	else if ((Control.style == simcore::CabinStyle::Button || (Control.style == simcore::CabinStyle::Toggle && bCapLook)) &&
			 PartMesh(TEXT("SM_ButtonRing")) && PartMesh(TEXT("SM_ButtonCap")))
	{
		Item.Moving->SetStaticMesh(PartMesh(TEXT("SM_ButtonCap")));
		PartBase = PartMesh(TEXT("SM_ButtonRing"));
		PartBaseSize = Control.look.width_m > 0.0 ? Control.look.width_m : 0.03;
		Item.LookWidthM = PartBaseSize;
	}
	else if (Control.style == simcore::CabinStyle::Rotary && PartMesh(TEXT("SM_KnobPointer")))
	{
		// The R-123M's T-bar knobs where the layout asks for them and the part is imported; the pointer knob otherwise.
		UStaticMesh* KnobMesh = Control.look.kind == simcore::CabinLookKind::BarKnob && PartMesh(TEXT("SM_KnobBar"))
			? PartMesh(TEXT("SM_KnobBar")) : PartMesh(TEXT("SM_KnobPointer"));
		Item.Moving->SetStaticMesh(KnobMesh);
		Item.Pointer->SetVisibility(false);
		Item.bParts = true;
	}
	else if (Control.style == simcore::CabinStyle::Wheel && PartMesh(TEXT("SM_WheelDish")) && PartMesh(TEXT("SM_WheelHandle")))
	{
		Item.Moving->SetStaticMesh(PartMesh(TEXT("SM_WheelDish")));
		Item.Pointer->SetStaticMesh(PartMesh(TEXT("SM_WheelHandle")));
		Item.bParts = true;
	}
	if (PartBase != nullptr)
	{
		Item.bParts = true;
		Item.Base = AddMesh(PartBase, Item.BaseColor);
		Place(Item.Base, Item.Location, PartRotation(Item.Normal, Item.Up), FVector(PartBaseSize));
	}
	else if (Control.style != simcore::CabinStyle::Cover && !(Item.bParts && Control.style == simcore::CabinStyle::Rotary))
	{
		Item.Base = AddMesh(CylinderMesh, Item.BaseColor);
		Place(Item.Base, Item.Location + Item.Normal * (50.0 * Collar.Y), FRotationMatrix::MakeFromZ(Item.Normal).Rotator(),
			FVector(Collar.X, Collar.X, Collar.Y));
	}
	if (!Control.auto_placed && !HasBakedFace(Control.panel_id))
	{
		AddLabel(Item.Location, Item.Normal, Item.Right, Item.Up,
			Control.look.width_m > 0.0 ? Control.look.width_m : (Control.style == simcore::CabinStyle::Wheel ? 0.12 : 0.03),
			ToF(Control.look.label));
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
	if (Lamp.look.kind == simcore::CabinLookKind::Legend)
	{
		// A legend lamp: black glass 1.2 cm proud (clear of the housings it sits in), with the letters on it, which
		// light up in the lamp's colour (Animate).
		Item.BaseColor = DarkColor;
		if (FMath::Abs(W - H) > 1e-6)
		{
			Item.Base->SetStaticMesh(CubeMesh);
			Place(Item.Base, Item.Location + Item.Normal * 0.6, FRotationMatrix::MakeFromXZ(Item.Normal, Item.Up).Rotator(),
				FVector(0.012, W, H));
		}
		else
		{
			Place(Item.Base, Item.Location + Item.Normal * 0.6, Facing, FVector(W, W, 0.012));
		}
		Paint(Item.Base, Item.BaseColor);
		Item.Legend = AddLegend(Item.Location + Item.Normal * 1.24, Item.Normal, Item.Right, Item.Up, W, H,
			ToF(Lamp.look.label.empty() ? Lamp.label : Lamp.look.label));
		Paint(Item.Legend, LegendUnlit);
		Items.Add(Item);
		return;
	}
	if (Lamp.look.kind == simcore::CabinLookKind::LampButton && PartMesh(TEXT("SM_ButtonRing")) && PartMesh(TEXT("SM_ButtonCap")))
	{
		// An illuminated push-button from the parts library: the glass cap in its ring.
		Item.bParts = true;
		Item.BaseColor = LookColor(Lamp.look.ring, ChromeColor);
		Item.Base->SetStaticMesh(PartMesh(TEXT("SM_ButtonRing")));
		Place(Item.Base, Item.Location, PartRotation(Item.Normal, Item.Up), FVector(W));
		Item.Moving = AddMesh(PartMesh(TEXT("SM_ButtonCap")), Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location, PartRotation(Item.Normal, Item.Up), FVector(W));
	}
	else if (Lamp.look.kind == simcore::CabinLookKind::LampButton)
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
	else if (PartMesh(TEXT("SM_LampBezel")) && PartMesh(TEXT("SM_LampLens")))
	{
		// A lamp from the parts library: the lens in its chrome bezel.
		Item.bParts = true;
		Item.BaseColor = ChromeColor;
		Item.Base->SetStaticMesh(PartMesh(TEXT("SM_LampBezel")));
		Place(Item.Base, Item.Location, PartRotation(Item.Normal, Item.Up), FVector(W * 1.4));
		Item.Moving = AddMesh(PartMesh(TEXT("SM_LampLens")), Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location, PartRotation(Item.Normal, Item.Up), FVector(W * 1.4));
	}
	else
	{
		Place(Item.Base, Item.Location + Item.Normal * 0.3, Facing, FVector(W * 1.4, W * 1.4, 0.006));
		Item.Moving = AddMesh(SphereMesh, Item.MovingColor * 0.08f);
		Place(Item.Moving, Item.Location + Item.Normal * 0.8, FRotator::ZeroRotator, FVector(W));
	}
	if (!HasBakedFace(Lamp.panel_id))
	{
		AddLabel(Item.Location, Item.Normal, Item.Right, Item.Up, FMath::Max(W, H) * (Item.bParts ? 1.4 : 1.0), ToF(Lamp.look.label));
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

UStaticMesh* USimCabinComponent::PartMesh(const TCHAR* Name)
{
	const FName Key(Name);
	if (const TObjectPtr<UStaticMesh>* Found = PartMeshes.Find(Key))
	{
		return Found->Get();
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SAM3D/Cabin/Parts/%s.%s"), Name, Name),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	PartMeshes.Add(Key, Mesh);
	return Mesh;
}

UStaticMeshComponent* USimCabinComponent::AddDecor(UStaticMesh* Mesh, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = AddMesh(Mesh, Color);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetMobility(EComponentMobility::Movable);
	DecorParts.Add(Part);
	return Part;
}

UProceduralMeshComponent* USimCabinComponent::AddTextPlate(const FVector& Centre, const FVector& Normal, const FVector& Right,
	const FVector& Up, double WidthM, double HeightM, const FString& Text, const FLinearColor& Paper, bool bEngraved)
{
	static UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	UMaterialInterface* Material = bEngraved ? LabelMaterial.Get() : ScreenMaterial.Get();
	if (Material == nullptr || Font == nullptr)
	{
		return nullptr;
	}
	// The texture: 1 px per 0.25 mm, dark text on the plate's colour.
	const int32 Tw = FMath::Clamp(FMath::RoundToInt32(WidthM * 4000.0), 16, 512);
	const int32 Th = FMath::Clamp(FMath::RoundToInt32(HeightM * 4000.0), 8, 256);
	// Engraved text: white letters on black, the red channel masking the label material (only the letters show).
	UTextureRenderTarget2D* Target =
		UKismetRenderingLibrary::CreateRenderTarget2D(this, Tw, Th, RTF_RGBA8, bEngraved ? FLinearColor::Black : Paper);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Target, Canvas, Size, Context);
	if (Canvas != nullptr)
	{
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), true);
		const int32 Count = FMath::Max(1, Lines.Num());
		// Roboto's default size is about 24 px high: the lines share the height, shrunk to fit the width.
		float Scale = static_cast<float>(Size.Y) / (30.0f * Count);
		for (const FString& Line : Lines)
		{
			const FVector2D Extent = Canvas->K2_TextSize(Font, Line, FVector2D(1.0, 1.0));
			if (Extent.X > 0.0)
			{
				Scale = FMath::Min(Scale, static_cast<float>(0.95 * Size.X / Extent.X));
			}
		}
		const bool bLightInk = bEngraved || Paper.GetLuminance() < 0.2f;
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			const FVector2D At(0.5 * Size.X, Size.Y * (I + 0.5) / Count);
			Canvas->K2_DrawText(Font, Lines[I], At, FVector2D(Scale, Scale),
				bLightInk ? FLinearColor::White : FLinearColor(0.02f, 0.02f, 0.02f), 0.0f, FLinearColor::Transparent,
				FVector2D::ZeroVector, true, true);
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	// The quad, 1.3 mm out, texture (0,0) at the reader's top left; both windings.
	UProceduralMeshComponent* Quad = NewObject<UProceduralMeshComponent>(GetOwner());
	Quad->SetupAttachment(this);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	UseCabinLighting(Quad);
	Quad->RegisterComponent();
	const FVector Hr = Right * (50.0 * WidthM);
	const FVector Hu = Up * (50.0 * HeightM);
	const TArray<FVector> Vertices = {Centre - Hr + Hu, Centre + Hr + Hu, Centre - Hr - Hu, Centre + Hr - Hu};
	const TArray<FVector2D> Uv = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1)};
	const TArray<int32> Triangles = {0, 1, 2, 2, 1, 3, 0, 2, 1, 2, 3, 1};
	const TArray<FVector> Normals = {Normal, Normal, Normal, Normal};
	Quad->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, Uv, {}, {}, false);
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Material, this);
	Mid->SetTextureParameterValue(bEngraved ? TEXT("Text") : TEXT("SlateUI"), Target);
	Mid->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
	Mid->SetVectorParameterValue(TEXT("BackColor"), bEngraved ? FLinearColor(0.0f, 0.0f, 0.0f, 0.0f) : Paper);
	Mid->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
	Quad->SetMaterial(0, Mid);
	DecorParts.Add(Quad);
	return Quad;
}

UProceduralMeshComponent* USimCabinComponent::AddLegend(const FVector& Centre, const FVector& Normal, const FVector& Right,
	const FVector& Up, double WidthM, double HeightM, const FString& Text)
{
	// A long label with a space goes on two lines, as on the caps (e.g. УЗКИЙ / ЛУЧ). The letters keep inside the
	// glass: a round cap's text area is a rectangle inside its circle.
	FString Shown = Text;
	int32 Space = INDEX_NONE;
	if (Text.Len() > 6 && Text.FindChar(TEXT(' '), Space))
	{
		Shown = Text.Left(Space) + TEXT("\n") + Text.Mid(Space + 1);
	}
	const bool bRound = FMath::Abs(WidthM - HeightM) < 1e-6;
	return AddTextPlate(Centre, Normal, Right, Up, bRound ? 0.74 * WidthM : 0.9 * WidthM, bRound ? 0.5 * WidthM : 0.7 * HeightM,
		Shown, FLinearColor::Black);
}

void USimCabinComponent::AddLabel(const FVector& Centre, const FVector& Normal, const FVector& Right, const FVector& Up,
	double ItemHeightM, const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}
	// Letters about 4 mm high (as engraved on the samsim_panels photos), 4 mm above the item.
	constexpr double LetterM = 0.004;
	const double WidthM = FMath::Min(0.11, 0.0026 * Text.Len() + 0.004);
	AddTextPlate(Centre + Up * (100.0 * (0.5 * ItemHeightM + 0.004 + 0.5 * LetterM)) + Normal * 0.05, Normal, Right, Up, WidthM,
		LetterM * 1.4, Text, FLinearColor::Transparent, true);
}

void USimCabinComponent::BuildShell(const simcore::CabinShellPart& Part)
{
	const FVector Centre = BodyPoint(Part.centre_m);
	if (Part.kind == simcore::CabinShellKind::Light)
	{
		// An interior lamp reaching as far as the part's size: an incandescent bulb's warm white, or a blackout lamp's
		// blue. It casts shadows (the racks, seats and crew shade each other), softened by the bulb's size.
		UPointLightComponent* Light = NewObject<UPointLightComponent>(GetOwner());
		Light->SetupAttachment(this);
		Light->ComponentTags.Add(CabinLightTag);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(Part.lumens > 0.0 ? static_cast<float>(Part.lumens) : 400.0f);
		Light->SetAttenuationRadius(static_cast<float>(100.0 * Part.size_m.x));
		Light->SetSourceRadius(4.0f);
		Light->SetSoftSourceRadius(6.0f);
		Light->SetLightColor(Part.color == "blue" ? FLinearColor(0.25f, 0.45f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.62f));
		Light->SetCastShadows(Part.color != "blue");
		Light->LightingChannels.bChannel0 = false;  // the cabin's channel only (UseCabinLighting)
		Light->LightingChannels.bChannel1 = true;
		Light->RegisterComponent();
		Light->SetRelativeLocation(Centre);
		return;
	}
	if (Part.kind == simcore::CabinShellKind::Mesh)
	{
		// A modelled part (art/cabin_interior, imported to /Game/SAM3D/Cabin/Interior with its own materials): its
		// origin is the centre of its bounding box, turned about the vertical by the part's yaw (clockwise from forward,
		// as Unreal's yaw from +X). Without the mesh, a box of its size stands in for it.
		const FString Name = ToF(Part.mesh);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SAM3D/Cabin/Interior/%s.%s"), *Name, *Name),
			nullptr, LOAD_NoWarn | LOAD_Quiet);
		const FRotator Yaw(0.0, FMath::RadiansToDegrees(Part.yaw_rad), 0.0);
		if (Mesh == nullptr)
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Cabin: mesh %s isn't imported (tools/ue_import_cabin_interior.py); a box stands in"), *Name);
			UStaticMeshComponent* Box = AddDecor(CubeMesh, CabinPaint);
			Place(Box, Centre, Yaw, FVector(Part.size_m.y, Part.size_m.x, Part.size_m.z));
			return;
		}
		UStaticMeshComponent* Part3D = NewObject<UStaticMeshComponent>(GetOwner());
		Part3D->SetStaticMesh(Mesh);
		Part3D->SetMobility(EComponentMobility::Movable);
		Part3D->SetupAttachment(this);
		Part3D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// The shell and furniture shade the cabin lamps' light and block the sky's (Lumen) outside the windows.
		Part3D->SetCastShadow(true);
		UseCabinLighting(Part3D);
		Part3D->RegisterComponent();
		Part3D->SetRelativeLocationAndRotation(Centre, Yaw);
		DecorParts.Add(Part3D);
		return;
	}
	FLinearColor Color = LookColor(Part.color, CabinPaint);
	if (Part.color == "paint" || Part.color.empty()) Color = CabinPaint;
	if (Part.color == "floor") Color = FloorColor;
	if (Part.kind == simcore::CabinShellKind::Glass) Color = GlassColor;
	if (Part.color == "white") Color = PlateColor;  // racks: the same cream plates as the console
	UStaticMeshComponent* Box = AddDecor(CubeMesh, Color);
	// Body frame (x right, y forward, z up) to this component (X forward, Y right, Z up); a cube is 1 m across.
	Place(Box, Centre, FRotator::ZeroRotator, FVector(Part.size_m.y, Part.size_m.x, Part.size_m.z));
}

UProceduralMeshComponent* USimCabinComponent::AddFaceQuad(const FVector& Centre, const FVector& Normal, const FVector& Right,
	const FVector& Up, double WidthM, double HeightM, const FVector2D& UvMin, const FVector2D& UvMax, UMaterialInterface* Material)
{
	UProceduralMeshComponent* Quad = NewObject<UProceduralMeshComponent>(GetOwner());
	Quad->SetupAttachment(this);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	UseCabinLighting(Quad);
	Quad->RegisterComponent();
	const FVector C = Centre + Normal * 0.02;
	const FVector Hr = Right * (50.0 * WidthM);
	const FVector Hu = Up * (50.0 * HeightM);
	// Texture (0, 0) at the reader's top left; both windings, like the text plates (the back one is culled). Tangents
	// along the reader's right, so the material's detail normal map lies the right way.
	const TArray<FVector> Vertices = {C - Hr + Hu, C + Hr + Hu, C - Hr - Hu, C + Hr - Hu};
	const TArray<FVector2D> Uv = {FVector2D(UvMin.X, UvMin.Y), FVector2D(UvMax.X, UvMin.Y), FVector2D(UvMin.X, UvMax.Y),
		FVector2D(UvMax.X, UvMax.Y)};
	const TArray<int32> Triangles = {0, 1, 2, 2, 1, 3, 0, 2, 1, 2, 3, 1};
	const TArray<FVector> Normals = {Normal, Normal, Normal, Normal};
	const FProcMeshTangent Tangent(Right, false);
	Quad->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, Uv, {}, {Tangent, Tangent, Tangent, Tangent}, false);
	Quad->SetMaterial(0, Material);
	DecorParts.Add(Quad);
	return Quad;
}

bool USimCabinComponent::BuildPanelFace(const simcore::CabinLayout& Layout, const simcore::CabinPanel& Panel)
{
	if (PanelFaceMaterial == nullptr)
	{
		return false;
	}
	const FString Name = TEXT("T_Panel_") + ToF(Panel.id);
	UTexture* Face = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/SAM3D/Cabin/Textures/%s.%s"), *Name, *Name),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Face == nullptr)
	{
		return false;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(PanelFaceMaterial, this);
	Mid->SetTextureParameterValue(TEXT("BaseTexture"), Face);
	const FVector N = BodyDir(Panel.Normal());
	const FVector R = BodyDir(Panel.Right());
	const FVector U = BodyDir(Panel.Up());
	// Panel (u, v) to the texture: u right from the left edge, v down from the top edge, both 0..1.
	auto ToUv = [&](double Um, double Vm)
	{
		return FVector2D((Um + 0.5 * Panel.width_m) / Panel.width_m, (0.5 * Panel.height_m - Vm) / Panel.height_m);
	};
	bool bAny = false;
	for (const simcore::CabinBlock& Block : Layout.blocks)
	{
		if (Block.panel_id != Panel.id)
		{
			continue;
		}
		// Over the plate's face (the plates are 4 mm smaller than their outline, BuildBlock).
		const double W = Block.width_m - 0.004;
		const double H = Block.height_m - 0.004;
		AddFaceQuad(BodyPoint(Panel.PointAt(Block.u_m, Block.v_m)), N, R, U, W, H,
			ToUv(Block.u_m - 0.5 * W, Block.v_m + 0.5 * H), ToUv(Block.u_m + 0.5 * W, Block.v_m - 0.5 * H), Mid);
		bAny = true;
	}
	if (!bAny)
	{
		// A panel without blocks is one plate: the whole face.
		AddFaceQuad(BodyPoint(Panel.centre_m), N, R, U, Panel.width_m, Panel.height_m, FVector2D(0.0, 0.0), FVector2D(1.0, 1.0), Mid);
	}
	return true;
}

void USimCabinComponent::BuildBlock(const simcore::CabinLayout& Layout, const simcore::CabinBlock& Block)
{
	const simcore::CabinPanel& Panel = *Layout.FindPanel(Block.panel_id);
	const FVector N = BodyDir(Panel.Normal());
	const FVector U = BodyDir(Panel.Up());
	// The plate's front face on the panel's plane, a little smaller than its outline so neighbours show a gap.
	UStaticMeshComponent* Plate = AddDecor(CubeMesh, LookColor(Block.color, PlateColor));
	Place(Plate, BodyPoint(Panel.PointAt(Block.u_m, Block.v_m)) - N * (0.5 * PlateDepthCm),
		FRotationMatrix::MakeFromXZ(N, U).Rotator(), FVector(0.01 * PlateDepthCm, Block.width_m - 0.004, Block.height_m - 0.004));
	// Fixing screws 1 cm in from the corners, and along the long edges (decoration, not from the photos).
	if (UStaticMesh* Screw = PartMesh(TEXT("SM_Screw")))
	{
		const double Hw = 0.5 * Block.width_m - 0.01;
		const double Hh = 0.5 * Block.height_m - 0.01;
		TArray<FVector2D> At = {FVector2D(-Hw, -Hh), FVector2D(Hw, -Hh), FVector2D(-Hw, Hh), FVector2D(Hw, Hh)};
		if (Block.width_m > 0.3 && Block.height_m > 0.08)
		{
			At.Add(FVector2D(0.0, -Hh));
			At.Add(FVector2D(0.0, Hh));
		}
		for (const FVector2D& P : At)
		{
			Place(AddDecor(Screw, ChromeColor * 0.7f), BodyPoint(Panel.PointAt(Block.u_m + P.X, Block.v_m + P.Y)),
				PartRotation(N, U), FVector(0.007));
		}
	}
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
	// The engraved label above the item (nameplates and legend lamps show their own text; big surrounds and diagrams
	// have none). A baked panel face has the labels and the printed diagrams in it.
	const bool bFaced = HasBakedFace(Fixture.panel_id);
	if (bFaced && Look.kind == CabinLookKind::Diagram)
	{
		return;
	}
	if (!bFaced && Look.kind != CabinLookKind::Nameplate && Look.kind != CabinLookKind::Bezel &&
		Look.kind != CabinLookKind::Diagram && Look.kind != CabinLookKind::Window && Look.kind != CabinLookKind::Legend)
	{
		AddLabel(L, N, R, U, Look.kind == CabinLookKind::Oval ? H : FMath::Max(W, H), ToF(Look.label));
	}
	// The parts library first (art/cabin_parts); engine shapes below where a part isn't imported.
	auto Part = [&](const TCHAR* Name, double Size, const FLinearColor& C)
	{
		UStaticMesh* M = PartMesh(Name);
		if (M != nullptr)
		{
			Place(AddDecor(M, C), L, PartRotation(N, U), FVector(Size));
		}
		return M != nullptr;
	};
	bool bDone = false;
	switch (Look.kind)
	{
	case CabinLookKind::Knob: bDone = Part(TEXT("SM_Knob"), W, Color); break;
	case CabinLookKind::PointerKnob: bDone = Part(TEXT("SM_KnobPointer"), W, Color); break;
	case CabinLookKind::BarKnob: bDone = Part(TEXT("SM_KnobBar"), W, Color); break;
	case CabinLookKind::Trim: bDone = Part(TEXT("SM_Trim"), W, Color); break;
	case CabinLookKind::Jack: bDone = Part(TEXT("SM_Jack"), W, Color); break;
	case CabinLookKind::Socket: bDone = Part(TEXT("SM_Socket"), W, Color); break;
	case CabinLookKind::Latch: bDone = Part(TEXT("SM_Latch"), W, ChromeColor); break;
	case CabinLookKind::Lamp:
		bDone = PartMesh(TEXT("SM_LampLens")) != nullptr && Part(TEXT("SM_LampBezel"), 1.4 * W, ChromeColor) &&
				Part(TEXT("SM_LampLens"), 1.4 * W, Color * 0.5f);
		break;
	case CabinLookKind::LampButton:
	case CabinLookKind::Button:
		bDone = PartMesh(TEXT("SM_ButtonCap")) != nullptr && Part(TEXT("SM_ButtonRing"), W, LookColor(Look.ring, ChromeColor)) &&
				Part(TEXT("SM_ButtonCap"), W, Color);
		break;
	case CabinLookKind::Toggle:
		if (PartMesh(TEXT("SM_ToggleLever")) != nullptr && Part(TEXT("SM_ToggleBase"), W, ChromeColor))
		{
			// Shown thrown down (off), 3 cm long from the bushing's top.
			const double A = FMath::DegreesToRadians(-35.0);
			const FVector Lever = N * FMath::Cos(A) + U * FMath::Sin(A);
			Place(AddDecor(PartMesh(TEXT("SM_ToggleLever")), ChromeColor), L + N * (42.0 * W),
				FRotationMatrix::MakeFromZX(Lever, R).Rotator(), FVector(0.03));
			bDone = true;
		}
		break;
	default:
		break;
	}
	if (bDone)
	{
		return;
	}
	switch (Look.kind)
	{
	case CabinLookKind::Knob:
		Disc(W, W, FMath::Min(2.0, 60.0 * W), Color);
		break;
	case CabinLookKind::PointerKnob:
	case CabinLookKind::BarKnob:
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
		// A low mount: the buttons and lamps on it stand out of it.
		Disc(W, H, 0.3, Color);
		break;
	case CabinLookKind::Legend:
		// Black glass 1.2 cm proud with its letters, never lit (the lamps the console doesn't model).
		if (FMath::Abs(W - H) > 1e-6)
		{
			Box(L, W, H, 1.2, DarkColor);
		}
		else
		{
			Disc(W, W, 1.2, DarkColor);
		}
		Paint(AddLegend(L + N * 1.24, N, R, U, W, H, ToF(Look.label)), LegendUnlit);
		break;
	case CabinLookKind::Gauge:
	{
		// A dark rim, a light face (the look's colour, white by default) and a needle at rest, round or square.
		const FLinearColor Face = LookColor(Look.color.empty() ? std::string("white") : Look.color, DarkColor);
		// The vehicle's instruments have black faces with white markings and a chrome rim (rwd_mb3 9a33_31, 9a33_32).
		const bool bBlackFace = Look.color == "black";
		const FLinearColor Rim = bBlackFace ? ChromeColor * 0.5f : DarkColor;
		if (bFaced)
		{
			// The dial (face, scale, needle, hub) is printed in the panel's baked face (tools/cabin_textures.py):
			// only the rim stands proud, open in the middle.
			if (FMath::Abs(W - H) > 1e-6)
			{
				const double T = 0.08 * FMath::Min(W, H);
				Box(L + U * (50.0 * (H - T)), W, T, 0.8, Rim);
				Box(L - U * (50.0 * (H - T)), W, T, 0.8, Rim);
				Box(L + R * (50.0 * (W - T)), T, H, 0.8, Rim);
				Box(L - R * (50.0 * (W - T)), T, H, 0.8, Rim);
			}
			else if (!Part(TEXT("SM_GaugeBezel"), W, Rim))
			{
				Disc(W, W, 0.4, Rim);
			}
			else
			{
				Part(TEXT("SM_MeterHousing"), 1.16 * W, DarkColor);
			}
			bDone = true;
			break;
		}
		if (FMath::Abs(W - H) > 1e-6)
		{
			Box(L, W, H, 1.0, Rim);
			Box(L, 0.84 * W, 0.84 * H, 0.2, Face, 1.0);
		}
		else
		{
			Disc(W, W, 1.0, Rim);
			Disc(0.84 * W, 0.84 * W, 0.2, Face, 1.0);
		}
		Box(L + U * (12.0 * H) - R * (8.0 * W), 0.0025, 0.36 * H, 0.1, bBlackFace ? FLinearColor(0.75f, 0.75f, 0.7f) : DarkColor, 1.2);
		break;
	}
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
			const simcore::CabinLamp& LampDef = Layout.lamps[Item.LayoutIndex];
			// A flashing lamp (an overheated missile gyro) alternates twice a second while its condition holds.
			const bool bFlashing = simcore::CabinLayout::LampFlashing(LampDef, Console.panel, Console.equipment);
			const bool bLit = bFlashing ? FMath::Fmod(GetWorld()->GetTimeSeconds() * 2.0, 1.0) < 0.5
			                            : simcore::CabinLayout::LampLit(LampDef, Console.panel, Console.equipment);
			// The lamps are small incandescent bulbs behind coloured glass: the filament takes a few hundredths of a second
			// to come up and a little longer to go out, glowing orange as it dims (SAM3D's figures).
			const float Tau = bLit ? 0.035f : 0.09f;
			Item.Glow += ((bLit ? 1.0f : 0.0f) - Item.Glow) * (1.0f - FMath::Exp(-DeltaTime / Tau));
			const FLinearColor Lit = FMath::Lerp(Item.MovingColor * FLinearColor(1.0f, 0.45f, 0.2f), Item.MovingColor, Item.Glow) * Item.Glow;
			if (Item.Legend != nullptr)
			{
				const FLinearColor Letters = FMath::Lerp(LegendUnlit, Item.MovingColor, Item.Glow);
				Paint(Item.Legend, bHighlighted ? FMath::Lerp(Letters, HighlightColor, 0.4f) : Letters);
			}
			const FLinearColor Glass = Item.MovingColor * 0.04f + Lit * 0.96f;
			Paint(Item.Moving, bHighlighted ? FMath::Lerp(Glass, HighlightColor, 0.4f) : Glass);
			Paint(Item.Base, bHighlighted ? HighlightColor : Item.BaseColor);
			continue;
		}
		Item.PressedFor = FMath::Max(0.0f, Item.PressedFor - DeltaTime);
		AnimateControl(Sim, Item);
		// The drill master's next control pulses green (the pointer's highlight wins while it is on it).
		const bool bAdvised = !bHighlighted && !AdvisedControl.IsEmpty() && Item.ControlId == AdvisedControl;
		const float Pulse = bAdvised ? 0.35f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.0f) : 0.0f;
		const FLinearColor AdviceColor(0.2f, 1.0f, 0.35f);
		Paint(Item.Base, bHighlighted ? FMath::Lerp(Item.BaseColor, HighlightColor, 0.6f)
		                              : bAdvised ? FMath::Lerp(Item.BaseColor, AdviceColor, Pulse) : Item.BaseColor);
		Paint(Item.Moving, bHighlighted ? FMath::Lerp(Item.MovingColor, HighlightColor, 0.6f)
		                                : bAdvised ? FMath::Lerp(Item.MovingColor, AdviceColor, Pulse) : Item.MovingColor);
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
	if (Item.bParts && AnimatePart(Panel, Control, *Def, Position, Item))
	{
		return;
	}
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

bool USimCabinComponent::AnimatePart(const simcore::ControlPanel& Panel, const simcore::CabinControl& Control,
	const simcore::ControlDef& Def, int32 Position, FSimCabinItem& Item)
{
	const FVector& L = Item.Location;
	const FVector& N = Item.Normal;
	const bool bCap = Item.LookKind == static_cast<uint8>(simcore::CabinLookKind::Button) ||
					  Item.LookKind == static_cast<uint8>(simcore::CabinLookKind::LampButton);
	switch (Control.style)
	{
	case simcore::CabinStyle::Toggle:
		if (!bCap)
		{
			// The lever from the bushing's top, thrown 35 degrees up the face for "on", down for "off"; 3 cm long.
			const double Face = simcore::CabinLayout::PositionAngleDeg(Control.style, Def.positions, Position);
			const double A = FMath::DegreesToRadians(Face < 90.0 ? 35.0 : -35.0);
			const FVector Lever = N * FMath::Cos(A) + Item.Up * FMath::Sin(A);
			const double Bushing = 100.0 * 0.42 * (Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.016);
			Place(Item.Moving, L + N * Bushing, FRotationMatrix::MakeFromZX(Lever, Item.Right).Rotator(), FVector(0.03));
			return true;
		}
		[[fallthrough]];
	case simcore::CabinStyle::Button:
	{
		// The cap in its ring, pushed in while pressed (or, for a switch drawn as a button, while on).
		const bool bPressed = Position == 1 || Item.PressedFor > 0.0f;
		const double W = Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.03;
		Place(Item.Moving, L - N * (bPressed ? 10.0 * W : 0.0), PartRotation(N, Item.Up), FVector(W));
		return true;
	}
	case simcore::CabinStyle::Rotary:
	{
		const FVector Dir = FaceDirection(Item, simcore::CabinLayout::PositionAngleDeg(Control.style, Def.positions, Position));
		const double D = Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.026;
		Place(Item.Moving, L, PartRotation(N, Dir), FVector(D));
		return true;
	}
	case simcore::CabinStyle::Wheel:
	{
		const double Turns = Panel.GetValue(Control.control_id) / Control.units_per_turn;
		const FVector Dir = FaceDirection(Item, 360.0 * Turns);
		const double D = Item.LookWidthM > 0.0 ? Item.LookWidthM : 0.12;
		Place(Item.Moving, L + N * 0.5, PartRotation(N, Dir), FVector(D));
		// The crank handle on the rim, 4.5 cm long.
		Place(Item.Pointer, L + N * (0.5 + 100.0 * 0.22 * D) + Dir * (100.0 * 0.4 * D), PartRotation(N, Dir), FVector(0.045));
		return true;
	}
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------------------------- interaction

int32 USimCabinComponent::GetNumStations() const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	return Layout != nullptr ? static_cast<int32>(Layout->stations.size()) : 0;
}

FString USimCabinComponent::GetStationId(int32 Station) const
{
	const USimWorldSubsystem* Sim = GetSim();
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	if (Layout == nullptr || Station < 0 || Station >= static_cast<int32>(Layout->stations.size()))
	{
		return FString();
	}
	return ToF(Layout->stations[Station].id);
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
