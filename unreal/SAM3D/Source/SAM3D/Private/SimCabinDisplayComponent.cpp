#include "SimCabinDisplayComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "GlobalRenderResources.h"
#include "SimCabinComponent.h"
#include "SimOsaVehicle.h"
#include "simcore/readouts.hpp"
#include "SimWorldSubsystem.h"

namespace
{
	// The search and elevation tubes are seen through an amber filter and use a long-persistence phosphor: yellow-orange
	// light on a dark amber face, not the saturated green of a modern display. The range indicator and TV are plain
	// tubes, a pale yellow-green and a bluish white.
	const FLinearColor Phosphor(1.0f, 0.78f, 0.22f);
	const FLinearColor TraceGreen(0.55f, 0.95f, 0.5f);
	const FLinearColor Amber(1.0f, 0.65f, 0.15f);
	// The tubes of the search and elevation indicators sit behind an amber filter, which is all that shows while
	// they are off (the museum photographs, opisybroni 54334/54539); the TOV monitor's dead glass is a pale grey-green.
	const FLinearColor AmberGlass(0.8f, 0.3f, 0.02f);
	// The TV's dead glass is dark (the user's cabin photo, crop_user_front; the pale glass in the daylight museum photos
	// is the windows reflected in it, which the CRT material adds as a soft highlight).
	const FLinearColor MonitorGlass(0.04f, 0.045f, 0.042f);

	// The displays' shapes go to the canvas as one list of triangles. Each canvas item costs the render thread a draw of
	// its own whatever its size, and the scopes were thousands of short lines and little discs a picture: some 100 ms a
	// frame in the cabin. Lines, rings, discs and dots are gathered here and drawn as one item when something else (text,
	// a texture, a box) comes, or the picture ends, so they keep their order.
	struct FShapeBatch
	{
		UCanvas* Canvas = nullptr;
		TArray<FCanvasUVTri> Tris;
	};
	FShapeBatch GShapes;

	void FlushShapes()
	{
		if (GShapes.Canvas != nullptr && GShapes.Tris.Num() > 0)
		{
			FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture);
			Item.TriangleList = MoveTemp(GShapes.Tris);
			Item.BlendMode = SE_BLEND_Translucent;  // as the canvas lines were: the colours carry their own alpha
			GShapes.Canvas->DrawItem(Item);
		}
		GShapes.Tris.Reset();
		GShapes.Canvas = nullptr;
	}

	void Tri(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		if (GShapes.Canvas != Canvas)
		{
			FlushShapes();
			GShapes.Canvas = Canvas;
		}
		FCanvasUVTri T;
		T.V0_Pos = A;
		T.V1_Pos = B;
		T.V2_Pos = C;
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		GShapes.Tris.Add(T);
	}

	// A line on a display, as a thin quad (a hairline one pixel wide).
	void Line(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
	{
		const FVector2D Along = B - A;
		const double Length = Along.Size();
		if (Length < 1e-6)
		{
			return;
		}
		const FVector2D Side = FVector2D(-Along.Y, Along.X) / Length * (0.5 * FMath::Max(1.0f, Thickness));
		Tri(Canvas, A - Side, A + Side, B + Side, Color);
		Tri(Canvas, A - Side, B + Side, B - Side, Color);
	}

	// A beam on a phosphor: the bright core and a faint wide halo round it.
	void Beam(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
	{
		Line(Canvas, A, B, Thickness * 3.0f, Color * 0.16f);
		Line(Canvas, A, B, Thickness, Color);
	}

	// Screen point at Angle (clockwise from up) and Radius from Centre; screen Y grows downward.
	FVector2D Polar(const FVector2D& Centre, double AngleRad, double Radius)
	{
		return Centre + FVector2D(FMath::Sin(AngleRad), -FMath::Cos(AngleRad)) * Radius;
	}

	// A ring.
	void Circle(UCanvas* Canvas, const FVector2D& Centre, double Radius, const FLinearColor& Color, float Thickness = 1.0f)
	{
		constexpr int32 Segments = 96;
		const double In = FMath::Max(0.0, Radius - 0.5 * Thickness);
		const double Out = Radius + 0.5 * Thickness;
		for (int32 I = 0; I < Segments; ++I)
		{
			const double A0 = UE_DOUBLE_TWO_PI * I / Segments;
			const double A1 = UE_DOUBLE_TWO_PI * (I + 1) / Segments;
			const FVector2D I0 = Polar(Centre, A0, In), I1 = Polar(Centre, A1, In), O0 = Polar(Centre, A0, Out), O1 = Polar(Centre, A1, Out);
			Tri(Canvas, I0, O0, O1, Color);
			Tri(Canvas, I0, O1, I1, Color);
		}
	}

	// A filled polygon (48 sides: a disc; 4: a dot).
	void Polygon(UCanvas* Canvas, const FVector2D& Centre, double Radius, int32 Sides, const FLinearColor& Color)
	{
		for (int32 I = 0; I < Sides; ++I)
		{
			Tri(Canvas, Centre, Polar(Centre, UE_DOUBLE_TWO_PI * I / Sides, Radius), Polar(Centre, UE_DOUBLE_TWO_PI * (I + 1) / Sides, Radius),
				Color);
		}
	}

	void Disc(UCanvas* Canvas, const FVector2D& Centre, double Radius, const FLinearColor& Color)
	{
		Polygon(Canvas, Centre, Radius, 48, Color);
	}

	// A sector of the tube from angle A0 to A1 (clockwise from up), Inner to Outer radius.
	void Wedge(UCanvas* Canvas, const FVector2D& Centre, double A0, double A1, double Inner, double Outer, const FLinearColor& Color)
	{
		const FVector2D I0 = Polar(Centre, A0, Inner), I1 = Polar(Centre, A1, Inner), O0 = Polar(Centre, A0, Outer), O1 = Polar(Centre, A1, Outer);
		Tri(Canvas, I0, O0, O1, Color);
		Tri(Canvas, I0, O1, I1, Color);
	}

	// A trace on a phosphor as the eye sees it: a wide faint bloom, a softer shoulder and the bright core (a long
	// afterglow screen's line is never a hard edge).
	void Glow(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
	{
		Line(Canvas, A, B, Thickness * 5.0f, Color * 0.06f);
		Line(Canvas, A, B, Thickness * 2.2f, Color * 0.22f);
		Line(Canvas, A, B, Thickness, Color);
	}

	// The colour of a В screen's light as it decays (radiolamp_elt: a white flash, a long yellow afterglow), seen through
	// the amber filter: pale yellow fresh, sinking to amber, at brightness Fresh (1 just struck, 0 gone).
	FLinearColor SearchGlow(double Fresh)
	{
		const float F = static_cast<float>(FMath::Clamp(Fresh, 0.0, 1.0));
		const FLinearColor Flash(1.0f, 0.95f, 0.72f);
		return FLinearColor::LerpUsingHSV(Phosphor * 0.8f, Flash, F * F) * F;
	}

	void Text(UCanvas* Canvas, const FString& S, const FVector2D& At, const FLinearColor& Color, float Scale, bool bCentre = true)
	{
		FlushShapes();
		UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
		Canvas->K2_DrawText(Font, S, At, FVector2D(Scale, Scale), Color, 0.0f, FLinearColor::Transparent, FVector2D::ZeroVector,
			bCentre, bCentre);
	}

	// The amber filter over a dead tube: darker towards the rim, the filter's mesh as faint vertical wires (the museum
	// photographs of the Osa's tubes; the IT-45's in rwd_mb3 is the same glass).
	void DeadFilter(UCanvas* Canvas, const FVector2D& C, double R)
	{
		constexpr int32 Steps = 14;
		for (int32 I = 0; I < Steps; ++I)
		{
			const float T = static_cast<float>(I) / (Steps - 1);
			Disc(Canvas, C, R * (1.0 - 0.45 * T), AmberGlass * (0.55f + 0.45f * FMath::Sqrt(T)));
		}
		for (int32 I = -3; I <= 3; ++I)
		{
			const double X = C.X + I * 0.085 * R;
			const double Half = FMath::Sqrt(FMath::Max(0.0, R * R - (X - C.X) * (X - C.X))) * 0.96;
			Line(Canvas, FVector2D(X, C.Y - Half), FVector2D(X, C.Y + Half), 1.0f, AmberGlass * 0.6f);
		}
	}

	// A number for a scale label: no trailing zeros.
	FString Label(double Value)
	{
		return FString::SanitizeFloat(FMath::RoundToDouble(Value * 10.0) / 10.0, 0);
	}
}

USimCabinDisplayComponent::USimCabinDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USimWorldSubsystem* USimCabinDisplayComponent::GetSim() const
{
	UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
}

void USimCabinDisplayComponent::Setup(const USimWorldSubsystem* Sim, int32 InLayoutIndex, const FVector& Centre,
	const FVector& Normal, const FVector& Right, const FVector& Up, UMaterialInterface* ScreenMaterialBase,
	UStaticMesh* Cube, UMaterialInterface* ShapeMaterial)
{
	LayoutIndex = InLayoutIndex;
	const simcore::CabinDisplay& Display = Sim->GetCabinLayout()->displays[LayoutIndex];
	Kind = Display.kind;
	Noise.Initialize(LayoutIndex + 1);
	if (Kind == simcore::CabinDisplayKind::Ppi)
	{
		Scope = MakeUnique<simcore::SearchScope>(Display.sweep_period_s, Display.afterglow_s);
	}
	if (Kind == simcore::CabinDisplayKind::ElevationScope)
	{
		ElevScope = MakeUnique<simcore::ElevationScope>(Display.afterglow_s, Sim->GetScenarioRunner() != nullptr
			? Sim->GetScenarioRunner()->Console().track.beam_width_deg : 1.0);
	}
	const double WidthCm = Display.width_m * 100.0;
	const double HeightCm = Display.height_m * 100.0;
	// Cathode-ray tubes and dials are round (the search indicator OP81-16M, OS81-10M, the βс / εс dials in the
	// samsim_panels photos); the TV monitor, range indicator and meters rectangular.
	const bool bRound = Kind == simcore::CabinDisplayKind::Ppi || Kind == simcore::CabinDisplayKind::ElevationScope ||
		Kind == simcore::CabinDisplayKind::Dial || Kind == simcore::CabinDisplayKind::AspdScope;
	UStaticMesh* Cylinder = bRound ? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")) : nullptr;

	// Bezel: a dark box (or ring) just behind the screen, a little larger. (Not for the readout strip: its frame is
	// baked into the panel face.)
	if (Cube != nullptr && Kind != simcore::CabinDisplayKind::Counters && Kind != simcore::CabinDisplayKind::PlotBoard &&
		Kind != simcore::CabinDisplayKind::StatusBoard)
	{
		UStaticMeshComponent* Bezel = NewObject<UStaticMeshComponent>(GetOwner());
		Bezel->SetStaticMesh(Cylinder != nullptr ? Cylinder : Cube);
		Bezel->SetupAttachment(GetAttachParent());
		Bezel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bezel->SetCastShadow(false);
		USimCabinComponent::UseCabinLighting(Bezel);
		Bezel->RegisterComponent();
		if (Cylinder != nullptr)
		{
			// The cylinder's axis out of the face.
			Bezel->SetRelativeLocationAndRotation(Centre + Normal * 0.6, FRotationMatrix::MakeFromZX(Normal, Up).Rotator());
			Bezel->SetRelativeScale3D(FVector(Display.height_m + 0.012, Display.width_m + 0.012, 0.012));
		}
		else
		{
			Bezel->SetRelativeLocationAndRotation(Centre + Normal * 0.5, FRotationMatrix::MakeFromXZ(Normal, Up).Rotator());
			Bezel->SetRelativeScale3D(FVector(0.012, Display.width_m + 0.02, Display.height_m + 0.02));
		}
		if (ShapeMaterial != nullptr)
		{
			Bezel->CreateDynamicMaterialInstance(0, ShapeMaterial)->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.01f, 0.01f));
		}
		Parts.Add(Bezel);
	}

	// The screen, 1.3 cm out from the panel with texture (0,0) at the operator's top left: a quad, or a disc for a
	// round tube (a fan, the texture mapped flat across it). Both windings, so it shows whichever way the material
	// culls.
	Screen = NewObject<UProceduralMeshComponent>(GetOwner());
	Screen->SetupAttachment(GetAttachParent());
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCastShadow(false);
	USimCabinComponent::UseCabinLighting(Screen);
	Screen->RegisterComponent();
	const FVector C = Centre + Normal * 1.3;
	const FVector Hr = Right * (0.5 * WidthCm);
	const FVector Hu = Up * (0.5 * HeightCm);
	FaceCentre = C;
	FaceNormal = Normal;
	FaceRight = Right;
	FaceUp = Up;
	FaceWidthCm = WidthCm;
	FaceHeightCm = HeightCm;
	TArray<FVector> Vertices = {C - Hr + Hu, C + Hr + Hu, C - Hr - Hu, C + Hr - Hu};
	TArray<FVector2D> Uv = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1)};
	TArray<int32> Triangles = {0, 1, 2, 2, 1, 3, 0, 2, 1, 2, 3, 1};
	if (bRound)
	{
		constexpr int32 Segments = 64;
		Vertices = {C};
		Uv = {FVector2D(0.5, 0.5)};
		Triangles.Reset();
		for (int32 I = 0; I < Segments; ++I)
		{
			const double A = UE_DOUBLE_TWO_PI * I / Segments;
			const double X = FMath::Sin(A);
			const double Y = FMath::Cos(A);
			Vertices.Add(C + Hr * X + Hu * Y);
			Uv.Add(FVector2D(0.5 + 0.5 * X, 0.5 - 0.5 * Y));
			const int32 Next = 1 + (I + 1) % Segments;
			Triangles.Append({0, 1 + I, Next, 0, Next, 1 + I});
		}
	}
	TArray<FVector> Normals;
	Normals.Init(Normal, Vertices.Num());
	Screen->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, Uv, {}, {}, false);

	const double Long = FMath::Max(WidthCm, HeightCm);
	// The TV picture is coarse (a 1970s monitor's raster), the others are drawn sharp and softened by the material; the
	// IT-45's 40 cm face finer, for its symbols, the ДХ's little window coarser.
	const int32 Pixels = Kind == simcore::CabinDisplayKind::Tv ? 256
		: Kind == simcore::CabinDisplayKind::AspdScope ? 768
		: Kind == simcore::CabinDisplayKind::KeypadWindow ? 256 : PixelsPerLongSide;
	const int32 W = FMath::Max(16, FMath::RoundToInt32(Pixels * WidthCm / Long));
	const int32 H = FMath::Max(16, FMath::RoundToInt32(Pixels * HeightCm / Long));
	Target = UKismetRenderingLibrary::CreateRenderTarget2D(this, W, H, RTF_RGBA8, FLinearColor::Black);
	// The plotting board is glass, not a tube: its own translucent material (tools/ue_make_glass_material.py), where the
	// etched lines and the grease pencil lift the opacity of a sheet that is otherwise all but clear.
	if (Kind == simcore::CabinDisplayKind::PlotBoard)
	{
		if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/SAM3D/Cabin/Materials/M_CabinGlassBoard.M_CabinGlassBoard"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			ScreenMaterialBase = Glass;
		}
	}
	if (ScreenMaterialBase != nullptr && Target != nullptr)
	{
		ScreenMaterial = UMaterialInstanceDynamic::Create(ScreenMaterialBase, this);
		ScreenMaterial->SetTextureParameterValue(TEXT("SlateUI"), Target);
		// The unlit glass is dimmed by the cabin camera's fixed exposure like everything else, but a lit tube is the
		// brightest thing in the compartment, and the render target stops at 1: the gain lifts it back. The TV's
		// picture is already a daylight image, so it needs less; the plotter's map is lit only by the cabin.
		const bool bTv = Kind == simcore::CabinDisplayKind::Tv;
		const float Gain = bTv ? 4.0f
			: Kind == simcore::CabinDisplayKind::Counters ? 10.0f
			: Kind == simcore::CabinDisplayKind::PlotBoard ? 2.5f
			: Kind == simcore::CabinDisplayKind::StatusBoard ? 4.0f : 6.0f;
		ScreenMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"),
			bTv ? FLinearColor(0.93f * Gain, 0.97f * Gain, Gain, 1.0f) : FLinearColor(Gain, Gain, Gain, 1.0f));
		// The tube's character (M_CabinCRT): curvature, halo (amount, radius), raster lines (strength, count), noise
		// (strength, grain), flicker, vignette, reflection of the cabin lamps. Dials and meters are flat glass.
		struct FCrt { float Curve, Glow, GlowR, Scan, Lines, Noise, Grain, Flick, Vig, Refl; };
		FCrt Crt{0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.25f};
		switch (Kind)
		{
		case simcore::CabinDisplayKind::Ppi:
		case simcore::CabinDisplayKind::ElevationScope: Crt = {0.05f, 0.6f, 0.009f, 0.0f, 100.0f, 0.05f, 120.0f, 0.03f, 0.4f, 0.55f}; break;
		case simcore::CabinDisplayKind::RangeScope: Crt = {0.07f, 0.5f, 0.007f, 0.15f, 200.0f, 0.07f, 140.0f, 0.03f, 0.45f, 0.5f}; break;
		case simcore::CabinDisplayKind::Tv: Crt = {0.08f, 0.3f, 0.004f, 0.4f, 150.0f, 0.16f, 220.0f, 0.05f, 0.5f, 0.8f}; break;
		case simcore::CabinDisplayKind::AspdScope: Crt = {0.06f, 0.5f, 0.0022f, 0.0f, 100.0f, 0.03f, 160.0f, 0.02f, 0.45f, 0.6f}; break;
		case simcore::CabinDisplayKind::KeypadWindow: Crt = {0.0f, 0.7f, 0.01f, 0.0f, 100.0f, 0.0f, 100.0f, 0.0f, 0.25f, 0.45f}; break;
		case simcore::CabinDisplayKind::StatusBoard: Crt = {0.0f, 0.35f, 0.006f, 0.0f, 100.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.2f}; break;
		case simcore::CabinDisplayKind::PlotBoard: Crt = {0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 100.0f, 0.0f, 0.1f, 0.4f}; break;
		default: break;
		}
		ScreenMaterial->SetScalarParameterValue(TEXT("Curvature"), Crt.Curve);
		ScreenMaterial->SetScalarParameterValue(TEXT("GlowAmount"), Crt.Glow);
		ScreenMaterial->SetScalarParameterValue(TEXT("GlowRadius"), Crt.GlowR);
		ScreenMaterial->SetScalarParameterValue(TEXT("ScanStrength"), Crt.Scan);
		ScreenMaterial->SetScalarParameterValue(TEXT("ScanLines"), Crt.Lines);
		ScreenMaterial->SetScalarParameterValue(TEXT("NoiseStrength"), Crt.Noise);
		ScreenMaterial->SetScalarParameterValue(TEXT("NoiseScale"), Crt.Grain);
		ScreenMaterial->SetScalarParameterValue(TEXT("FlickerStrength"), Crt.Flick);
		ScreenMaterial->SetScalarParameterValue(TEXT("VignetteStrength"), Crt.Vig);
		ScreenMaterial->SetScalarParameterValue(TEXT("ReflectStrength"), Crt.Refl);
		ScreenMaterial->SetVectorParameterValue(TEXT("BackColor"), FLinearColor::Black);
		ScreenMaterial->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
		Screen->SetMaterial(0, ScreenMaterial);
	}

	if (Kind == simcore::CabinDisplayKind::Tv)
	{
		// Configured before registering, so it doesn't capture a frame on its own while the TV is off.
		Capture = NewObject<USceneCaptureComponent2D>(GetOwner());
		Capture->TextureTarget = Target;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		// A black-and-white picture, as on the TOV monitor.
		Capture->PostProcessSettings.bOverride_ColorSaturation = true;
		Capture->PostProcessSettings.ColorSaturation = FVector4(0.0, 0.0, 0.0, 1.0);
		// Crushed shadows and bright highlights, as on a worn video tube.
		Capture->PostProcessSettings.bOverride_ColorContrast = true;
		Capture->PostProcessSettings.ColorContrast = FVector4(1.35, 1.35, 1.35, 1.0);
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = -1.2f;
		Capture->PostProcessBlendWeight = 1.0f;
		Capture->HideActorComponents(GetOwner());
		Capture->RegisterComponent();
		// Crosshair over the picture.
		for (int32 I = 0; I < 2 && Cube != nullptr; ++I)
		{
			UStaticMeshComponent* Line = NewObject<UStaticMeshComponent>(GetOwner());
			Line->SetStaticMesh(Cube);
			Line->SetupAttachment(GetAttachParent());
			Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Line->SetCastShadow(false);
			USimCabinComponent::UseCabinLighting(Line);
			Line->RegisterComponent();
			Line->SetRelativeLocationAndRotation(Centre + Normal * 1.4, FRotationMatrix::MakeFromXZ(Normal, Up).Rotator());
			Line->SetRelativeScale3D(I == 0 ? FVector(0.001, 0.4 * Display.width_m, 0.0012) : FVector(0.001, 0.0012, 0.4 * Display.height_m));
			if (ShapeMaterial != nullptr)
			{
				// Black reads against the bright sky the camera mostly sees.
				Line->CreateDynamicMaterialInstance(0, ShapeMaterial)->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
			}
			Line->SetVisibility(false);
			Parts.Add(Line);
		}
	}
}

void USimCabinDisplayComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part != nullptr)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	if (Screen != nullptr)
	{
		Screen->DestroyComponent();
	}
	if (Capture != nullptr)
	{
		Capture->DestroyComponent();
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

namespace
{
TAutoConsoleVariable<float> CVarCabinDisplayHz(TEXT("sam3d.CabinDisplayHz"), 30.0f,
	TEXT("How many times a second the cabin's scopes, dials and meters are redrawn while the camera is in the cabin (each is ")
	TEXT("hundreds of canvas lines on the render thread). Outside the cabin they aren't drawn at all."),
	ECVF_Default);
TAutoConsoleVariable<int32> CVarSkipKinds(TEXT("sam3d.CabinDisplaySkipKinds"), 0,
	TEXT("Debugging: a bit mask of cabin display kinds not to redraw (1 search scope, 2 range scope, 4 elevation scope, 8 TV, ")
	TEXT("16 dial, 32 meter, 64 counters, 128 IT-45, 256 status board, 512 plot board, 1024 keypad window)."),
	ECVF_Default);
TAutoConsoleVariable<float> CVarTvHz(TEXT("sam3d.TvHz"), 15.0f,
	TEXT("How many times a second the TOV monitor films the world while it is on and the camera is in the cabin (each is a ")
	TEXT("second rendering of the scene)."),
	ECVF_Default);
}

bool USimCabinDisplayComponent::IsSeen() const
{
	const ASimOsaVehicle* Vehicle = Cast<ASimOsaVehicle>(GetOwner());
	return Vehicle == nullptr || Vehicle->IsInCabin();
}

void USimCabinDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RedrawSecondsLeft -= DeltaTime;
	DeltaSinceDraw += DeltaTime;
	const USimWorldSubsystem* Sim = GetSim();
	simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetScenarioRunner() : nullptr;
	const simcore::CabinLayout* Layout = Sim != nullptr ? Sim->GetCabinLayout() : nullptr;
	if (Runner == nullptr || Layout == nullptr || Target == nullptr || LayoutIndex >= static_cast<int32>(Layout->displays.size()))
	{
		return;
	}
	const simcore::CabinDisplay& Display = Layout->displays[LayoutIndex];
	const simcore::ConsoleData& Console = Runner->Console();
	const bool bOn = simcore::CabinLayout::DisplayOn(Display, Console.panel, Console.equipment);
	// A network client never steps its own sim: its scopes come from the knowledge stream, and its sweep runs on the
	// frame clock (the stream's views arrive a few times a second).
	const bool bMirror = Sim->IsMirrorOnly();
	const simcore::em::ViewScope* Mirrored = bMirror ? Sim->GetMirroredScope() : nullptr;
	Feed = Mirrored != nullptr ? simcore::FromViewScope(*Mirrored)
	                           : simcore::ScopeFeedOf(Runner->Engagement(), Runner->GetLauncher(), Runner->Time());
	auto StepSeconds = [&]() {
		if (bMirror)
		{
			return FMath::Clamp(static_cast<double>(DeltaTime) * Sim->GetTimeScale(), 0.0, 1.0);
		}
		const double Now = Runner->Time();
		const double Dt = LastSimTimeSeconds < 0.0 ? 0.0 : FMath::Clamp(Now - LastSimTimeSeconds, 0.0, 1.0);
		LastSimTimeSeconds = Now;
		return Dt;
	};

	if (Kind == simcore::CabinDisplayKind::Tv)
	{
		UpdateTv(Sim, *Runner, bOn);
		return;
	}
	if (Kind == simcore::CabinDisplayKind::Ppi)
	{
		// The sweep follows simulated time, so it stops when the sim is paused.
		const double Dt = StepSeconds();
		const bool bRotating = Console.has_sensors && Console.equipment.IsReady(Console.search.antenna);
		// With the EM layer the sweep is the antenna's own (roadmap E3): an echo paints as the beam passes it.
		double AntennaRad = -1.0;
		if (Feed.search_antenna_az_deg > simcore::EngagementModel::kNoAntenna + 1.0)
		{
			AntennaRad = FMath::Fmod(FMath::DegreesToRadians(Feed.search_antenna_az_deg) + UE_DOUBLE_TWO_PI, UE_DOUBLE_TWO_PI);
		}
		Scope->Update(Dt, bRotating, Feed.search, Feed.iff, Feed.strobes, AntennaRad);
	}
	if (Kind == simcore::CabinDisplayKind::AspdScope)
	{
		// The ФР block's sweep, on the sim's clock (a client's on the frame's).
		const double Dt = StepSeconds();
		if (bOn)
		{
			AspdSweepRad += Dt * UE_DOUBLE_TWO_PI / FMath::Max(1.0, Display.sweep_period_s);
			while (AspdSweepRad >= UE_DOUBLE_TWO_PI)
			{
				AspdSweepRad -= UE_DOUBLE_TWO_PI;
				++AspdTurns;
			}
		}
	}
	if (Kind == simcore::CabinDisplayKind::ElevationScope && Console.has_sensors)
	{
		const double Dt = StepSeconds();
		// The engagement model scans the antenna (target designation) or points it; the indicator shows the beam it
		// searches and paints what the antenna's beam passed over.
		const simcore::TrackSensorDef& Track = Console.track;
		ElevRaster = Feed.elevation_beam;
		ElevScope->Update(Dt, bOn && Console.equipment.IsReady(Track.transmitter), Feed.swept_low_deg,
			Feed.swept_high_deg, Console.panel.GetValue(Track.range_wheel), Display.range_window_m,
			Feed.track_returns);
	}

	// The state above runs every frame (the sweeps and afterglow keep time); the picture only when it is seen, at the set
	// rate.
	if (!IsSeen() || RedrawSecondsLeft > 0.0f || (CVarSkipKinds.GetValueOnGameThread() & (1 << static_cast<int32>(Kind))) != 0)
	{
		return;
	}
	RedrawSecondsLeft = 1.0f / FMath::Max(1.0f, CVarCabinDisplayHz.GetValueOnGameThread());
	// A picture that hasn't changed since it was drawn stays as it is.
	uint32 Key = 0;
	if (ContentKey(Display, Console, bOn, Key))
	{
		if (bDrawnKeyValid && Key == DrawnKey)
		{
			return;
		}
		DrawnKey = Key;
		bDrawnKeyValid = true;
	}
	else
	{
		bDrawnKeyValid = false;
	}
	if (Kind == simcore::CabinDisplayKind::Ppi && FaceTarget == nullptr)
	{
		DrawPpiFace(FVector2D(Target->SizeX, Target->SizeY));
	}
	const float DrawDelta = DeltaSinceDraw;
	DeltaSinceDraw = 0.0f;
	// The range indicator's dead glass is pale like the TV's (opisybroni 54539); the tubes and dials paint their own.
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Target, Kind == simcore::CabinDisplayKind::RangeScope && !bOn ? MonitorGlass : FLinearColor::Black);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Target, Canvas, Size, Context);
	if (Canvas != nullptr)
	{
		switch (Kind)
		{
		case simcore::CabinDisplayKind::Ppi:
			DrawPpi(Canvas, Size, Display, *Runner, bOn);
			break;
		case simcore::CabinDisplayKind::RangeScope:
			DrawRangeScope(Canvas, Size, *Runner, bOn);
			break;
		case simcore::CabinDisplayKind::ElevationScope:
			DrawElevationScope(Canvas, Size, Display, *Runner, bOn);
			break;
		case simcore::CabinDisplayKind::Dial:
			DrawDial(Canvas, Size, Display, Console, bOn, simcore::CabinLayout::DisplayLit(Display, Console.panel, Console.equipment));
			break;
		case simcore::CabinDisplayKind::Meter:
			DrawMeter(Canvas, Size, Display, Console, bOn);
			break;
		case simcore::CabinDisplayKind::Counters:
			DrawCounters(Canvas, Size, Display, *Runner, bOn);
			break;
		case simcore::CabinDisplayKind::AspdScope:
			DrawAspdScope(Canvas, Size, Display, Sim->GetPostFeed(), bOn);
			break;
		case simcore::CabinDisplayKind::StatusBoard:
			DrawStatusBoard(Canvas, Size, Sim->GetPostFeed(), bOn, DrawDelta);
			break;
		case simcore::CabinDisplayKind::PlotBoard:
			DrawPlotBoard(Canvas, Size, Sim->GetPostFeed(), bOn);
			break;
		case simcore::CabinDisplayKind::KeypadWindow:
			DrawKeypadWindow(Canvas, Size, Sim->GetPostFeed(), bOn);
			break;
		case simcore::CabinDisplayKind::Tv:
			break;
		}
	}
	FlushShapes();
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

