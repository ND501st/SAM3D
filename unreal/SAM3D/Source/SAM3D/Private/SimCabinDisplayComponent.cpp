#include "SimCabinDisplayComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "SimWorldSubsystem.h"

namespace
{
	const FLinearColor Phosphor(0.3f, 1.0f, 0.4f);
	const FLinearColor TraceGreen(0.4f, 1.0f, 0.5f);
	const FLinearColor Amber(1.0f, 0.65f, 0.15f);

	// Screen point at Angle (clockwise from up) and Radius from Centre; screen Y grows downward.
	FVector2D Polar(const FVector2D& Centre, double AngleRad, double Radius)
	{
		return Centre + FVector2D(FMath::Sin(AngleRad), -FMath::Cos(AngleRad)) * Radius;
	}

	void Circle(UCanvas* Canvas, const FVector2D& Centre, double Radius, const FLinearColor& Color, float Thickness = 1.0f)
	{
		constexpr int32 Segments = 72;
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
	const double WidthCm = Display.width_m * 100.0;
	const double HeightCm = Display.height_m * 100.0;

	// Bezel: a dark box just behind the screen, a little larger.
	if (Cube != nullptr)
	{
		UStaticMeshComponent* Bezel = NewObject<UStaticMeshComponent>(GetOwner());
		Bezel->SetStaticMesh(Cube);
		Bezel->SetupAttachment(GetAttachParent());
		Bezel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bezel->SetCastShadow(false);
		Bezel->RegisterComponent();
		Bezel->SetRelativeLocationAndRotation(Centre + Normal * 0.5, FRotationMatrix::MakeFromXZ(Normal, Up).Rotator());
		Bezel->SetRelativeScale3D(FVector(0.012, Display.width_m + 0.02, Display.height_m + 0.02));
		if (ShapeMaterial != nullptr)
		{
			Bezel->CreateDynamicMaterialInstance(0, ShapeMaterial)->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.01f, 0.01f));
		}
		Parts.Add(Bezel);
	}

	// The screen: a quad 1.3 cm out from the panel with texture (0,0) at the operator's top left. Both windings, so
	// it shows whichever way the material culls.
	Screen = NewObject<UProceduralMeshComponent>(GetOwner());
	Screen->SetupAttachment(GetAttachParent());
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCastShadow(false);
	Screen->RegisterComponent();
	const FVector C = Centre + Normal * 1.3;
	const FVector Hr = Right * (0.5 * WidthCm);
	const FVector Hu = Up * (0.5 * HeightCm);
	const TArray<FVector> Vertices = {C - Hr + Hu, C + Hr + Hu, C - Hr - Hu, C + Hr - Hu};
	const TArray<FVector2D> Uv = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1)};
	const TArray<int32> Triangles = {0, 1, 2, 2, 1, 3, 0, 2, 1, 2, 3, 1};
	const TArray<FVector> Normals = {Normal, Normal, Normal, Normal};
	Screen->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, Uv, {}, {}, false);

	const double Long = FMath::Max(WidthCm, HeightCm);
	const int32 W = FMath::Max(16, FMath::RoundToInt32(PixelsPerLongSide * WidthCm / Long));
	const int32 H = FMath::Max(16, FMath::RoundToInt32(PixelsPerLongSide * HeightCm / Long));
	Target = UKismetRenderingLibrary::CreateRenderTarget2D(this, W, H, RTF_RGBA8, FLinearColor::Black);
	if (ScreenMaterialBase != nullptr && Target != nullptr)
	{
		ScreenMaterial = UMaterialInstanceDynamic::Create(ScreenMaterialBase, this);
		ScreenMaterial->SetTextureParameterValue(TEXT("SlateUI"), Target);
		ScreenMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
		ScreenMaterial->SetVectorParameterValue(TEXT("BackColor"), FLinearColor::Black);
		ScreenMaterial->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
		Screen->SetMaterial(0, ScreenMaterial);
	}

	if (Kind == simcore::CabinDisplayKind::Tv)
	{
		Capture = NewObject<USceneCaptureComponent2D>(GetOwner());
		Capture->RegisterComponent();
		Capture->TextureTarget = Target;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		// A black-and-white picture, as on the TOV monitor.
		Capture->PostProcessSettings.bOverride_ColorSaturation = true;
		Capture->PostProcessSettings.ColorSaturation = FVector4(0.0, 0.0, 0.0, 1.0);
		Capture->PostProcessBlendWeight = 1.0f;
		Capture->HideActorComponents(GetOwner());
		// Crosshair over the picture.
		for (int32 I = 0; I < 2 && Cube != nullptr; ++I)
		{
			UStaticMeshComponent* Line = NewObject<UStaticMeshComponent>(GetOwner());
			Line->SetStaticMesh(Cube);
			Line->SetupAttachment(GetAttachParent());
			Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Line->SetCastShadow(false);
			Line->RegisterComponent();
			Line->SetRelativeLocationAndRotation(Centre + Normal * 1.4, FRotationMatrix::MakeFromXZ(Normal, Up).Rotator());
			Line->SetRelativeScale3D(I == 0 ? FVector(0.001, 0.3 * Display.width_m, 0.0015) : FVector(0.001, 0.0015, 0.3 * Display.height_m));
			if (ShapeMaterial != nullptr)
			{
				Line->CreateDynamicMaterialInstance(0, ShapeMaterial)->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
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

	if (Kind == simcore::CabinDisplayKind::Tv)
	{
		UpdateTv(Sim, *Runner, bOn);
		return;
	}
	if (Kind == simcore::CabinDisplayKind::Ppi)
	{
		// The sweep follows simulated time, so it stops when the sim is paused.
		const double Now = Runner->Time();
		const double Dt = LastSimTimeSeconds < 0.0 ? 0.0 : FMath::Clamp(Now - LastSimTimeSeconds, 0.0, 1.0);
		LastSimTimeSeconds = Now;
		const bool bRotating = Console.has_sensors && Console.equipment.IsReady(Console.search.antenna);
		Scope->Update(Dt, bRotating, Runner->Engagement().SearchPicture());
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(this, Target, FLinearColor::Black);
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
		case simcore::CabinDisplayKind::Dial:
			DrawDial(Canvas, Size, Display, Console, bOn, simcore::CabinLayout::DisplayLit(Display, Console.panel, Console.equipment));
			break;
		case simcore::CabinDisplayKind::Meter:
			DrawMeter(Canvas, Size, Display, Console, bOn);
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
	if (!bOn)
	{
		return;
	}
	const simcore::ConsoleData& Console = Runner.Console();
	const simcore::SearchSensorDef& Search = Console.search;
	const FVector2D C = Size * 0.5;
	const double R = 0.46 * FMath::Min(Size.X, Size.Y);
	Disc(Canvas, C, R, Phosphor * 0.04f);

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
		Circle(Canvas, C, RadiusOf(K), Phosphor * 0.25f);
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

	// Fading echoes.
	for (const simcore::SearchScope::Blip& B : Scope->Blips())
	{
		if (B.range_m < MinM || B.range_m > MaxM)
		{
			continue;
		}
		Disc(Canvas, Polar(C, B.azimuth_rad, RadiusOf(B.range_m)), 3.5, Phosphor * static_cast<float>(B.brightness));
	}

	// The sweep, with a short fading trail.
	if (Console.equipment.IsReady(Search.antenna))
	{
		const double Sweep = Scope->SweepAzimuthRad();
		for (int32 I = 6; I >= 0; --I)
		{
			const double A = Sweep - FMath::DegreesToRadians(2.5 * I);
			Canvas->K2_DrawLine(C, Polar(C, A, R), 2.0f, Phosphor * (I == 0 ? 0.9f : 0.25f / I));
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
	Text(Canvas, FString::Printf(TEXT("%.0f-%.0f km"), MinM / 1000.0, MaxM / 1000.0), FVector2D(Size.X - 52.0, 18.0),
		Phosphor * 0.7f, 1.0f);
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
	const std::vector<simcore::Contact>& Returns = Runner.Engagement().TrackReturns();
	const double X0 = 0.05 * Size.X;
	const double X1 = 0.95 * Size.X;

	// One sweep: baseline, grass while radiating, echo spikes, gate marks. [FromM, ToM] maps to X0..X1.
	auto Sweep = [&](double Y, double FromM, double ToM, double SpikeH)
	{
		auto XOf = [&](double M) { return X0 + (M - FromM) / (ToM - FromM) * (X1 - X0); };
		double PrevX = X0;
		double PrevY = Y;
		for (double X = X0; X <= X1; X += 3.0)
		{
			const double Ny = bRadiating ? Noise.FRandRange(0.0, 3.0) : 0.0;
			Canvas->K2_DrawLine(FVector2D(PrevX, PrevY), FVector2D(X, Y - Ny), 1.5f, TraceGreen * 0.8f);
			PrevX = X;
			PrevY = Y - Ny;
		}
		for (const simcore::Contact& Echo : Returns)
		{
			if (Echo.range_m < FromM || Echo.range_m > ToM)
			{
				continue;
			}
			const double X = XOf(Echo.range_m);
			Canvas->K2_DrawLine(FVector2D(X - 3.0, Y), FVector2D(X, Y - SpikeH), 2.0f, TraceGreen);
			Canvas->K2_DrawLine(FVector2D(X, Y - SpikeH), FVector2D(X + 3.0, Y), 2.0f, TraceGreen);
		}
		// The range gate: a notch under the trace.
		for (double M : {Gate - HalfGate, Gate + HalfGate})
		{
			if (M >= FromM && M <= ToM)
			{
				Canvas->K2_DrawLine(FVector2D(XOf(M), Y + 2.0), FVector2D(XOf(M), Y + 12.0), 2.0f, Amber);
			}
		}
	};
	Sweep(0.42 * Size.Y, 0.0, MaxM, 0.22 * Size.Y);
	const double Half = 0.5 * Display.magnified_window_m;
	Sweep(0.85 * Size.Y, Gate - Half, Gate + Half, 0.25 * Size.Y);
	Canvas->K2_DrawLine(FVector2D(0.5 * Size.X, 0.6 * Size.Y), FVector2D(0.5 * Size.X, 0.88 * Size.Y), 1.0f, Amber * 0.5f);

	Text(Canvas, TEXT("0"), FVector2D(X0, 0.48 * Size.Y), TraceGreen * 0.6f, 1.0f);
	Text(Canvas, FString::Printf(TEXT("%.0f km"), MaxM / 1000.0), FVector2D(X1 - 12.0, 0.48 * Size.Y), TraceGreen * 0.6f, 1.0f);
	// The range counter: where the gate is (it follows the target while tracking in range).
	Text(Canvas, FString::Printf(TEXT("%05.0f m"), FMath::Max(0.0, Gate)), FVector2D(0.5 * Size.X, 0.1 * Size.Y), Amber, 1.6f);
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
	Canvas->K2_DrawPolygon(nullptr, Size * 0.5, Size * 0.72, 4, Face);  // square face, corners off-screen
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
		if (bTvWasOn)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, Target, FLinearColor::Black);
		}
		bTvWasOn = false;
		Capture->bCaptureEveryFrame = false;
		return;
	}
	bTvWasOn = true;
	// The camera sits on the antenna unit and looks along the tracking radar's azimuth (from north) and elevation.
	const simcore::ConsoleData& Console = Runner.Console();
	const double Az = FMath::DegreesToRadians(Console.panel.GetValue(Console.track.az_wheel));
	const double El = FMath::DegreesToRadians(Console.panel.GetValue(Console.track.el_wheel));
	const simcore::Vec3 Dir{FMath::Sin(Az) * FMath::Cos(El), FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(El)};
	const simcore::Vec3& Unit = Runner.UnitPosition();
	const FVector From = Sim->ToUnreal(Unit);
	const FVector To = Sim->ToUnreal(Unit + Dir * 1000.0);
	Capture->SetWorldLocationAndRotation(From, (To - From).Rotation());
	Capture->FOVAngle = static_cast<float>(FMath::Max(0.1, simcore::TvFieldOfViewDeg(Console)));
	Capture->bCaptureEveryFrame = true;
}
