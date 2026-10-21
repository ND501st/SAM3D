#include "SimCabinHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "SimOsaVehicle.h"
#include "SimWorldSubsystem.h"

void ASimCabinHUD::DrawHUD()
{
	Super::DrawHUD();
	APlayerController* OwnerPc = GetOwningPlayerController();
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	UFont* SmallFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	if (OwnerPc != nullptr && OwnerPc->WasInputKeyJustPressed(EKeys::B))
	{
		bShowBoard = !bShowBoard;
	}
	if (Sim != nullptr && Canvas != nullptr)
	{
		DrawRadio(Sim, SmallFont);
		if (bShowBoard)
		{
			DrawBoard(Sim, SmallFont);
		}
	}
	const APlayerController* Pc = OwnerPc;
	const ASimOsaVehicle* Vehicle = Pc != nullptr ? Cast<ASimOsaVehicle>(Pc->GetPawn()) : nullptr;
	if (Vehicle == nullptr || !Vehicle->IsInCabin() || Canvas == nullptr)
	{
		return;
	}
	const float Cx = Canvas->ClipX * 0.5f;
	const float Cy = Canvas->ClipY * 0.5f;
	const FLinearColor Ink(1.0f, 1.0f, 1.0f, 0.8f);
	DrawLine(Cx - 8.0f, Cy, Cx + 8.0f, Cy, Ink, 1.5f);
	DrawLine(Cx, Cy - 8.0f, Cx, Cy + 8.0f, Ink, 1.5f);

	UFont* Font = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	const FString Hover = Vehicle->GetCabinHoverText();
	if (!Hover.IsEmpty())
	{
		DrawText(Hover, FLinearColor(1.0f, 0.9f, 0.4f), Cx + 16.0f, Cy + 12.0f, Font);
	}
	const FString Feedback = Vehicle->GetCabinFeedbackText();
	if (!Feedback.IsEmpty())
	{
		DrawText(Feedback, FLinearColor(1.0f, 0.4f, 0.3f), Cx + 16.0f, Cy + 32.0f, Font);
	}
	DrawText(FString::Printf(TEXT("%s   |   C leave cabin   Tab next seat   left/right click operate   drag or scroll wheels   B plotting board"),
				 *Vehicle->GetCabinStationLabel()),
		Ink, 20.0f, Canvas->ClipY - 40.0f, Font);
}

void ASimCabinHUD::DrawRadio(USimWorldSubsystem* Sim, UFont* Font)
{
	const double Now = Sim->GetSimTimeSeconds();
	const TArray<FSimRadioMessage> Messages = Sim->GetRadioMessages(RadioLines);
	float Y = Canvas->ClipY - 70.0f - 16.0f * Messages.Num();
	for (const FSimRadioMessage& M : Messages)
	{
		const double Age = Now - M.TimeSeconds;
		if (Age > RadioLineSeconds && Now > 0.0)
		{
			Y += 16.0f;
			continue;
		}
		const float Alpha = FMath::Clamp(1.0f - static_cast<float>(Age) / RadioLineSeconds, 0.35f, 1.0f);
		DrawText(FString::Printf(TEXT("[%s] %s: %s"), *M.Net, *M.From, *M.Text), FLinearColor(0.75f, 0.95f, 0.75f, Alpha), 20.0f, Y, Font);
		Y += 16.0f;
	}
}

void ASimCabinHUD::DrawBoard(USimWorldSubsystem* Sim, UFont* Font)
{
	// The battery commander's glass plotting board (samsim_osa p20): range rings at 50-200 km, the battery at the centre,
	// north up, each target's marks with its number, height over code, and the minute stamp; older plots dimmer.
	const float Size = Canvas->ClipY * 0.7f;
	const FVector2D Origin(Canvas->ClipX - Size - 30.0f, 40.0f);
	const FVector2D Centre = Origin + FVector2D(Size, Size) * 0.5f;
	DrawRect(FLinearColor(0.02f, 0.04f, 0.05f, 0.85f), Origin.X, Origin.Y, Size, Size);
	const TArray<float> Rings = Sim->GetBoardRangeRings();
	float MaxRange = 1.f;
	for (float R : Rings)
	{
		MaxRange = FMath::Max(MaxRange, R);
	}
	const float Scale = 0.48f * Size / MaxRange;  // px per metre
	const FLinearColor Grid(0.4f, 0.6f, 0.65f, 0.7f);
	for (float R : Rings)
	{
		const int32 Seg = 72;
		for (int32 I = 0; I < Seg; ++I)
		{
			const float A0 = 2.f * PI * I / Seg, A1 = 2.f * PI * (I + 1) / Seg;
			DrawLine(Centre.X + R * Scale * FMath::Sin(A0), Centre.Y - R * Scale * FMath::Cos(A0), Centre.X + R * Scale * FMath::Sin(A1),
				Centre.Y - R * Scale * FMath::Cos(A1), Grid, 1.0f);
		}
		DrawText(FString::Printf(TEXT("%.0f"), R / 1000.f), Grid, Centre.X + 3.f, Centre.Y - R * Scale - 12.f, Font);
	}
	DrawLine(Centre.X, Origin.Y + 4.f, Centre.X, Origin.Y + Size - 4.f, Grid * 0.6f, 1.0f);
	DrawLine(Origin.X + 4.f, Centre.Y, Origin.X + Size - 4.f, Centre.Y, Grid * 0.6f, 1.0f);
	DrawText(TEXT("N"), Grid, Centre.X - 4.f, Origin.Y + 2.f, Font);
	DrawRect(FLinearColor(1.f, 0.3f, 0.2f), Centre.X - 3.f, Centre.Y - 3.f, 6.f, 6.f);

	const TArray<FSimBoardMark> Marks = Sim->GetBoardMarks();
	int32 Newest = 0;
	for (const FSimBoardMark& M : Marks)
	{
		Newest = FMath::Max(Newest, M.Stamp);
	}
	TMap<int32, FVector2D> Last;  // the track of each number, joined from plot to plot
	for (int32 Stamp = Newest - 4; Stamp <= Newest; ++Stamp)
	{
		for (const FSimBoardMark& M : Marks)
		{
			if (M.Stamp != Stamp)
			{
				continue;
			}
			const FVector2D P = Centre + FVector2D(M.EastNorthMeters.X, -M.EastNorthMeters.Y) * Scale;
			const bool bCurrent = M.Stamp == Newest;
			const FLinearColor Ink = bCurrent ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.8f, 0.6f, 0.25f, 0.5f);
			if (const FVector2D* Prev = Last.Find(M.Number))
			{
				DrawLine(Prev->X, Prev->Y, P.X, P.Y, Ink, 1.0f);
			}
			Last.Add(M.Number, P);
			DrawRect(Ink, P.X - 2.f, P.Y - 2.f, 4.f, 4.f);
			DrawText(FString::Printf(TEXT("%02d"), M.Stamp % 100), Ink, P.X + 4.f, P.Y + 2.f, Font, 0.8f);
			if (bCurrent)
			{
				DrawText(FString::Printf(TEXT("%04d  %d/%02d"), M.Number, M.HeightHm, M.Code), Ink, P.X + 6.f, P.Y - 14.f, Font);
			}
		}
	}
	DrawText(FString::Printf(TEXT("PU-12 plotting board   plot %d   (B to close)"), Newest), Grid, Origin.X + 8.f, Origin.Y + Size - 18.f, Font);
}