bool USimCabinDisplayComponent::ContentKey(const simcore::CabinDisplay& Display, const simcore::ConsoleData& Console, bool bOn, uint32& Key) const
{
	Key = GetTypeHash(bOn);
	switch (Kind)
	{
	case simcore::CabinDisplayKind::Dial:
		Key = HashCombine(Key, GetTypeHash(simcore::CabinLayout::DisplayLit(Display, Console.panel, Console.equipment)));
		Key = HashCombine(Key, GetTypeHash(FMath::RoundToInt64(Console.panel.GetValue(Display.control) * 100.0)));
		return true;
	case simcore::CabinDisplayKind::Meter:
		Key = HashCombine(Key, GetTypeHash(FMath::RoundToInt64(simcore::MeterValue(Display, Console) * 1000.0)));
		return true;
	case simcore::CabinDisplayKind::Ppi:
		// Off: the dead tube behind its filter, with the scale round it.
		return !bOn;
	case simcore::CabinDisplayKind::RangeScope:
	case simcore::CabinDisplayKind::ElevationScope:
		return !bOn;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------------------------- search scope

void USimCabinDisplayComponent::DrawPpiFace(const FVector2D& Size)
{
	FaceTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, FMath::RoundToInt32(Size.X), FMath::RoundToInt32(Size.Y), RTF_RGBA8,
		FLinearColor::Black);
	if (FaceTarget == nullptr)
	{
		return;
	}
	UCanvas* Canvas = nullptr;
	FVector2D FaceSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, FaceTarget, Canvas, FaceSize, Context);
	if (Canvas != nullptr)
	{
		const FVector2D C = FaceSize * 0.5;
		const double S = FMath::Min(FaceSize.X, FaceSize.Y);
		// The azimuth scale around the tube, as on OP81-16M (samsim_panels): 0 to 59 clockwise from the top (hundreds of
		// thousandths, 6000 to the circle), light marks on a dark ring, with a tick every half division.
		const FLinearColor ScaleInk(0.32f, 0.32f, 0.3f);
		Disc(Canvas, C, 0.5 * S, FLinearColor(0.012f, 0.012f, 0.012f));
		for (int32 I = 0; I < 120; ++I)
		{
			const double A = UE_DOUBLE_TWO_PI * I / 120.0;
			Line(Canvas, Polar(C, A, 0.437 * S), Polar(C, A, (I % 2 == 0 ? 0.452 : 0.446) * S), 1.0f, ScaleInk);
		}
		for (int32 I = 0; I < 60; ++I)
		{
			Text(Canvas, FString::FromInt(I), Polar(C, UE_DOUBLE_TWO_PI * I / 60.0, 0.474 * S), ScaleInk, 0.8f);
		}
	}
	FlushShapes();
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void USimCabinDisplayComponent::DrawPpi(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	simcore::ScenarioRunner& Runner, bool bOn)
{
	const FVector2D C = Size * 0.5;
	const double S = FMath::Min(Size.X, Size.Y);
	const double R = 0.43 * S;
	// The fixed face (the azimuth scale), drawn once (DrawPpiFace).
	if (FaceTarget != nullptr)
	{
		FlushShapes();
		Canvas->K2_DrawTexture(FaceTarget, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White,
			BLEND_Opaque);
	}
	if (!bOn)
	{
		DeadFilter(Canvas, C, R);
		return;
	}
	const simcore::ConsoleData& Console = Runner.Console();
	const simcore::SearchSensorDef& Search = Console.search;
	Disc(Canvas, C, R, AmberGlass * 0.12f);   // the dark tube seen through the amber filter

	// Displayed range window from the ДАЛЬНОСТЬ selector; the 10-45 km scale starts at the centre with 10 km.
	double MinM = 0.0;
	double MaxM = 45000.0;
	const int32 ScaleIndex = Console.panel.Get(Search.range_control);
	if (ScaleIndex >= 0 && ScaleIndex < static_cast<int32>(Search.range_scales_m.size()))
	{
		MinM = Search.range_scales_m[ScaleIndex].first;
		MaxM = Search.range_scales_m[ScaleIndex].second;
	}
	const double Span = FMath::Max(1.0, MaxM - MinM);
	auto RadiusOf = [&](double RangeM) { return (RangeM - MinM) / Span * R; };

	// The sweep, counted round: everything it paints fades from when it passed.
	const bool bTurning = Console.equipment.IsReady(Search.antenna);
	const double Sweep = Scope->SweepAzimuthRad();
	if (bPpiSweepKnown)
	{
		double Step = FMath::Fmod(Sweep - PpiLastSweepRad, UE_DOUBLE_TWO_PI);
		if (Step < 0.0)
		{
			Step += UE_DOUBLE_TWO_PI;
		}
		PpiTurns += Step / UE_DOUBLE_TWO_PI;
	}
	PpiLastSweepRad = Sweep;
	bPpiSweepKnown = true;
	const double Period = FMath::Max(0.5, Display.sweep_period_s);
	const double Persist = FMath::Max(0.3, Display.afterglow_s);
	// How long ago the sweep passed an azimuth (s), and how bright what it painted there still is (0..1).
	auto AgeAt = [&](double A)
	{
		double Behind = FMath::Fmod(Sweep - A, UE_DOUBLE_TWO_PI);
		if (Behind < 0.0)
		{
			Behind += UE_DOUBLE_TWO_PI;
		}
		return Behind / UE_DOUBLE_TWO_PI * Period;
	};
	auto Left = [&](double A) { return bTurning ? FMath::Exp(-AgeAt(A) / Persist) : 0.35; };

	const bool bRadiating = Console.equipment.IsReady(Search.transmitter) && Console.equipment.IsReady(Search.antenna);
	// The swept face still faintly lit behind the sweep, darkest just ahead of it (the screen's own glow: noise and the
	// defocused trace spread over the phosphor).
	constexpr int32 Sectors = 90;
	if (bTurning)
	{
		for (int32 K = 0; K < Sectors; ++K)
		{
			const double A0 = Sweep - UE_DOUBLE_TWO_PI * (K + 1) / Sectors;
			const double A1 = Sweep - UE_DOUBLE_TWO_PI * K / Sectors;
			const double Fresh = FMath::Exp(-(Period * (K + 0.5) / Sectors) / Persist);
			Wedge(Canvas, C, A0, A1, 0.0, R, Phosphor * static_cast<float>((bRadiating ? 0.07 : 0.03) * Fresh));
		}
	}

	// The range marks: painted by the sweep as it passes, fading behind it. Every 5 km (10 km on the long scales).
	const double Ring = Span > 20000.0 ? 10000.0 : 5000.0;
	for (double K = FMath::CeilToDouble((MinM + 1.0) / Ring) * Ring; K <= MaxM + 1.0; K += Ring)
	{
		const double Rr = RadiusOf(K);
		for (int32 I = 0; I < Sectors; ++I)
		{
			const double A0 = UE_DOUBLE_TWO_PI * I / Sectors;
			const double A1 = UE_DOUBLE_TWO_PI * (I + 1) / Sectors;
			const double Fresh = Left(0.5 * (A0 + A1));
			Wedge(Canvas, C, A0, A1, Rr - 2.2, Rr + 2.2, Phosphor * static_cast<float>(0.05 * Fresh));
			Wedge(Canvas, C, A0, A1, Rr - 0.8, Rr + 0.8, SearchGlow(0.12 + 0.55 * Fresh) * 0.8f);
		}
	}

	// Receiver noise: a speckle the sweep lays down while it radiates, each grain where it fell until the sweep comes round
	// again (seeded by the sweep's pass over its sector), fading with the afterglow. Ground clutter round the centre
	// without MTI (picture only; the model hides low targets in it).
	const bool bMti = !Search.mti_control.empty() && Console.panel.Get(Search.mti_control) == 1;
	if (bRadiating)
	{
		const double ClutterR = MinM <= 0.0 && !bMti ? RadiusOf(4000.0) : 0.0;
		for (int32 K = 0; K < Sectors; ++K)
		{
			const double A0 = UE_DOUBLE_TWO_PI * K / Sectors;
			const double Fresh = Left(A0 + 0.5 * UE_DOUBLE_TWO_PI / Sectors);
			if (Fresh < 0.04)
			{
				continue;
			}
			// The pass that painted this sector: the sweep's turns, less how far behind it the sector lies.
			const int32 Pass = FMath::FloorToInt32(PpiTurns - AgeAt(A0) / Period + 1e-6);
			FRandomStream Grain(K * 7919 + Pass * 104729);
			for (int32 I = 0; I < 14; ++I)
			{
				const double A = A0 + Grain.FRand() * UE_DOUBLE_TWO_PI / Sectors;
				const double Rr = R * FMath::Sqrt(Grain.FRand());
				const float Spark = Grain.FRandRange(0.15f, 0.55f);
				Polygon(Canvas, Polar(C, A, Rr), 1.1, 4, SearchGlow(Fresh * Spark));
			}
			for (int32 I = 0; ClutterR > 0.0 && I < 4; ++I)
			{
				const double A = A0 + Grain.FRand() * UE_DOUBLE_TWO_PI / Sectors;
				const FVector2D P = Polar(C, A, ClutterR * FMath::Sqrt(Grain.FRand()));
				Polygon(Canvas, P, 1.5, 4, SearchGlow(Fresh * Grain.FRandRange(0.25f, 0.6f)) * 0.8f);  // speckle, never a solid patch
			}
		}
	}
	for (int32 I = 0; I < 12; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 12.0;
		Line(Canvas, Polar(C, A, R * 0.95), Polar(C, A, R), 1.5f, Phosphor * 0.35f);
	}

	// Fading echoes, and IFF replies as arcs just beyond the echo: 20 degrees wide in open reception, narrow when
	// gated by the echo (mon_2112_80 p21); a second arc for a control interrogation (ЗАПРОС К).
	const bool bGated = !Search.iff_gate_control.empty() && Console.panel.Get(Search.iff_gate_control) == 1;
	for (const simcore::SearchScope::Blip& B : Scope->Blips())
	{
		if (B.strobe)
		{
			// A noise jammer's strobe (roadmap E3): a grainy bright spoke along its azimuth across the whole scale, as
			// bright as the jamming is strong; it has no range.
			const float Strength = static_cast<float>(B.brightness);
			Glow(Canvas, C, Polar(C, B.azimuth_rad, R), 2.5f, SearchGlow(0.9) * Strength);
			for (int32 I = 0; I < 40; ++I)
			{
				const FVector2D P = Polar(C, B.azimuth_rad + Noise.FRandRange(-0.02, 0.02), R * Noise.FRand());
				Polygon(Canvas, P, 1.3, 4, SearchGlow(Noise.FRandRange(0.4f, 1.0f)) * Strength);
			}
			continue;
		}
		if (B.range_m < MinM || B.range_m > MaxM)
		{
			continue;
		}
		const float Strength = static_cast<float>(B.brightness);
		if (B.iff_reply)
		{
			const double Half = FMath::DegreesToRadians(bGated ? 2.0 : 10.0);
			for (int32 Arc = 0; Arc < (B.control_reply ? 2 : 1); ++Arc)
			{
				const double Rr = RadiusOf(B.range_m) + 9.0 + 5.0 * Arc;
				Wedge(Canvas, C, B.azimuth_rad - Half, B.azimuth_rad + Half, Rr - 3.0, Rr + 3.0, Phosphor * (0.12f * Strength));
				Wedge(Canvas, C, B.azimuth_rad - Half, B.azimuth_rad + Half, Rr - 1.0, Rr + 1.0, SearchGlow(Strength));
			}
			continue;
		}
		// The echo: a short arc as wide as the beam and as deep as the pulse, a white flash when struck sinking through
		// yellow to amber, with the bloom of an overdriven spot round it.
		const double Rr = RadiusOf(B.range_m);
		const double HalfBeam = FMath::DegreesToRadians(1.2);
		Wedge(Canvas, C, B.azimuth_rad - 2.5 * HalfBeam, B.azimuth_rad + 2.5 * HalfBeam, Rr - 7.0, Rr + 7.0, Phosphor * (0.07f * Strength));
		Wedge(Canvas, C, B.azimuth_rad - 1.6 * HalfBeam, B.azimuth_rad + 1.6 * HalfBeam, Rr - 3.5, Rr + 3.5, SearchGlow(Strength) * 0.45f);
		Wedge(Canvas, C, B.azimuth_rad - HalfBeam, B.azimuth_rad + HalfBeam, Rr - 1.6, Rr + 1.6, SearchGlow(Strength));
	}

	// The sweep: the bright trace itself, with the freshly struck phosphor glowing just behind it.
	if (bTurning)
	{
		for (int32 I = 4; I >= 1; --I)
		{
			const double A = Sweep - FMath::DegreesToRadians(0.8 * I);
			Line(Canvas, C, Polar(C, A, R), 2.0f, SearchGlow(0.55 / I) * 0.5f);
		}
		Glow(Canvas, C, Polar(C, Sweep, R), 1.8f, SearchGlow(1.0) * (bRadiating ? 0.95f : 0.6f));
	}

	// The SSC azimuth line and its range gate, which the range officer puts on the target mark.
	if (Console.has_sensors)
	{
		const simcore::TrackSensorDef& Track = Console.track;
		const double Az = FMath::DegreesToRadians(Console.panel.GetValue(Track.az_wheel));
		Line(Canvas, C, Polar(C, Az, R), 1.5f, Amber * 0.7f);
		const double Gate = Console.panel.GetValue(Track.range_wheel);
		if (Gate >= MinM && Gate <= MaxM)
		{
			const double Rg = RadiusOf(Gate);
			Line(Canvas, Polar(C, Az - 0.06, Rg), Polar(C, Az + 0.06, Rg), 2.0f, Amber);
		}
	}
}

// ---------------------------------------------------------------------------------------------- range indicator

void USimCabinDisplayComponent::DrawRangeScope(UCanvas* Canvas, const FVector2D& Size, simcore::ScenarioRunner& Runner, bool bOn)
{
	if (!bOn)
	{
		return;
	}
	const simcore::ConsoleData& Console = Runner.Console();
	if (!Console.has_sensors)
	{
		return;
	}
	const simcore::TrackSensorDef& Track = Console.track;
	const simcore::CabinDisplay& Display = GetSim()->GetCabinLayout()->displays[LayoutIndex];
	const bool bRadiating = Console.equipment.IsReady(Track.transmitter);
	const double MaxM = FMath::Max(1.0, Track.max_range_m);
	const double Gate = Console.panel.GetValue(Track.range_wheel);
	const double HalfGate = Runner.EngagementOn() ? Runner.Engagement().Tuning().range_gate_m : 500.0;
	const std::vector<simcore::Contact>& Returns = Feed.track_returns;
	// Р I — ЦЕЛЬ — Р II: the fine sweep follows the target, or the missile of channel I or II (its range unit drives it).
	const simcore::Launcher* Launch = Runner.GetLauncher();
	const int32 SweepChannel = Launch != nullptr ? Launch->RangeSweepChannel(Console) : 0;
	const double MissileRange = SweepChannel != 0 ? Feed.MissileRangeM(SweepChannel) : -1.0;
	const bool bFollowMissile = SweepChannel != 0 && MissileRange >= 0.0;
	const double FineCentre = SweepChannel != 0 ? (bFollowMissile ? MissileRange : Gate) : Gate;
	const double X0 = 0.05 * Size.X;
	const double X1 = 0.95 * Size.X;

	// One sweep: baseline, grass while radiating, echo spikes, gate marks. [FromM, ToM] maps to X0..X1.
	auto Sweep = [&](double Y, double FromM, double ToM, double SpikeH, double TickM, bool bMissileSweep)
	{
		auto XOf = [&](double M) { return X0 + (M - FromM) / (ToM - FromM) * (X1 - X0); };
		// The graticule: a division every TickM, longer every fifth (op_panel's legend: "1 div = 1000 m" on the rough
		// sweep, "1 div = 5 m" on the precise one).
		if ((ToM - FromM) / TickM < 300.0)
		{
			for (double M = FMath::CeilToDouble(FromM / TickM) * TickM; M <= ToM; M += TickM)
			{
				const bool bMajor = FMath::Fmod(FMath::RoundToDouble(M / TickM), 5.0) < 0.5;
				Line(Canvas, FVector2D(XOf(M), Y + 3.0), FVector2D(XOf(M), Y + (bMajor ? 11.0 : 7.0)), 1.0f, TraceGreen * 0.4f);
			}
		}
		// The baseline and its grass: the noise differs every sweep, the phosphor keeps the last few, so the eye sees a
		// fuzzy band with the newest sweep brightest on it (ap3302: "the centreline is not sharp due to the presence of
		// noise"); an echo stands in the same place sweep after sweep.
		for (int32 Back = 3; Back >= 0; --Back)
		{
			FRandomStream Grass((RangeSweeps - Back) * 7919 + static_cast<int32>(Y));
			const float Fade = Back == 0 ? 1.0f : 0.35f / Back;
			double PrevX = X0;
			double PrevY = Y;
			for (double X = X0; X <= X1; X += 2.5)
			{
				const double Ny = bRadiating ? Grass.FRandRange(0.0, 1.0) * Grass.FRandRange(0.0, 9.0) : 0.0;
				if (Back == 0)
				{
					Glow(Canvas, FVector2D(PrevX, PrevY), FVector2D(X, Y - Ny), 1.6f, TraceGreen);
				}
				else
				{
					Line(Canvas, FVector2D(PrevX, PrevY), FVector2D(X, Y - Ny), 1.0f, TraceGreen * (0.5f * Fade));
				}
				PrevX = X;
				PrevY = Y - Ny;
			}
		}
		auto Spike = [&](double RangeM)
		{
			if (RangeM < FromM || RangeM > ToM)
			{
				return;
			}
			// A pulse: its rise and fall a few pixels apart, the top a little rounded, slightly different each
			// sweep (the receiver's noise rides on it).
			const double X = XOf(RangeM);
			const double H = SpikeH * Noise.FRandRange(0.93, 1.0);
			Glow(Canvas, FVector2D(X - 4.0, Y), FVector2D(X - 1.5, Y - H), 1.4f, TraceGreen);
			Glow(Canvas, FVector2D(X - 1.5, Y - H), FVector2D(X + 1.5, Y - H * 0.97), 1.4f, TraceGreen);
			Glow(Canvas, FVector2D(X + 1.5, Y - H * 0.97), FVector2D(X + 4.0, Y), 1.4f, TraceGreen);
		};
		if (bMissileSweep)
		{
			if (bFollowMissile)
			{
				Spike(MissileRange);  // the missile's echo, centred in the window
			}
		}
		else
		{
			for (const simcore::Contact& Echo : Returns)
			{
				Spike(Echo.range_m);
			}
		}
		// The range gate: a notch under the trace (not on a missile's sweep: it has its own range unit).
		for (double M : {Gate - HalfGate, Gate + HalfGate})
		{
			if (!bMissileSweep && M >= FromM && M <= ToM)
			{
				Line(Canvas, FVector2D(XOf(M), Y + 2.0), FVector2D(XOf(M), Y + 12.0), 2.0f, Amber);
			}
		}
	};
	++RangeSweeps;  // a new pair of sweeps each picture (the tube's own sweeps are far faster than the redraw)
	Sweep(0.42 * Size.Y, 0.0, MaxM, 0.22 * Size.Y, 1000.0, false);
	const double Half = 0.5 * Display.magnified_window_m;
	Sweep(0.85 * Size.Y, FineCentre - Half, FineCentre + Half, 0.25 * Size.Y, 5.0, SweepChannel != 0);
	Line(Canvas, FVector2D(0.5 * Size.X, 0.6 * Size.Y), FVector2D(0.5 * Size.X, 0.88 * Size.Y), 1.0f, Amber * 0.5f);

	// The range counter: where the gate is (it follows the target while tracking in range), as five lit drum digits
	// like the readout strip's, not a text line painted on the picture.
	const FString Digits = FString::Printf(TEXT("%05.0f"), FMath::Max(0.0, Gate));
	const double CellW = 0.055 * Size.X;
	const double CellH = 0.13 * Size.Y;
	for (int32 I = 0; I < 5; ++I)
	{
		// each digit in its own lit cell: a vertical bar CellW wide, with a gap to the next
		const FVector2D At(0.5 * Size.X + (I - 2) * CellW * 1.4, 0.1 * Size.Y);
		Line(Canvas, At - FVector2D(0.0, 0.5 * (CellH - CellW)), At + FVector2D(0.0, 0.5 * (CellH - CellW)),
			static_cast<float>(CellW), Amber * 0.85f);
		Text(Canvas, Digits.Mid(I, 1), At, FLinearColor(0.12f, 0.05f, 0.01f), 2.0f);
	}
}

// ---------------------------------------------------------------------------------------------- elevation search

void USimCabinDisplayComponent::DrawElevationScope(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	simcore::ScenarioRunner& Runner, bool bOn)
{
	// A round tube (OS81-10M) with a square raster area inside it: range across, elevation up.
	const FVector2D C = Size * 0.5;
	const double R = 0.48 * FMath::Min(Size.X, Size.Y);
	Disc(Canvas, C, R, bOn ? AmberGlass * 0.12f : AmberGlass);
	const simcore::ConsoleData& Console = Runner.Console();
	if (!bOn || !ElevScope.IsValid() || ElevRaster < 0 || ElevRaster >= static_cast<int32>(Display.rasters.size()) ||
		ElevRaster >= static_cast<int32>(Console.track.beams_deg.size()))
	{
		return;
	}
	const simcore::ElevationRaster& Raster = Display.rasters[ElevRaster];
	const double ElLow = Console.track.beams_deg[ElevRaster].first;
	const double ElHigh = Console.track.beams_deg[ElevRaster].second;
	const double Half = 0.66 * R;  // the square fits inside the round screen
	const double Left = C.X - Half;
	const double Right = C.X + Half;
	auto YOfScreen = [&](double Fraction) { return C.Y + Half - 2.0 * Half * Fraction; };  // 0 bottom, 1 top
	const double Top = YOfScreen(Raster.screen_high);
	const double Bottom = YOfScreen(Raster.screen_low);
	auto YOfEl = [&](double El)
	{
		const double F = (El - ElLow) / (ElHigh - ElLow);
		return Bottom + (Top - Bottom) * FMath::Clamp(F, 0.0, 1.0);
	};
	auto XOfOffset = [&](double OffsetM) { return C.X + OffsetM / (0.5 * Display.range_window_m) * Half; };

	// The tube's face lit faintly from behind the amber filter, darker to the rim (the dead tube's filter, lit).
	for (int32 I = 0; I < 6; ++I)
	{
		Disc(Canvas, C, R * (0.98 - 0.12 * I), Phosphor * (0.012f + 0.008f * I));
	}
	const bool bRadiating = Console.has_sensors && Console.equipment.IsReady(Console.track.transmitter);
	++ElevFrames;
	// The raster: the scan lines across the beam's band, each a little brighter or dimmer than its neighbour (a raster
	// on a long-persistence В screen is never even), and while the radar radiates, the receiver's noise on them as short
	// flecks that change every picture (the same "grass" as the range sweep's, drawn as the raster writes it).
	FRandomStream Grass((ElevFrames / 2) * 4099 + LayoutIndex);
	int32 Row = 0;
	for (double Y = Top; Y <= Bottom; Y += 4.0, ++Row)
	{
		const float Even = 0.07f + 0.04f * static_cast<float>((Row * 37) % 5) / 4.0f;
		Line(Canvas, FVector2D(Left, Y), FVector2D(Right, Y), 1.0f, Phosphor * Even);
		if (!bRadiating)
		{
			continue;
		}
		const int32 Flecks = Grass.RandRange(1, 4);
		for (int32 K = 0; K < Flecks; ++K)
		{
			const double X = Grass.FRandRange(Left, Right - 6.0);
			const double W = Grass.FRandRange(2.0, 7.0);
			Line(Canvas, FVector2D(X, Y), FVector2D(X + W, Y), 1.2f, SearchGlow(Grass.FRandRange(0.15, 0.4)));
		}
	}
	// The band's edges and the range marks: engraved on the filter, so seen in the tube's own light only faintly.
	Line(Canvas, FVector2D(Left, Top), FVector2D(Right, Top), 1.5f, Phosphor * 0.28f);
	Line(Canvas, FVector2D(Left, Bottom), FVector2D(Right, Bottom), 1.5f, Phosphor * 0.28f);
	Line(Canvas, FVector2D(Left, Top), FVector2D(Left, Bottom), 1.5f, Phosphor * 0.28f);
	Line(Canvas, FVector2D(Right, Top), FVector2D(Right, Bottom), 1.5f, Phosphor * 0.28f);
	for (int32 I = -2; I <= 2; ++I)
	{
		const double X = C.X + I * 0.5 * Half;
		Line(Canvas, FVector2D(X, Bottom + 2.0), FVector2D(X, Bottom + (I == 0 ? 9.0 : 5.0)), 1.0f, Phosphor * 0.3f);
	}
	// The range gate sits in the middle of the range window.
	Line(Canvas, FVector2D(C.X, Bottom), FVector2D(C.X, Bottom + 6.0), 1.5f, Amber * 0.6f);

	// Where the antenna points now: the raster's brightest line, with the phosphor's bloom round it, while the tracking
	// radar radiates; a dim line otherwise.
	const double AntennaY = YOfEl(Feed.antenna_el_deg);
	if (bRadiating)
	{
		Glow(Canvas, FVector2D(Left, AntennaY), FVector2D(Right, AntennaY), 2.4f, SearchGlow(0.85));
	}
	else
	{
		Line(Canvas, FVector2D(Left, AntennaY), FVector2D(Right, AntennaY), 2.0f, Phosphor * 0.35f);
	}

	// Target marks: a blip drawn along the raster's lines (the echo is as long as the pulse, a few lines high), struck
	// pale yellow and sinking to amber with the afterglow.
	for (const simcore::ElevationScope::Mark& M : ElevScope->Marks())
	{
		const double Fresh = M.brightness;
		const FVector2D P(XOfOffset(M.range_offset_m), YOfEl(M.elevation_deg));
		const FLinearColor Ink = SearchGlow(Fresh);
		Disc(Canvas, P, 11.0, Ink * 0.12f);
		for (int32 L = -2; L <= 2; ++L)
		{
			const double W = (L == 0 ? 9.0 : L == 1 || L == -1 ? 7.0 : 4.0);
			Glow(Canvas, P + FVector2D(-W, 2.0 * L), P + FVector2D(W, 2.0 * L), 1.4f, Ink * (L == 0 ? 1.0f : 0.7f));
		}
	}
}

// ---------------------------------------------------------------------------------------------- instruments

void USimCabinDisplayComponent::DrawDial(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	const simcore::ConsoleData& Console, bool bOn, bool bLit)
{
	const FVector2D C = Size * 0.5;
	const double R = 0.47 * FMath::Min(Size.X, Size.Y);
	// Unpowered or unlit, the face is barely readable in the cabin's gloom.
	const FLinearColor Face = bLit ? FLinearColor(0.85f, 0.8f, 0.55f) : FLinearColor(0.07f, 0.07f, 0.06f);
	const FLinearColor Ink = bLit ? FLinearColor(0.05f, 0.05f, 0.05f) : FLinearColor(0.2f, 0.2f, 0.18f);
	Disc(Canvas, C, R, Face);
	Circle(Canvas, C, R, Ink, 2.0f);
	for (int32 I = 0; I < Display.marks; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / Display.marks;
		Line(Canvas, Polar(C, A, 0.85 * R), Polar(C, A, R), 2.0f, Ink);
		Text(Canvas, Label(Display.labels_per_turn * I / Display.marks), Polar(C, A, 0.68 * R), Ink, 1.3f);
	}
	if (!bOn)
	{
		return;
	}
	const double A = UE_DOUBLE_TWO_PI * Console.panel.GetValue(Display.control) / Display.value_per_turn;
	Line(Canvas, C, Polar(C, A, 0.8 * R), 3.0f, FLinearColor(0.7f, 0.05f, 0.02f));
	Disc(Canvas, C, 0.06 * R, Ink);
}

void USimCabinDisplayComponent::DrawMeter(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	const simcore::ConsoleData& Console, bool bOn)
{
	const FLinearColor Face(0.8f, 0.78f, 0.7f);
	const FLinearColor Ink(0.05f, 0.05f, 0.05f);
	// Fill the face: one line as thick as the face is tall.
	Line(Canvas, FVector2D(0.0, 0.5 * Size.Y), FVector2D(Size.X, 0.5 * Size.Y), static_cast<float>(Size.Y), Face);
	const FVector2D Pivot(0.5 * Size.X, 0.92 * Size.Y);
	const double R = 0.75 * Size.Y;
	constexpr double SwingDeg = 50.0;
	for (int32 I = 0; I <= Display.marks; ++I)
	{
		const double A = FMath::DegreesToRadians(-SwingDeg + 2.0 * SwingDeg * I / Display.marks);
		Line(Canvas, Polar(Pivot, A, 0.88 * R), Polar(Pivot, A, R), 2.0f, Ink);
		Text(Canvas, Label(Display.full_scale * I / Display.marks), Polar(Pivot, A, 0.75 * R), Ink, 1.2f);
	}
	Text(Canvas, FString::Printf(TEXT("%s  %s"), *FString(UTF8_TO_TCHAR(Display.label.c_str())), *FString(UTF8_TO_TCHAR(Display.unit.c_str()))),
		FVector2D(0.5 * Size.X, 0.55 * Size.Y), Ink, 1.3f);
	const double Reading = bOn ? simcore::MeterValue(Display, Console) : 0.0;
	const double Fraction = FMath::Clamp(Reading / Display.full_scale, 0.0, 1.0);
	const double A = FMath::DegreesToRadians(-SwingDeg + 2.0 * SwingDeg * Fraction);
	Line(Canvas, Pivot, Polar(Pivot, A, 0.95 * R), 2.5f, FLinearColor(0.7f, 0.05f, 0.02f));
}

// ---------------------------------------------------------------------------------------------- readout strip

void USimCabinDisplayComponent::DrawCounters(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	simcore::ScenarioRunner& Runner, bool bOn)
{
	// Eight round amber-lit digit counters in a pale window (unit 0094-5, as baked into the panel face by
	// tools/cabin_textures.py), each reading its place's value (simcore/readouts.hpp). Unpowered, the lamps are dark and
	// the digits unreadable.
	const simcore::ConsoleData& Console = Runner.Console();
	Line(Canvas, FVector2D(0.0, 0.5 * Size.Y), FVector2D(Size.X, 0.5 * Size.Y), static_cast<float>(Size.Y),
		FLinearColor(0.74f, 0.72f, 0.66f));
	const double Pitch = 0.88 * Size.X / 8.0;
	const double Dia = FMath::Min(0.84 * Size.Y, 0.8 * Pitch);
	for (int32 K = 0; K < 8; ++K)
	{
		const FVector2D At(0.03 * Size.X + Pitch * (K + 0.5), 0.5 * Size.Y);
		Disc(Canvas, At, 0.5 * Dia, FLinearColor(0.16f, 0.13f, 0.1f));
		Disc(Canvas, At, 0.42 * Dia, bOn ? FLinearColor(0.9f, 0.36f, 0.05f) : FLinearColor(0.32f, 0.18f, 0.07f));
		if (bOn && K < static_cast<int32>(Display.readouts.size()))
		{
			const int32 Value = simcore::CounterReading(Display.readouts[K], Console, Feed.track_returns, Feed.antenna_el_deg);
			Text(Canvas, FString::Printf(TEXT("%03d"), Value), At, FLinearColor(0.12f, 0.04f, 0.0f), 1.1f);
		}
	}
	// the lamp at the right end of the strip
	Disc(Canvas, FVector2D(Size.X - 0.045 * Size.X, 0.5 * Size.Y), 0.1 * Size.Y, bOn ? FLinearColor(0.7f, 0.25f, 0.04f) : FLinearColor(0.3f, 0.2f, 0.1f));
}

// ---------------------------------------------------------------------------------------------- command post

namespace
{
	// The IT-45's tube: a long-afterglow radar screen behind an amber filter (the photographs, rwd_mb3 and shilka_narod;
	// the VIKO-1 remote PPI wears the same orange glass). The Soviet tube table (radiolamp_elt) gives the radar screens'
	// long-afterglow coatings a white flash fading to yellow (В) or a pink one to orange (К): through the filter a pale
	// yellow flash that sinks to amber as it fades.
	const FLinearColor AspdFlash(1.0f, 0.88f, 0.52f);
	const FLinearColor AspdAfter(1.0f, 0.46f, 0.06f);
	// The character generator's symbols, rewritten every frame: a steady yellow-amber.
	const FLinearColor AspdSymbol(1.0f, 0.7f, 0.26f);
	// The lit tube's own faint glow through the filter. The canvas draws lines without blending, so everything on the face
	// is drawn as the face plus its own light (OnFace), never darker than the face around it.
	const FLinearColor AspdFace = AmberGlass * 0.07f;
	FLinearColor OnFace(const FLinearColor& Light)
	{
		FLinearColor C = AspdFace + Light;
		C.A = 1.0f;
		return C;
	}

	// The tube's colour at brightness B (1 just painted, falling as it fades).
	FLinearColor Afterglow(double B)
	{
		const float Bf = static_cast<float>(FMath::Clamp(B, 0.0, 1.0));
		return FMath::Lerp(AspdAfter, AspdFlash, Bf * Bf) * Bf;
	}

	// A digit as strokes: the seven segments of the ДХ's vacuum-fluorescent window and of the IT-45's character
	// generator (SAM3D's glyphs). At is the top left, W x H the cell, Slant the top's lean to the right (in W).
	void StrokeDigit(UCanvas* Canvas, TCHAR Digit, const FVector2D& At, double W, double H, float Thickness, const FLinearColor& Color,
		double Slant = 0.0, bool bGlow = true)
	{
		static const uint8 Segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};  // gfedcba
		const int32 D = Digit - TEXT('0');
		const uint8 Mask = D >= 0 && D <= 9 ? Segments[D] : (Digit == TEXT('-') ? 0x40 : 0x00);
		auto P = [&](double X, double Y) { return At + FVector2D(X * W + Slant * W * (1.0 - Y), Y * H); };
		const double G = 0.08;  // the gap at each segment's ends
		const FVector2D Ends[7][2] = {{P(G, 0.0), P(1.0 - G, 0.0)}, {P(1.0, G), P(1.0, 0.5 - G * 0.5)},
			{P(1.0, 0.5 + G * 0.5), P(1.0, 1.0 - G)}, {P(G, 1.0), P(1.0 - G, 1.0)}, {P(0.0, 0.5 + G * 0.5), P(0.0, 1.0 - G)},
			{P(0.0, G), P(0.0, 0.5 - G * 0.5)}, {P(G, 0.5), P(1.0 - G, 0.5)}};
		for (int32 S = 0; S < 7; ++S)
		{
			if ((Mask & (1 << S)) != 0)
			{
				if (bGlow)
				{
					Beam(Canvas, Ends[S][0], Ends[S][1], Thickness, Color);
				}
				else
				{
					Line(Canvas, Ends[S][0], Ends[S][1], Thickness, Color);
				}
			}
		}
	}

	void StrokeNumber(UCanvas* Canvas, const FString& Digits, const FVector2D& At, double W, double H, float Thickness,
		const FLinearColor& Color, double Slant = 0.0, bool bGlow = true)
	{
		for (int32 I = 0; I < Digits.Len(); ++I)
		{
			StrokeDigit(Canvas, Digits[I], At + FVector2D(I * W * 1.45, 0.0), W, H, Thickness, Color, Slant, bGlow);
		}
	}

	void Fill(UCanvas* Canvas, const FVector2D& At, const FVector2D& Size, const FLinearColor& Color)
	{
		FlushShapes();
		Canvas->K2_DrawTexture(nullptr, At, Size, FVector2D::ZeroVector, FVector2D::UnitVector, Color, BLEND_Opaque);
	}

	// A time of arrival as the IT-45 writes it, minutes and seconds.
	FString ArrivalText(double Seconds)
	{
		const int32 S = FMath::Max(0, FMath::RoundToInt32(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	// The plotter's grease pencil (SAM3D's colours): red for the enemy and the unknown, blue for friends (the board's code
	// tens digit 1).
	const FLinearColor PencilRed(0.22f, 0.02f, 0.01f);
	const FLinearColor PencilBlue(0.015f, 0.035f, 0.16f);
}

void USimCabinDisplayComponent::DrawAspdScope(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	const simcore::PostFeed& Post, bool bOn)
{
	// The IT-45 (rwd_mb3: the primary air picture and the telecode's data as digits, signs and symbols; the ФР block's
	// radial-circular sweep, its range marks and test echo; command_post.hpp): north up, the post at the centre, on the
	// scale the ФР block's switch is at (35, 50 or 100 km, ru_pu12_text). The sweep turns with the radar it is synchronised
	// to and paints as it goes: the range marks and a cabled radar's echoes glow behind it and fade in the afterglow. Over
	// the picture the character generator writes each target: its number, a mark for its height zone (H1 a circle, H2 a
	// square, H3 a triangle), its speed vector (a minute's travel), friends barred, groups ringed twice; the DT-2's marker;
	// the marked target brighter, with its time of arrival (rwd_mb3). Over the tube the grid glass (shilka_narod). Symbols,
	// glyphs and colours are SAM3D's. Drawn from the post's feed (post_feed.hpp): the sim's, or the stream's on a client.
	const FVector2D C = Size * 0.5;
	const double S = FMath::Min(Size.X, Size.Y);
	const double R = 0.47 * S;
	const double ScaleM = Post.scale_m;
	Disc(Canvas, C, 0.5 * S, FLinearColor(0.008f, 0.006f, 0.004f));
	if (!bOn || !(ScaleM > 0.0))
	{
		DeadFilter(Canvas, C, R);
	}
	else
	{
		Disc(Canvas, C, R, OnFace(FLinearColor::Black));
		const double Omega = UE_DOUBLE_TWO_PI / FMath::Max(1.0, Display.sweep_period_s);
		const double Tau = FMath::Max(0.2, Display.afterglow_s);
		auto AgeAt = [&](double A) { return FMath::Fmod(AspdSweepRad - A + 2.0 * UE_DOUBLE_TWO_PI, UE_DOUBLE_TWO_PI) / Omega; };
		auto BrightAt = [&](double A) { return FMath::Exp(-AgeAt(A) / Tau); };
		auto ScreenOf = [&](double X, double Y) { return C + FVector2D(X, -Y) * R; };

		// The ФР block's range marks: rings the sweep brightens as it passes.
		const double RingStepM = ScaleM > 60000.0 ? 20000.0 : (ScaleM > 40000.0 ? 10000.0 : 5000.0);
		constexpr int32 RingSegments = 144;
		for (double Ring = RingStepM; Ring <= ScaleM + 1.0; Ring += RingStepM)
		{
			const double Rr = R * Ring / ScaleM;
			for (int32 I = 0; I < RingSegments; ++I)
			{
				const double A0 = UE_DOUBLE_TWO_PI * I / RingSegments;
				const double A1 = UE_DOUBLE_TWO_PI * (I + 1) / RingSegments;
				Line(Canvas, Polar(C, A0, Rr), Polar(C, A1, Rr), 1.5f, OnFace(Afterglow(BrightAt(0.5 * (A0 + A1))) * 0.16f));
			}
		}

		// A cabled radar's picture (any primary track): the receiver's noise speckled over the tube where the sweep has
		// been, and each target's echo under its symbol. The speckle stays where it was painted until the sweep comes round.
		bool bPrimary = false;
		for (const simcore::CommandPost::ScopeMark& M : Post.scope)
		{
			bPrimary = bPrimary || M.primary;
		}
		if (bPrimary)
		{
			constexpr int32 Bins = 180;
			for (int32 Bin = 0; Bin < Bins; ++Bin)
			{
				const double A = UE_DOUBLE_TWO_PI * (Bin + 0.5) / Bins;
				const double B = BrightAt(A);
				if (B < 0.08)
				{
					continue;
				}
				const int32 Turn = AspdTurns - (A > AspdSweepRad ? 1 : 0);
				FRandomStream Speckle(Bin * 7919 + Turn * 104729);
				for (int32 K = 0; K < 3; ++K)
				{
					const double Aa = A + Speckle.FRandRange(-0.5, 0.5) * UE_DOUBLE_TWO_PI / Bins;
					const FVector2D P = Polar(C, Aa, R * FMath::Sqrt(Speckle.FRand()));
					Polygon(Canvas, P, 1.2, 4, OnFace(Afterglow(B) * Speckle.FRandRange(0.04f, 0.16f)));
				}
			}
			for (const simcore::CommandPost::ScopeMark& M : Post.scope)
			{
				if (!M.primary)
				{
					continue;
				}
				// a short arc across the beam, brightest where it was just painted
				const double Az = FMath::Atan2(M.x, M.y);
				const double Rr = R * FMath::Sqrt(M.x * M.x + M.y * M.y);
				const double B = BrightAt(Az);
				for (int32 I = -2; I <= 2; ++I)
				{
					const double Aa = Az + I * 0.006;
					Disc(Canvas, Polar(C, Aa, Rr), 0.007 * S, OnFace(Afterglow(B) * (0.6f - 0.1f * FMath::Abs(I))));
				}
			}
		}

		// The sweep: the trace itself, faint, and a short glow behind it.
		for (int32 I = 12; I >= 1; --I)
		{
			const double A = AspdSweepRad - FMath::DegreesToRadians(0.6 * I);
			Line(Canvas, C, Polar(C, A, R), 2.0f, OnFace(Afterglow(BrightAt(A)) * (0.05f / I)));
		}
		Line(Canvas, C, Polar(C, AspdSweepRad, R), 1.5f, OnFace(AspdFlash * 0.3f));

		// The character generator's symbols.
		const int32 KeypadTarget = Post.keypad.empty() ? -1 : FCString::Atoi(UTF8_TO_TCHAR(Post.keypad.c_str()));
		for (const simcore::CommandPost::ScopeMark& M : Post.scope)
		{
			if (M.number < 0)
			{
				continue;  // the ФР block's imitated echo not yet taken off: only its echo
			}
			const bool bMarked = M.number == Post.marked;
			const bool bKeyed = M.number == KeypadTarget;
			const FVector2D At = ScreenOf(M.x, M.y);
			const FVector2D Tip = At + FVector2D(M.vx, -M.vy) * R;
			const FLinearColor Ink = OnFace(AspdSymbol * (bMarked || bKeyed ? 0.9f : (M.friendly ? 0.4f : 0.6f)));
			const float T = bMarked ? 2.2f : 1.5f;
			Line(Canvas, At, Tip, T, Ink);
			const double K = 0.011 * S;
			if (M.zone <= 1)
			{
				Circle(Canvas, At, K, Ink, T);
			}
			else if (M.zone == 2)
			{
				FlushShapes();
				Canvas->K2_DrawBox(At - FVector2D(K, K), FVector2D(2.0 * K, 2.0 * K), T, Ink);
			}
			else
			{
				const FVector2D P0 = At + FVector2D(0.0, -1.25 * K);
				const FVector2D P1 = At + FVector2D(1.1 * K, 0.8 * K);
				const FVector2D P2 = At + FVector2D(-1.1 * K, 0.8 * K);
				Line(Canvas, P0, P1, T, Ink);
				Line(Canvas, P1, P2, T, Ink);
				Line(Canvas, P2, P0, T, Ink);
			}
			if (M.group)
			{
				Circle(Canvas, At, 1.7 * K, Ink, 1.0f);
			}
			if (M.friendly)
			{
				Line(Canvas, At + FVector2D(-1.7 * K, -1.8 * K), At + FVector2D(1.7 * K, -1.8 * K), T, Ink);
			}
			const double Gw = 0.012 * S;
			StrokeNumber(Canvas, FString::Printf(TEXT("%02d"), M.number), At + FVector2D(1.9 * K, -3.4 * K), Gw, 1.6 * Gw, T, Ink, 0.0, false);
			if (bKeyed)
			{
				// the number the ДХ is on: bracketed
				const FVector2D B0 = At - FVector2D(2.6 * K, 2.6 * K);
				FlushShapes();
				Canvas->K2_DrawBox(B0, FVector2D(5.2 * K, 5.2 * K), 1.0f, Ink);
			}
			if (bMarked && Post.marked_arrival_s >= 0.0)
			{
				Text(Canvas, ArrivalText(Post.marked_arrival_s), At + FVector2D(2.0 * K, 1.4 * K), OnFace(AspdSymbol * 0.9f), 1.0f, false);
			}
			if (M.designated)
			{
				// the regiment's designation to this battery (ЦУ with its address): a diamond round the symbol
				const double D = 2.4 * K;
				const FVector2D N = At + FVector2D(0.0, -D), E = At + FVector2D(D, 0.0), So = At + FVector2D(0.0, D), W = At + FVector2D(-D, 0.0);
				Line(Canvas, N, E, 1.2f, Ink);
				Line(Canvas, E, So, 1.2f, Ink);
				Line(Canvas, So, W, 1.2f, Ink);
				Line(Canvas, W, N, 1.2f, Ink);
			}
		}

		// The regiment's last command by telecode: its two digits at the tube's top left (rwd_mb3: commands and reports
		// shown on the indicator as decimal numbers).
		if (Post.command >= 0)
		{
			const double Gw = 0.016 * S;
			StrokeNumber(Canvas, FString::Printf(TEXT("%02d"), Post.command), C + FVector2D(-0.62 * R, -0.62 * R), Gw, 1.6 * Gw, 1.8f,
				OnFace(AspdSymbol * 0.9f), 0.0, false);
		}

		// The DT-2's marker: a small ring open at the quarters.
		const FVector2D Mk = ScreenOf(Post.marker_x, Post.marker_y);
		const double Mr = 0.02 * S;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			for (int32 I = 0; I < 8; ++I)
			{
				const double A0 = UE_DOUBLE_HALF_PI * Q + 0.25 + 0.135 * I;
				Line(Canvas, Polar(Mk, A0, Mr), Polar(Mk, A0 + 0.135, Mr), 1.8f, OnFace(AspdFlash * 0.8f));
			}
		}
	}
	if (!Display.glass)
	{
		return;
	}
	// The grid glass in front of the tube (shilka_narod: ruled in squares), its lines catching the cabin's light, and the
	// azimuth ring round its edge every 10 degrees (SAM3D's).
	const FLinearColor Etch(0.03f, 0.03f, 0.028f);
	for (int32 I = 1; I < 12; ++I)
	{
		const double U = C.X - R + 2.0 * R * I / 12.0;
		const double V = C.Y - R + 2.0 * R * I / 12.0;
		const double Hu = FMath::Sqrt(FMath::Max(0.0, R * R - FMath::Square(U - C.X)));
		const double Hv = FMath::Sqrt(FMath::Max(0.0, R * R - FMath::Square(V - C.Y)));
		Line(Canvas, FVector2D(U, C.Y - Hu), FVector2D(U, C.Y + Hu), 1.0f, Etch);
		Line(Canvas, FVector2D(C.X - Hv, V), FVector2D(C.X + Hv, V), 1.0f, Etch);
	}
	for (int32 I = 0; I < 36; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 36.0;
		Line(Canvas, Polar(C, A, R * (I % 3 == 0 ? 0.955 : 0.975)), Polar(C, A, R), 1.0f, Etch * 1.6f);
	}
}

bool USimCabinDisplayComponent::RayHit(const FVector& WorldStart, const FVector& WorldEnd, FVector2D& OutUvM, double& OutDistanceCm) const
{
	if (Screen == nullptr || !(FaceWidthCm > 0.0))
	{
		return false;
	}
	const FTransform& T = Screen->GetComponentTransform();
	const FVector A = T.InverseTransformPosition(WorldStart);
	const FVector B = T.InverseTransformPosition(WorldEnd);
	const double Da = FVector::DotProduct(A - FaceCentre, FaceNormal);
	const double Db = FVector::DotProduct(B - FaceCentre, FaceNormal);
	if (Da == Db || (Da > 0.0) == (Db > 0.0))
	{
		return false;  // parallel, or both on one side
	}
	const double F = Da / (Da - Db);
	const FVector P = A + (B - A) * F;
	const double U = FVector::DotProduct(P - FaceCentre, FaceRight);
	const double V = FVector::DotProduct(P - FaceCentre, FaceUp);
	if (FMath::Abs(U) > 0.5 * FaceWidthCm || FMath::Abs(V) > 0.5 * FaceHeightCm)
	{
		return false;
	}
	OutUvM = FVector2D(U / 100.0, V / 100.0);
	OutDistanceCm = F * (WorldEnd - WorldStart).Size();
	return true;
}

bool USimCabinDisplayComponent::BoardGround(const FVector2D& UvM, double& OutEastM, double& OutNorthM, double& OutMetresPerBoardM) const
{
	const USimWorldSubsystem* Sim = GetSim();
	if (Sim == nullptr || Kind != simcore::CabinDisplayKind::PlotBoard || !(FaceWidthCm > 0.0))
	{
		return false;
	}
	// As DrawPlotBoard draws it: the outer ring at 0.47 of the shorter side.
	const double RingM = 0.47 * FMath::Min(FaceWidthCm, FaceHeightCm) / 100.0;
	OutMetresPerBoardM = PlotBoardRadiusM / RingM;
	const simcore::PostFeed Post = Sim->GetPostFeed();
	OutEastM = Post.position.x + UvM.X * OutMetresPerBoardM;
	OutNorthM = Post.position.y + UvM.Y * OutMetresPerBoardM;
	return Post.post;
}

void USimCabinDisplayComponent::DrawPlotBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn)
{
	// The plotter's air-situation board (csla_pu12_manual item 24; rwd_mb3, Luftlagekarte: a sheet of plexiglass on which
	// the targets' tracks are drawn by hand in the designation squares, and their azimuth and range taken off; its
	// photograph: the grid and range circles etched into the glass). Drawn on black, which the glass material
	// (M_CabinGlassBoard) leaves clear: only the etching and the pencil show. The post at the centre, north up, 200 km to
	// the outer ring (data/command/pu12.json): rings every 50 km, ticks every 10 degrees, the PVO grid's big squares
	// (1 degree by 1 degree 30', their two digits) and their ninths (pvo_grid.hpp; bguir_tsp), so a report's square can be
	// found. Each target's last crosses joined into its track in yellow grease pencil (blue for a friend), its number and
	// its height over its code (the board's notation). Spacing, lettering and colours are SAM3D's.
	const double RadiusM = PlotBoardRadiusM;
	const FVector2D C = Size * 0.5;
	const double R = 0.47 * FMath::Min(Size.X, Size.Y);
	Fill(Canvas, FVector2D::ZeroVector, Size, FLinearColor::Black);  // clear glass
	const FLinearColor Etch(0.16f, 0.17f, 0.17f);     // frosted lines in the plexiglass
	const FLinearColor EtchFine(0.07f, 0.075f, 0.075f);
	const FLinearColor EtchText(0.22f, 0.23f, 0.23f);
	const FLinearColor Yellow(1.0f, 0.82f, 0.18f);
	const FLinearColor Blue(0.25f, 0.55f, 1.0f);
	auto ToScreen = [&](double EastRel, double NorthRel) { return C + FVector2D(EastRel, -NorthRel) / RadiusM * R; };
	auto Inside = [&](const FVector2D& P) { return FVector2D::Distance(P, C) <= R; };

	// The PVO grid, etched: big squares with their digits, their ninths faint (where the scenario's place is known).
	if (const USimWorldSubsystem* Sim = GetSim())
	{
		const FVector LonLatH = Sim->GetOriginLongitudeLatitudeHeight();
		const simcore::LocalFrame Frame(simcore::GeoPoint::FromDegrees(LonLatH.Y, LonLatH.X, LonLatH.Z));
		const simcore::GeoPoint Centre = Frame.EnuToGeodetic(Post.position);
		const double Lat0 = FMath::RadiansToDegrees(Centre.lat_rad);
		const double Lon0 = FMath::RadiansToDegrees(Centre.lon_rad);
		const double DLat = RadiusM / 111320.0;
		const double DLon = RadiusM / (111320.0 * FMath::Max(0.2, FMath::Cos(Centre.lat_rad)));
		auto GridScreen = [&](double Lat, double Lon)
		{
			const simcore::Vec3 E = Frame.GeodeticToEnu(simcore::GeoPoint::FromDegrees(Lat, Lon, 0.0));
			return ToScreen(E.x - Post.position.x, E.y - Post.position.y);
		};
		const double LatA = FMath::FloorToDouble(Lat0 - DLat) - 1.0, LatB = FMath::CeilToDouble(Lat0 + DLat) + 1.0;
		const double LonA = FMath::FloorToDouble((Lon0 - DLon) / 1.5) * 1.5 - 1.5, LonB = FMath::CeilToDouble((Lon0 + DLon) / 1.5) * 1.5 + 1.5;
		constexpr int32 Steps = 24;
		// The ninths and the ninths' ninths: the medium squares' lines faint, the big squares' bold.
		for (int32 Level = 2; Level >= 0; --Level)
		{
			const double Div = Level == 0 ? 1.0 : (Level == 1 ? 3.0 : 9.0);
			const FLinearColor Ink = Level == 0 ? Etch : (Level == 1 ? EtchFine : EtchFine * 0.45f);
			const float Thick = Level == 0 ? 1.6f : 1.0f;
			for (double Lat = LatA; Lat <= LatB + 1e-9; Lat += 1.0 / Div)
			{
				for (int32 I = 0; I < Steps; ++I)
				{
					const FVector2D P0 = GridScreen(Lat, LonA + (LonB - LonA) * I / Steps);
					const FVector2D P1 = GridScreen(Lat, LonA + (LonB - LonA) * (I + 1) / Steps);
					if (Inside(P0) && Inside(P1)) Line(Canvas, P0, P1, Thick, Ink);
				}
			}
			for (double Lon = LonA; Lon <= LonB + 1e-9; Lon += 1.5 / Div)
			{
				for (int32 I = 0; I < Steps; ++I)
				{
					const FVector2D P0 = GridScreen(LatA + (LatB - LatA) * I / Steps, Lon);
					const FVector2D P1 = GridScreen(LatA + (LatB - LatA) * (I + 1) / Steps, Lon);
					if (Inside(P0) && Inside(P1)) Line(Canvas, P0, P1, Thick, Ink);
				}
			}
		}
		// Each big square's two digits (its row and column, pvo_grid::Encode) in its top-left corner.
		for (double Lat = LatA; Lat < LatB; Lat += 1.0)
		{
			for (double Lon = LonA; Lon < LonB; Lon += 1.5)
			{
				const FVector2D P = GridScreen(Lat + 0.93, Lon + 0.06);
				if (!Inside(P)) continue;
				const std::string Digits = simcore::pvo_grid::Encode(Lat + 0.5, Lon + 0.75, 0);
				Text(Canvas, FString(UTF8_TO_TCHAR(Digits.c_str())), P, EtchText, 0.85f, false);
			}
		}
	}

	// The range rings (every 50 km, numbered) and the azimuth ticks (every 10 degrees, the cardinal ones longer).
	for (int32 K = 1; K <= 4; ++K)
	{
		Circle(Canvas, C, R * K / 4.0, Etch, K == 4 ? 2.0f : 1.4f);
		Text(Canvas, FString::FromInt(50 * K), Polar(C, FMath::DegreesToRadians(45.0), R * K / 4.0) + FVector2D(3.0, -6.0), EtchText, 0.6f, false);
	}
	for (int32 I = 0; I < 36; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 36.0;
		Line(Canvas, Polar(C, A, R), Polar(C, A, R * (I % 9 == 0 ? 0.92 : 0.96)), 1.2f, Etch);
	}
	Text(Canvas, TEXT("С"), C + FVector2D(-4.0, -R - 0.025 * Size.Y), EtchText, 0.8f, false);  // север
	Disc(Canvas, C, 0.008 * Size.X, Etch * 1.5f);  // the post

	// The grease pencil: each target's crosses of the last minutes joined into its track, its number, height over code.
	for (const simcore::em::ViewBoardMark& Latest : simcore::LatestMarks(Post))
	{
		const FLinearColor Pencil = Latest.code / 10 == 1 ? Blue : Yellow;
		FVector2D Prev = FVector2D::ZeroVector;
		bool bHasPrev = false;
		for (const simcore::em::ViewBoardMark& M : Post.glass)
		{
			if (M.number != Latest.number || Latest.stamp - M.stamp > 4)
			{
				continue;
			}
			const simcore::Vec3 Rel = M.position - Post.position;
			const FVector2D P = ToScreen(Rel.x, Rel.y);
			const double K = 0.008 * Size.X;
			// a cross for each plot, the waxy line of the pencil a little uneven
			Line(Canvas, P - FVector2D(K, K), P + FVector2D(K, K), 2.4f, Pencil);
			Line(Canvas, P - FVector2D(K, -K), P + FVector2D(K, -K), 2.4f, Pencil);
			Text(Canvas, FString::Printf(TEXT("%02d"), M.stamp % 100), P + FVector2D(K + 1.0, 1.0), Pencil * 0.75f, 0.55f, false);
			if (bHasPrev)
			{
				Line(Canvas, Prev, P, 2.0f, Pencil * 0.85f);
			}
			Prev = P;
			bHasPrev = true;
		}
		if (bHasPrev)
		{
			const FVector2D At = Prev + FVector2D(0.018 * Size.X, -0.05 * Size.Y);
			Text(Canvas, FString::Printf(TEXT("%d"), Latest.number), At, Pencil, 0.95f, false);
			Text(Canvas, FString::Printf(TEXT("%d"), Latest.height_hm), At + FVector2D(0.0, 0.026 * Size.Y), Pencil, 0.75f, false);
			Line(Canvas, At + FVector2D(0.0, 0.052 * Size.Y), At + FVector2D(0.045 * Size.X, 0.052 * Size.Y), 1.5f, Pencil);
			Text(Canvas, FString::Printf(TEXT("%02d"), Latest.code), At + FVector2D(0.0, 0.056 * Size.Y), Pencil, 0.75f, false);
		}
	}
	(void)bOn;  // a board is read by the cabin's light, powered or not
}

void USimCabinDisplayComponent::DrawKeypadWindow(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn)
{
	// The ДХ's window (rwd_mb3's photograph: blue glass at the top of the keypad): the number the keypad is on, typed or
	// the marker's target; ЦУ while it waits for the launcher's place, flashing once a second (a warning's rate, GOST
	// 21829-76: 0.5-1 Hz); the last designation under it. Blue-green vacuum-fluorescent digits with their dark segments
	// faintly showing: SAM3D's reading of the blue window.
	Fill(Canvas, FVector2D::ZeroVector, Size, FLinearColor(0.004f, 0.01f, 0.03f));
	const FLinearColor Vfd(0.3f, 1.0f, 0.82f);
	const FLinearColor Ghost(0.012f, 0.035f, 0.04f);
	const double H = 0.5 * Size.Y;
	const double W = 0.28 * H;
	const FVector2D At(0.08 * Size.X, 0.14 * Size.Y);
	StrokeNumber(Canvas, TEXT("88"), At, W * 2.0, H, 5.0f, Ghost, 0.08, false);
	if (!bOn)
	{
		return;
	}
	if (!Post.keypad.empty())
	{
		StrokeNumber(Canvas, FString(UTF8_TO_TCHAR(Post.keypad.c_str())).Right(2).LeftPad(2), At, W * 2.0, H, 5.0f, Vfd, 0.08);
	}
	const bool bBlink = FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0) < 0.6;
	const FVector2D Cu(0.78 * Size.X, 0.3 * Size.Y);
	Text(Canvas, TEXT("ЦУ"), Cu, Post.keypad_designating && bBlink ? Vfd : Ghost * 2.0f, 1.6f);
	if (!Post.keypad_last.empty())
	{
		Text(Canvas, FString(UTF8_TO_TCHAR(Post.keypad_last.c_str())), FVector2D(0.7 * Size.X, 0.8 * Size.Y), Vfd * 0.7f, 1.1f);
	}
}

