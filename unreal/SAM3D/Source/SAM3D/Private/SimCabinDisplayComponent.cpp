#include "SimCabinDisplayComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "SimCabinComponent.h"
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

	// A beam on a phosphor: the bright core and a faint wide halo round it.
	void Beam(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
	{
		Canvas->K2_DrawLine(A, B, Thickness * 3.0f, Color * 0.16f);
		Canvas->K2_DrawLine(A, B, Thickness, Color);
	}

	// Screen point at Angle (clockwise from up) and Radius from Centre; screen Y grows downward.
	FVector2D Polar(const FVector2D& Centre, double AngleRad, double Radius)
	{
		return Centre + FVector2D(FMath::Sin(AngleRad), -FMath::Cos(AngleRad)) * Radius;
	}

	void Circle(UCanvas* Canvas, const FVector2D& Centre, double Radius, const FLinearColor& Color, float Thickness = 1.0f)
	{
		constexpr int32 Segments = 180;
		for (int32 I = 0; I < Segments; ++I)
		{
			const double A0 = UE_DOUBLE_TWO_PI * I / Segments;
			const double A1 = UE_DOUBLE_TWO_PI * (I + 1) / Segments;
			Canvas->K2_DrawLine(Polar(Centre, A0, Radius), Polar(Centre, A1, Radius), Thickness, Color);
		}
	}

	void Disc(UCanvas* Canvas, const FVector2D& Centre, double Radius, const FLinearColor& Color)
	{
		Canvas->K2_DrawPolygon(nullptr, Centre, FVector2D(Radius, Radius), 48, Color);
	}

	void Text(UCanvas* Canvas, const FString& S, const FVector2D& At, const FLinearColor& Color, float Scale, bool bCentre = true)
	{
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
			Canvas->K2_DrawLine(FVector2D(X, C.Y - Half), FVector2D(X, C.Y + Half), 1.0f, AmberGlass * 0.6f);
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
			: Kind == simcore::CabinDisplayKind::PlotBoard ? 3.5f
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

void USimCabinDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
			DrawStatusBoard(Canvas, Size, Sim->GetPostFeed(), bOn, DeltaTime);
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
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

// ---------------------------------------------------------------------------------------------- search scope

void USimCabinDisplayComponent::DrawPpi(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	simcore::ScenarioRunner& Runner, bool bOn)
{
	const FVector2D C = Size * 0.5;
	const double S = FMath::Min(Size.X, Size.Y);
	const double R = 0.43 * S;
	// The azimuth scale around the tube, as on OP81-16M (samsim_panels): 0 to 59 clockwise from the top (hundreds of
	// thousandths, 6000 to the circle), light marks on a dark ring, with a tick every half division.
	const FLinearColor ScaleInk(0.32f, 0.32f, 0.3f);
	Disc(Canvas, C, 0.5 * S, FLinearColor(0.012f, 0.012f, 0.012f));
	for (int32 I = 0; I < 120; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 120.0;
		Canvas->K2_DrawLine(Polar(C, A, 0.437 * S), Polar(C, A, (I % 2 == 0 ? 0.452 : 0.446) * S), 1.0f, ScaleInk);
	}
	for (int32 I = 0; I < 60; ++I)
	{
		Text(Canvas, FString::FromInt(I), Polar(C, UE_DOUBLE_TWO_PI * I / 60.0, 0.474 * S), ScaleInk, 0.8f);
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

	// Range rings every 5 km (10 km on long scales), bearing ticks every 30 degrees.
	const double Ring = Span > 20000.0 ? 10000.0 : 5000.0;
	for (double K = FMath::CeilToDouble((MinM + 1.0) / Ring) * Ring; K <= MaxM + 1.0; K += Ring)
	{
		Circle(Canvas, C, RadiusOf(K), Phosphor * 0.3f, 1.6f);
	}
	for (int32 I = 0; I < 12; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 12.0;
		Canvas->K2_DrawLine(Polar(C, A, R * 0.95), Polar(C, A, R), 1.5f, Phosphor * 0.5f);
	}

	const bool bRadiating = Console.equipment.IsReady(Search.transmitter) && Console.equipment.IsReady(Search.antenna);
	const bool bMti = !Search.mti_control.empty() && Console.panel.Get(Search.mti_control) == 1;
	if (bRadiating && !bMti && MinM <= 0.0)
	{
		// Ground clutter around the centre without MTI (picture only; the model hides low targets in it).
		const double ClutterR = RadiusOf(4000.0);
		for (int32 I = 0; I < 90; ++I)
		{
			const FVector2D P = Polar(C, Noise.FRandRange(0.0, UE_DOUBLE_TWO_PI), ClutterR * FMath::Sqrt(Noise.FRand()));
			Canvas->K2_DrawPolygon(nullptr, P, FVector2D(1.5, 1.5), 4, Phosphor * Noise.FRandRange(0.2f, 0.7f));
		}
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
			const float Glow = static_cast<float>(B.brightness);
			for (int32 K = -2; K <= 2; ++K)
			{
				const double A = B.azimuth_rad + FMath::DegreesToRadians(0.6 * K);
				Canvas->K2_DrawLine(C, Polar(C, A, R), K == 0 ? 3.0f : 1.5f, Phosphor * (Glow * (K == 0 ? 0.9f : 0.4f)));
			}
			for (int32 I = 0; I < 28; ++I)
			{
				const FVector2D P = Polar(C, B.azimuth_rad + Noise.FRandRange(-0.015, 0.015), R * Noise.FRand());
				Canvas->K2_DrawPolygon(nullptr, P, FVector2D(1.5, 1.5), 4, Phosphor * (Glow * Noise.FRandRange(0.3f, 1.0f)));
			}
			continue;
		}
		if (B.range_m < MinM || B.range_m > MaxM)
		{
			continue;
		}
		if (B.iff_reply)
		{
			const double Half = FMath::DegreesToRadians(bGated ? 2.0 : 10.0);
			const float Glow = static_cast<float>(B.brightness);
			for (int32 Arc = 0; Arc < (B.control_reply ? 2 : 1); ++Arc)
			{
				const double Rr = RadiusOf(B.range_m) + 9.0 + 5.0 * Arc;
				for (int32 I = 0; I < 8; ++I)
				{
					const double A0 = B.azimuth_rad - Half + 2.0 * Half * I / 8.0;
					const double A1 = B.azimuth_rad - Half + 2.0 * Half * (I + 1) / 8.0;
					Canvas->K2_DrawLine(Polar(C, A0, Rr), Polar(C, A1, Rr), 2.0f, Phosphor * Glow);
				}
			}
			continue;
		}
		// A bright arc-shaped smear when fresh, fading with the afterglow.
		const float Glow = static_cast<float>(B.brightness);
		const FVector2D P = Polar(C, B.azimuth_rad, RadiusOf(B.range_m));
		Disc(Canvas, P, 7.0, Phosphor * (0.35f * Glow));
		Disc(Canvas, P, 4.0, FLinearColor::LerpUsingHSV(Phosphor, FLinearColor::White, 0.5f) * Glow);
	}

	// The sweep, with a short fading trail.
	if (Console.equipment.IsReady(Search.antenna))
	{
		const double Sweep = Scope->SweepAzimuthRad();
		for (int32 I = 6; I >= 0; --I)
		{
			const double A = Sweep - FMath::DegreesToRadians(2.5 * I);
			Beam(Canvas, C, Polar(C, A, R), 2.0f, Phosphor * (I == 0 ? 0.9f : 0.25f / I));
		}
	}

	// The SSC azimuth line and its range gate, which the range officer puts on the target mark.
	if (Console.has_sensors)
	{
		const simcore::TrackSensorDef& Track = Console.track;
		const double Az = FMath::DegreesToRadians(Console.panel.GetValue(Track.az_wheel));
		Canvas->K2_DrawLine(C, Polar(C, Az, R), 1.5f, Amber * 0.7f);
		const double Gate = Console.panel.GetValue(Track.range_wheel);
		if (Gate >= MinM && Gate <= MaxM)
		{
			const double Rg = RadiusOf(Gate);
			Canvas->K2_DrawLine(Polar(C, Az - 0.06, Rg), Polar(C, Az + 0.06, Rg), 2.0f, Amber);
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
				Canvas->K2_DrawLine(FVector2D(XOf(M), Y + 3.0), FVector2D(XOf(M), Y + (bMajor ? 11.0 : 7.0)), 1.0f, TraceGreen * 0.4f);
			}
		}
		double PrevX = X0;
		double PrevY = Y;
		for (double X = X0; X <= X1; X += 3.0)
		{
			const double Ny = bRadiating ? Noise.FRandRange(0.0, 3.0) : 0.0;
			Beam(Canvas, FVector2D(PrevX, PrevY), FVector2D(X, Y - Ny), 1.2f, TraceGreen * 0.8f);
			PrevX = X;
			PrevY = Y - Ny;
		}
		auto Spike = [&](double RangeM)
		{
			if (RangeM < FromM || RangeM > ToM)
			{
				return;
			}
			const double X = XOf(RangeM);
			Beam(Canvas, FVector2D(X - 3.0, Y), FVector2D(X, Y - SpikeH), 1.6f, TraceGreen);
			Beam(Canvas, FVector2D(X, Y - SpikeH), FVector2D(X + 3.0, Y), 1.6f, TraceGreen);
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
				Canvas->K2_DrawLine(FVector2D(XOf(M), Y + 2.0), FVector2D(XOf(M), Y + 12.0), 2.0f, Amber);
			}
		}
	};
	Sweep(0.42 * Size.Y, 0.0, MaxM, 0.22 * Size.Y, 1000.0, false);
	const double Half = 0.5 * Display.magnified_window_m;
	Sweep(0.85 * Size.Y, FineCentre - Half, FineCentre + Half, 0.25 * Size.Y, 5.0, SweepChannel != 0);
	Canvas->K2_DrawLine(FVector2D(0.5 * Size.X, 0.6 * Size.Y), FVector2D(0.5 * Size.X, 0.88 * Size.Y), 1.0f, Amber * 0.5f);

	// The range counter: where the gate is (it follows the target while tracking in range), as five lit drum digits
	// like the readout strip's, not a text line painted on the picture.
	const FString Digits = FString::Printf(TEXT("%05.0f"), FMath::Max(0.0, Gate));
	const double CellW = 0.055 * Size.X;
	const double CellH = 0.13 * Size.Y;
	for (int32 I = 0; I < 5; ++I)
	{
		// each digit in its own lit cell: a vertical bar CellW wide, with a gap to the next
		const FVector2D At(0.5 * Size.X + (I - 2) * CellW * 1.4, 0.1 * Size.Y);
		Canvas->K2_DrawLine(At - FVector2D(0.0, 0.5 * (CellH - CellW)), At + FVector2D(0.0, 0.5 * (CellH - CellW)),
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

	// The raster: faint horizontal lines over the beam's band, with its outline.
	for (double Y = Top; Y <= Bottom; Y += 4.0)
	{
		Canvas->K2_DrawLine(FVector2D(Left, Y), FVector2D(Right, Y), 1.0f, Phosphor * 0.1f);
	}
	Canvas->K2_DrawLine(FVector2D(Left, Top), FVector2D(Right, Top), 1.5f, Phosphor * 0.28f);
	Canvas->K2_DrawLine(FVector2D(Left, Bottom), FVector2D(Right, Bottom), 1.5f, Phosphor * 0.28f);
	Canvas->K2_DrawLine(FVector2D(Left, Top), FVector2D(Left, Bottom), 1.5f, Phosphor * 0.28f);
	Canvas->K2_DrawLine(FVector2D(Right, Top), FVector2D(Right, Bottom), 1.5f, Phosphor * 0.28f);
	// The range gate sits in the middle of the range window.
	Canvas->K2_DrawLine(FVector2D(C.X, Bottom), FVector2D(C.X, Bottom + 6.0), 1.5f, Amber * 0.6f);

	// Where the antenna points now: a brighter line while the tracking radar radiates.
	const bool bRadiating = Console.has_sensors && Console.equipment.IsReady(Console.track.transmitter);
	const double AntennaY = YOfEl(Feed.antenna_el_deg);
	Canvas->K2_DrawLine(FVector2D(Left, AntennaY), FVector2D(Right, AntennaY), 3.0f, Phosphor * (bRadiating ? 1.0f : 0.45f));

	// Target marks: bright spots fading with the afterglow.
	for (const simcore::ElevationScope::Mark& M : ElevScope->Marks())
	{
		const float Glow = static_cast<float>(M.brightness);
		const FVector2D P(XOfOffset(M.range_offset_m), YOfEl(M.elevation_deg));
		Disc(Canvas, P, 9.0, Phosphor * (0.5f * Glow));
		Disc(Canvas, P, 5.0, FLinearColor::LerpUsingHSV(Phosphor, FLinearColor::White, 0.5f) * Glow);
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
		Canvas->K2_DrawLine(Polar(C, A, 0.85 * R), Polar(C, A, R), 2.0f, Ink);
		Text(Canvas, Label(Display.labels_per_turn * I / Display.marks), Polar(C, A, 0.68 * R), Ink, 1.3f);
	}
	if (!bOn)
	{
		return;
	}
	const double A = UE_DOUBLE_TWO_PI * Console.panel.GetValue(Display.control) / Display.value_per_turn;
	Canvas->K2_DrawLine(C, Polar(C, A, 0.8 * R), 3.0f, FLinearColor(0.7f, 0.05f, 0.02f));
	Disc(Canvas, C, 0.06 * R, Ink);
}

void USimCabinDisplayComponent::DrawMeter(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	const simcore::ConsoleData& Console, bool bOn)
{
	const FLinearColor Face(0.8f, 0.78f, 0.7f);
	const FLinearColor Ink(0.05f, 0.05f, 0.05f);
	// Fill the face: one line as thick as the face is tall.
	Canvas->K2_DrawLine(FVector2D(0.0, 0.5 * Size.Y), FVector2D(Size.X, 0.5 * Size.Y), static_cast<float>(Size.Y), Face);
	const FVector2D Pivot(0.5 * Size.X, 0.92 * Size.Y);
	const double R = 0.75 * Size.Y;
	constexpr double SwingDeg = 50.0;
	for (int32 I = 0; I <= Display.marks; ++I)
	{
		const double A = FMath::DegreesToRadians(-SwingDeg + 2.0 * SwingDeg * I / Display.marks);
		Canvas->K2_DrawLine(Polar(Pivot, A, 0.88 * R), Polar(Pivot, A, R), 2.0f, Ink);
		Text(Canvas, Label(Display.full_scale * I / Display.marks), Polar(Pivot, A, 0.75 * R), Ink, 1.2f);
	}
	Text(Canvas, FString::Printf(TEXT("%s  %s"), *FString(UTF8_TO_TCHAR(Display.label.c_str())), *FString(UTF8_TO_TCHAR(Display.unit.c_str()))),
		FVector2D(0.5 * Size.X, 0.55 * Size.Y), Ink, 1.3f);
	const double Reading = bOn ? Console.panel.GetValue(Display.control) * Display.scale : 0.0;
	const double Fraction = FMath::Clamp(Reading / Display.full_scale, 0.0, 1.0);
	const double A = FMath::DegreesToRadians(-SwingDeg + 2.0 * SwingDeg * Fraction);
	Canvas->K2_DrawLine(Pivot, Polar(Pivot, A, 0.95 * R), 2.5f, FLinearColor(0.7f, 0.05f, 0.02f));
}

// ---------------------------------------------------------------------------------------------- readout strip

void USimCabinDisplayComponent::DrawCounters(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
	simcore::ScenarioRunner& Runner, bool bOn)
{
	// Eight round amber-lit digit counters in a pale window (unit 0094-5, as baked into the panel face by
	// tools/cabin_textures.py), each reading its place's value (simcore/readouts.hpp). Unpowered, the lamps are dark and
	// the digits unreadable.
	const simcore::ConsoleData& Console = Runner.Console();
	Canvas->K2_DrawLine(FVector2D(0.0, 0.5 * Size.Y), FVector2D(Size.X, 0.5 * Size.Y), static_cast<float>(Size.Y),
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
					Canvas->K2_DrawLine(Ends[S][0], Ends[S][1], Thickness, Color);
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
				Canvas->K2_DrawLine(Polar(C, A0, Rr), Polar(C, A1, Rr), 1.5f, OnFace(Afterglow(BrightAt(0.5 * (A0 + A1))) * 0.16f));
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
					Canvas->K2_DrawPolygon(nullptr, P, FVector2D(1.2, 1.2), 4, OnFace(Afterglow(B) * Speckle.FRandRange(0.04f, 0.16f)));
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
			Canvas->K2_DrawLine(C, Polar(C, A, R), 2.0f, OnFace(Afterglow(BrightAt(A)) * (0.05f / I)));
		}
		Canvas->K2_DrawLine(C, Polar(C, AspdSweepRad, R), 1.5f, OnFace(AspdFlash * 0.3f));

		// The character generator's symbols.
		const int32 KeypadTarget = Post.keypad.empty() ? -1 : FCString::Atoi(UTF8_TO_TCHAR(Post.keypad.c_str()));
		for (const simcore::CommandPost::ScopeMark& M : Post.scope)
		{
			const bool bMarked = M.number == Post.marked;
			const bool bKeyed = M.number == KeypadTarget;
			const FVector2D At = ScreenOf(M.x, M.y);
			const FVector2D Tip = At + FVector2D(M.vx, -M.vy) * R;
			const FLinearColor Ink = OnFace(AspdSymbol * (bMarked || bKeyed ? 0.9f : (M.friendly ? 0.4f : 0.6f)));
			const float T = bMarked ? 2.2f : 1.5f;
			Canvas->K2_DrawLine(At, Tip, T, Ink);
			const double K = 0.011 * S;
			if (M.zone <= 1)
			{
				Circle(Canvas, At, K, Ink, T);
			}
			else if (M.zone == 2)
			{
				Canvas->K2_DrawBox(At - FVector2D(K, K), FVector2D(2.0 * K, 2.0 * K), T, Ink);
			}
			else
			{
				const FVector2D P0 = At + FVector2D(0.0, -1.25 * K);
				const FVector2D P1 = At + FVector2D(1.1 * K, 0.8 * K);
				const FVector2D P2 = At + FVector2D(-1.1 * K, 0.8 * K);
				Canvas->K2_DrawLine(P0, P1, T, Ink);
				Canvas->K2_DrawLine(P1, P2, T, Ink);
				Canvas->K2_DrawLine(P2, P0, T, Ink);
			}
			if (M.group)
			{
				Circle(Canvas, At, 1.7 * K, Ink, 1.0f);
			}
			if (M.friendly)
			{
				Canvas->K2_DrawLine(At + FVector2D(-1.7 * K, -1.8 * K), At + FVector2D(1.7 * K, -1.8 * K), T, Ink);
			}
			const double Gw = 0.012 * S;
			StrokeNumber(Canvas, FString::Printf(TEXT("%02d"), M.number), At + FVector2D(1.9 * K, -3.4 * K), Gw, 1.6 * Gw, T, Ink, 0.0, false);
			if (bKeyed)
			{
				// the number the ДХ is on: bracketed
				const FVector2D B0 = At - FVector2D(2.6 * K, 2.6 * K);
				Canvas->K2_DrawBox(B0, FVector2D(5.2 * K, 5.2 * K), 1.0f, Ink);
			}
			if (bMarked && Post.marked_arrival_s >= 0.0)
			{
				Text(Canvas, ArrivalText(Post.marked_arrival_s), At + FVector2D(2.0 * K, 1.4 * K), OnFace(AspdSymbol * 0.9f), 1.0f, false);
			}
		}

		// The DT-2's marker: a small ring open at the quarters.
		const FVector2D Mk = ScreenOf(Post.marker_x, Post.marker_y);
		const double Mr = 0.02 * S;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			for (int32 I = 0; I < 8; ++I)
			{
				const double A0 = UE_DOUBLE_HALF_PI * Q + 0.25 + 0.135 * I;
				Canvas->K2_DrawLine(Polar(Mk, A0, Mr), Polar(Mk, A0 + 0.135, Mr), 1.8f, OnFace(AspdFlash * 0.8f));
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
		Canvas->K2_DrawLine(FVector2D(U, C.Y - Hu), FVector2D(U, C.Y + Hu), 1.0f, Etch);
		Canvas->K2_DrawLine(FVector2D(C.X - Hv, V), FVector2D(C.X + Hv, V), 1.0f, Etch);
	}
	for (int32 I = 0; I < 36; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 36.0;
		Canvas->K2_DrawLine(Polar(C, A, R * (I % 3 == 0 ? 0.955 : 0.975)), Polar(C, A, R), 1.0f, Etch * 1.6f);
	}
}

void USimCabinDisplayComponent::DrawPlotBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn)
{
	// The plotter's air-situation map (csla_pu12_manual item 24; rwd_mb3, Luftlagekarte: a plexiglass board on which the
	// targets' tracks are drawn by hand in the designation squares, and their azimuth and range taken off; the photograph:
	// etched with a grid and range circles). The post at the centre, north up, 100 km to the edge (the ASPD-U's longest
	// scale, ru_pu12_text): circles every 25 km, squares of 25 km lettered along the top and numbered down the side. Each
	// target's last marks joined into its track in grease pencil, its number and its height over its code (the board's
	// notation, data/command/pu12.json). Spacing, lettering and the pencil's colours are SAM3D's.
	constexpr double RadiusM = 100000.0;
	const FVector2D C = Size * 0.5;
	const double R = 0.47 * FMath::Min(Size.X, Size.Y);
	Fill(Canvas, FVector2D::ZeroVector, Size, FLinearColor(0.5f, 0.5f, 0.48f));  // the plexiglass over its white backing
	const FLinearColor Etch(0.07f, 0.075f, 0.07f);
	const FLinearColor Ink(0.02f, 0.02f, 0.02f);
	const double Cell = R * 25000.0 / RadiusM;
	static const TCHAR* Letters[] = {TEXT("А"), TEXT("Б"), TEXT("В"), TEXT("Г"), TEXT("Д"), TEXT("Е"), TEXT("Ж"), TEXT("И"),
		TEXT("К"), TEXT("Л")};
	for (int32 I = -4; I <= 4; ++I)
	{
		const double X = C.X + I * Cell;
		const double Y = C.Y + I * Cell;
		Canvas->K2_DrawLine(FVector2D(X, C.Y - 4 * Cell), FVector2D(X, C.Y + 4 * Cell), 1.0f, Etch);
		Canvas->K2_DrawLine(FVector2D(C.X - 4 * Cell, Y), FVector2D(C.X + 4 * Cell, Y), 1.0f, Etch);
		if (I < 4)
		{
			Text(Canvas, Letters[I + 4], FVector2D(X + 0.5 * Cell, C.Y - 4 * Cell - 0.022 * Size.Y), Ink, 0.75f);
			Text(Canvas, FString::FromInt(I + 5), FVector2D(C.X - 4 * Cell - 0.03 * Size.X, Y + 0.5 * Cell), Ink, 0.75f);
		}
	}
	for (int32 K = 1; K <= 4; ++K)
	{
		Circle(Canvas, C, R * K / 4.0, Etch * 0.8f, 1.3f);
		Text(Canvas, FString::FromInt(25 * K), Polar(C, FMath::DegreesToRadians(45.0), R * K / 4.0) + FVector2D(4.0, -6.0), Ink, 0.6f, false);
	}
	for (int32 I = 0; I < 12; ++I)
	{
		const double A = UE_DOUBLE_TWO_PI * I / 12.0;
		Canvas->K2_DrawLine(Polar(C, A, 0.0), Polar(C, A, R), 1.0f, Etch * 0.6f);
	}
	Disc(Canvas, C, 0.012 * Size.X, Ink);  // the post
	for (const simcore::em::ViewBoardMark& Latest : simcore::LatestMarks(Post))
	{
		const FLinearColor Pencil = Latest.code / 10 == 1 ? PencilBlue : PencilRed;
		FVector2D Prev = FVector2D::ZeroVector;
		bool bHasPrev = false;
		for (const simcore::em::ViewBoardMark& M : Post.glass)
		{
			if (M.number != Latest.number || Latest.stamp - M.stamp > 4)
			{
				continue;
			}
			const simcore::Vec3 Rel = M.position - Post.position;
			const FVector2D P = C + FVector2D(Rel.x, -Rel.y) / RadiusM * R;
			// a short cross for each plot, joined by the track (the grease pencil's soft, waxy line)
			const double K = 0.007 * Size.X;
			Canvas->K2_DrawLine(P - FVector2D(K, 0.0), P + FVector2D(K, 0.0), 2.0f, Pencil);
			Canvas->K2_DrawLine(P - FVector2D(0.0, K), P + FVector2D(0.0, K), 2.0f, Pencil);
			if (bHasPrev)
			{
				Canvas->K2_DrawLine(Prev, P, 2.5f, Pencil * 0.9f);
			}
			Prev = P;
			bHasPrev = true;
		}
		if (bHasPrev)
		{
			Text(Canvas, FString::Printf(TEXT("%d"), Latest.number), Prev + FVector2D(0.02 * Size.X, -0.025 * Size.Y), Pencil, 1.0f, false);
			Text(Canvas, FString::Printf(TEXT("%d"), Latest.height_hm), Prev + FVector2D(0.02 * Size.X, 0.002 * Size.Y), Pencil, 0.8f, false);
			Canvas->K2_DrawLine(Prev + FVector2D(0.02 * Size.X, 0.03 * Size.Y), Prev + FVector2D(0.06 * Size.X, 0.03 * Size.Y), 1.5f, Pencil);
			Text(Canvas, FString::Printf(TEXT("%02d"), Latest.code), Prev + FVector2D(0.02 * Size.X, 0.034 * Size.Y), Pencil, 0.8f, false);
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
		Canvas->K2_DrawLine(FVector2D(Gx + 0.1 * Cw, 0.11 * Size.Y), FVector2D(Gx + 2.9 * Cw, 0.11 * Size.Y), 1.0f, Ink);
		if (G > 0)
		{
			Canvas->K2_DrawLine(FVector2D(Gx, 0.03 * Size.Y), FVector2D(Gx, Size.Y - 0.02 * Size.Y), 1.0f, Ink);
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
	if (!bOn)
	{
		// A dark screen. Cleared every frame, not just when switched off: a capture can land in the target
		// before the first update (found by tools/ue_cabin_check.py: a picture on a TV that was never on).
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Target, MonitorGlass);
		bTvWasOn = false;
		Capture->bCaptureEveryFrame = false;
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
	Capture->bCaptureEveryFrame = true;
}