void USimCabinDisplayComponent::DrawStatusBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn,
	float DeltaTime)
{
	// The IT-1M / IT-2M board (rwd_mb3: the technical state and the combat work of the fire units; its photographs): a
	// light plate, black legends over three groups of amber lamps, a row per launcher in the battery's order and one for the
	// support vehicles. КОМПЛЕКТ 1, 1/2, 1/4 (its missiles), РАБОТА ГОТОВ, ВИЖУ, ВЫП. (ready, a target seen, launched),
	// ТЕХ. СОСТОЯНИЕ РПК, БЧ, ШАССИ (faults: none modelled, dark), lit as the post last heard on the battery net
	// (command_post.hpp BoardLampsOf). The lamps are small bulbs and ease on and off.
	static const TCHAR* Groups[] = {TEXT("КОМПЛЕКТ"), TEXT("РАБОТА"), TEXT("ТЕХ. СОСТОЯНИЕ")};
	static const TCHAR* Legends[] = {TEXT("1"), TEXT("1/2"), TEXT("1/4"), TEXT("ГОТОВ"), TEXT("ВИЖУ"), TEXT("ВЫП."), TEXT("РПК"),
		TEXT("БЧ"), TEXT("ШАССИ")};
	constexpr int32 Rows = 5;     // four launchers and the support vehicles (ТЗМ, МТО)
	constexpr int32 Lamps = 9;
	const FLinearColor Plate(0.09f, 0.092f, 0.095f);   // light grey under the board's gain
	const FLinearColor Ink(0.006f, 0.006f, 0.006f);
	const FLinearColor LampLit(1.0f, 0.55f, 0.1f);
	const FLinearColor LampDark(0.05f, 0.025f, 0.008f);
	Fill(Canvas, FVector2D::ZeroVector, Size, Plate);
	const double X0 = 0.14 * Size.X;        // the НОМЕР column to the left of it
	const double Cw = (Size.X - X0 - 0.02 * Size.X) / Lamps;
	const double HeaderH = 0.24 * Size.Y;
	const double Rh = (Size.Y - HeaderH - 0.03 * Size.Y) / Rows;
	for (int32 G = 0; G < 3; ++G)
	{
		const double Gx = X0 + Cw * 3 * G;
		Text(Canvas, Groups[G], FVector2D(Gx + 1.5 * Cw, 0.06 * Size.Y), Ink, 0.75f);
		Line(Canvas, FVector2D(Gx + 0.1 * Cw, 0.11 * Size.Y), FVector2D(Gx + 2.9 * Cw, 0.11 * Size.Y), 1.0f, Ink);
		if (G > 0)
		{
			Line(Canvas, FVector2D(Gx, 0.03 * Size.Y), FVector2D(Gx, Size.Y - 0.02 * Size.Y), 1.0f, Ink);
		}
	}
	for (int32 L = 0; L < Lamps; ++L)
	{
		Text(Canvas, Legends[L], FVector2D(X0 + Cw * (L + 0.5), 0.17 * Size.Y), Ink, 0.62f);
	}
	Text(Canvas, TEXT("НОМЕР"), FVector2D(0.5 * X0, 0.12 * Size.Y), Ink, 0.62f);
	if (BoardGlow.Num() != Rows * Lamps)
	{
		BoardGlow.Init(0.0f, Rows * Lamps);
	}
	const double Lr = 0.3 * FMath::Min(Cw, Rh);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const double Y = HeaderH + Rh * (Row + 0.5);
		const simcore::LauncherStatus* St = Row < 4 && Row < static_cast<int32>(Post.launchers.size()) ? &Post.launchers[Row] : nullptr;
		Text(Canvas, Row < 4 ? FString::FromInt(Row + 1) : FString(TEXT("ТЗМ")), FVector2D(0.5 * X0, Y), Ink, 0.75f);
		simcore::BoardLamps B;
		if (St != nullptr && bOn)
		{
			B = simcore::BoardLampsOf(*St);
		}
		const bool Lit[Lamps] = {B.load == 1, B.load == 2, B.load == 3, B.ready, B.sees, B.fired, B.fault, B.fault, B.fault};
		for (int32 L = 0; L < Lamps; ++L)
		{
			float& Glow = BoardGlow[Row * Lamps + L];
			const float Tau = Lit[L] ? 0.035f : 0.09f;
			Glow += ((Lit[L] ? 1.0f : 0.0f) - Glow) * (1.0f - FMath::Exp(-DeltaTime / Tau));
			const FVector2D At(X0 + Cw * (L + 0.5), Y);
			Disc(Canvas, At, Lr * 1.25, FLinearColor(0.03f, 0.03f, 0.03f));  // the lamp's black holder
			const FLinearColor Cap = FMath::Lerp(LampDark, FMath::Lerp(LampLit * FLinearColor(1.0f, 0.5f, 0.25f), LampLit, Glow), Glow);
			if (Glow > 0.02f)
			{
				Disc(Canvas, At, Lr * 1.6, LampLit * (0.12f * Glow));  // its glow on the plate
			}
			Disc(Canvas, At, Lr, Cap);
			Disc(Canvas, At - FVector2D(0.3 * Lr, 0.3 * Lr), 0.3 * Lr, Cap * 1.3f + FLinearColor(0.02f, 0.02f, 0.02f));  // the facet's highlight
		}
	}
}

// ---------------------------------------------------------------------------------------------- TV

void USimCabinDisplayComponent::UpdateTv(const USimWorldSubsystem* Sim, simcore::ScenarioRunner& Runner, bool bOn)
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part != nullptr && Part != Parts[0])
		{
			Part->SetVisibility(bOn);  // crosshair
		}
	}
	if (Capture == nullptr)
	{
		return;
	}
	Capture->bCaptureEveryFrame = false;  // filmed by hand below, at sam3d.TvHz
	if (!bOn)
	{
		// A dark screen. Cleared on each redraw, not just when switched off: a capture can land in the target before the
		// first update (found by tools/ue_cabin_check.py: a picture on a TV that was never on).
		if (IsSeen() && (bTvWasOn || RedrawSecondsLeft <= 0.0f))
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, Target, MonitorGlass);
			RedrawSecondsLeft = 0.5f;
		}
		bTvWasOn = false;
		return;
	}
	bTvWasOn = true;
	// The camera sits on the antenna unit and looks along the tracking radar's azimuth and elevation (the wheel's
	// azimuth is in the crew's frame; the camera needs it from true north).
	const simcore::ConsoleData& Console = Runner.Console();
	const double Az = FMath::DegreesToRadians(Feed.IndicatedToTrueDeg(Console.panel.GetValue(Console.track.az_wheel)));
	const double El = FMath::DegreesToRadians(Console.panel.GetValue(Console.track.el_wheel));
	const simcore::Vec3 Dir{FMath::Sin(Az) * FMath::Cos(El), FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(El)};
	const simcore::Vec3& Unit = Feed.unit_position;
	const FVector From = Sim->ToUnreal(Unit);
	const FVector To = Sim->ToUnreal(Unit + Dir * 1000.0);
	Capture->SetWorldLocationAndRotation(From, (To - From).Rotation());
	Capture->FOVAngle = static_cast<float>(FMath::Max(0.1, simcore::TvFieldOfViewDeg(Console)));
	// Each picture renders the scene a second time: only while someone in the cabin can see the monitor, at a TV's
	// rate rather than the game's.
	if (IsSeen() && RedrawSecondsLeft <= 0.0f)
	{
		RedrawSecondsLeft = 1.0f / FMath::Max(1.0f, CVarTvHz.GetValueOnGameThread());
		Capture->CaptureScene();
	}
}
