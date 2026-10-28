#include "SimCabinHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "SimCabinComponent.h"
#include "InputCoreTypes.h"
#include "SimOsaVehicle.h"
#include "SimViewComponent.h"
#include "SimWorldSubsystem.h"

void ASimCabinHUD::DrawHUD()
{
	Super::DrawHUD();
	GuideBottom = 0.0f;
	APlayerController* OwnerPc = GetOwningPlayerController();
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	UFont* SmallFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	if (OwnerPc != nullptr && OwnerPc->WasInputKeyJustPressed(EKeys::B))
	{
		bShowBoard = !bShowBoard;
	}
	const APlayerController* Pc = OwnerPc;
	const ASimOsaVehicle* Vehicle = Pc != nullptr ? Cast<ASimOsaVehicle>(Pc->GetPawn()) : nullptr;
	// The radio journal held up: the keyboard is the pencil's (digits, Backspace, Tab, Enter, S), nothing here hears it.
	if (Vehicle != nullptr && Vehicle->IsAtJournal())
	{
		OwnerPc = nullptr;
	}
	// G: every key; N: the guide's next step.
	if (OwnerPc != nullptr && Vehicle != nullptr && OwnerPc->WasInputKeyJustPressed(EKeys::G))
	{
		bShowKeys = !bShowKeys;
		if (bShowKeys && Sim != nullptr)
		{
			Sim->NoteTutorialEvent(TEXT("keys_card"));
		}
	}
	// The crew menu opened over the keys card: the card goes (the menu has the mouse and the keys, so it must be seen).
	if (bShowKeys && Vehicle != nullptr && Vehicle->IsCrewMenuOpen())
	{
		bShowKeys = false;
	}
	if (OwnerPc != nullptr && Sim != nullptr && Sim->CanSkipTutorialStep() && Vehicle != nullptr && !Vehicle->IsMissionMenuOpen() &&
		OwnerPc->WasInputKeyJustPressed(EKeys::N))
	{
		Sim->SkipTutorialStep();
	}
	// Backspace: back a step, to read it again.
	if (OwnerPc != nullptr && Sim != nullptr && Sim->HasTutorial() && Vehicle != nullptr && !Vehicle->IsMissionMenuOpen() &&
		OwnerPc->WasInputKeyJustPressed(EKeys::BackSpace))
	{
		Sim->BackTutorialStep();
	}
	if (Vehicle != nullptr && Sim != nullptr && Canvas != nullptr && Sim->IsScenarioRunning() && !Vehicle->IsMissionMenuOpen())
	{
		DrawView(Sim, Vehicle);
		DrawGuide(Sim, Vehicle);
	}
	if (Sim != nullptr && Canvas != nullptr && (Vehicle == nullptr || !Vehicle->IsMissionMenuOpen()))
	{
		const float CommsH = DrawComms(Sim, Vehicle, SmallFont, PostAidsTop(Sim, Vehicle) - 8.0f);
		DrawRadio(Sim, SmallFont, PostAidsTop(Sim, Vehicle) - 8.0f - CommsH, Vehicle != nullptr ? Vehicle->GetStationId() : FString(),
			Vehicle != nullptr && Vehicle->IsAssistOn());
		if (bShowBoard)
		{
			DrawBoard(Sim, SmallFont, Vehicle != nullptr ? Vehicle->GetStationId() : FString());
		}
	}
	if (Vehicle != nullptr && Sim != nullptr && Canvas != nullptr && Sim->IsScenarioRunning())
	{
		if (Vehicle->IsMissionMenuOpen())
		{
			DrawMissionMenu(Sim, Vehicle);
			if (bShowKeys)
			{
				DrawKeys(Vehicle);
			}
			return;
		}
		if (Vehicle->IsInstructorOpen())
		{
			DrawInstructor(Sim, Vehicle);
		}
		if (Sim->IsScenarioFinished())
		{
			UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
			const FString Ended = TEXT("The scenario has ended.   Esc: the debrief and the menu");
			float Tw = 0.0f;
			float Th = 0.0f;
			GetTextSize(Ended, Tw, Th, Medium);
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), (Canvas->ClipX - Tw) * 0.5f - 12.0f, 8.0f, Tw + 24.0f, Th + 10.0f);
			DrawText(Ended, FLinearColor(1.0f, 0.78f, 0.35f), (Canvas->ClipX - Tw) * 0.5f, 13.0f, Medium);
		}
	}
	// A battery link: the other vehicle is a key away.
	const FString SwitchHint = Sim != nullptr && Sim->IsBatteryLink() ? FString::Printf(TEXT("   V to %s"), *Sim->GetOtherLinkedVehicle()) : FString();
	if (Vehicle != nullptr && !Vehicle->IsInCabin() && Canvas != nullptr && Sim != nullptr && Sim->IsScenarioRunning())
	{
		const bool bFree = Vehicle->GetViews() != nullptr && Vehicle->GetViews()->IsFreeCamera();
		DrawText(FString::Printf(TEXT("%s   F1 the cabin   F2-F11 views   right mouse turns, wheel zooms   Ctrl+Z time   G keys   Esc menu%s"),
					 bFree ? TEXT("W A S D Q E fly") : TEXT("W A S D drive"), *SwitchHint),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), 20.0f, Canvas->ClipY - 40.0f, GEngine != nullptr ? GEngine->GetMediumFont() : nullptr);
	}
	if (bShowKeys && Vehicle != nullptr && Canvas != nullptr)
	{
		DrawKeys(Vehicle);
	}
	if (Vehicle == nullptr || !Vehicle->IsInCabin() || Canvas == nullptr || bShowKeys)
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
	if (Vehicle->IsCrossOnBoard())
	{
		// The plotter's pencil on the board: the number he would write, small beside the cross; the wheel turns it.
		const int32 Picked = Vehicle->GetPickedTarget();
		const FString Chip = Picked >= 0 ? (Picked >= 100 ? FString::Printf(TEXT("%d"), Picked) : FString::Printf(TEXT("IT-45 %02d"), Picked))
										 : FString(TEXT("—"));
		UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : Font;
		float W = 0.0f, H = 0.0f;
		GetTextSize(Chip, W, H, Small);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), Cx + 12.0f, Cy - 26.0f, W + 10.0f, H + 4.0f);
		DrawText(Chip, FLinearColor(1.0f, 0.82f, 0.2f), Cx + 17.0f, Cy - 24.0f, Small);
		DrawText(TEXT("wheel: number   click: write"), FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), Cx + 14.0f, Cy + 12.0f, Small);
	}
	const FString Feedback = Vehicle->GetCabinFeedbackText();
	if (!Feedback.IsEmpty())
	{
		DrawText(Feedback, FLinearColor(1.0f, 0.4f, 0.3f), Cx + 16.0f, Cy + 32.0f, Font);
	}
	const FString CommanderStation = Sim != nullptr ? Sim->GetCommanderStation() : FString(TEXT("commander"));
	if (Vehicle->IsWalking())
	{
		DrawText(FString::Printf(TEXT("On your feet (from the %s's seat)   |   W A S D walk   mouse looks   X sit down at a seat   Tab back to a seat   Caps Lock %s   G keys%s"),
					 *Vehicle->GetCabinStationLabel(), Vehicle->GetStationId() == CommanderStation ? TEXT("orders") : TEXT("reports"), *SwitchHint),
			Ink, 20.0f, Canvas->ClipY - 40.0f, Font);
	}
	else
	{
		DrawText(FString::Printf(TEXT("%s%s   |   F1/Tab next seat   X get up   C or F2 outside   T %s   Caps Lock %s   H help   B board   G keys%s"),
					 *Vehicle->GetCabinStationLabel(), Vehicle->IsHoldingSeat() ? TEXT(" (your seat)") : TEXT(""),
					 Vehicle->IsHoldingSeat() ? TEXT("give the seat back") : TEXT("take this seat"),
					 Vehicle->GetStationId() == CommanderStation ? TEXT("orders") : TEXT("reports"), *SwitchHint),
			Ink, 20.0f, Canvas->ClipY - 40.0f, Font);
	}
	DrawCrew(Vehicle, Font);
	if (Sim != nullptr && Vehicle->IsCommandPost())
	{
		DrawPost(Sim, Vehicle, Font);
	}
	if (Sim != nullptr && Vehicle->IsAtJournal())
	{
		DrawJournal(Sim, Vehicle);
	}
}

void ASimCabinHUD::DrawJournal(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle)
{
	// What he wrote is on the page and nowhere else (the first-person rule): the page as the cabin's paper has it, only
	// held up close and in good light.
	UFont* Large = GEngine != nullptr ? GEngine->GetLargeFont() : nullptr;
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor PaperInk(0.93f, 0.91f, 0.84f);    // the page
	const FLinearColor Rule(0.62f, 0.72f, 0.86f);        // its ruled lines
	const FLinearColor Margin(0.85f, 0.45f, 0.45f);      // the margin line
	const FLinearColor Printed(0.18f, 0.18f, 0.18f);     // the form's printed words
	const FLinearColor Pen(0.06f, 0.12f, 0.42f);         // his pen
	const FLinearColor Pencil(0.85f, 0.35f, 0.05f);      // the box he is on
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const float W = FMath::Min(Canvas->ClipX - 80.0f, 1100.0f);
	const float H = FMath::Min(Canvas->ClipY - 120.0f, 720.0f);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f - 20.0f;
	DrawRect(PaperInk, X, Y, W, H);
	const float Left = X + 0.09f * W;
	DrawLine(Left - 12.0f, Y, Left - 12.0f, Y + H, Margin, 1.5f);
	const float Scale = FMath::Clamp(W / 900.0f, 0.9f, 1.4f);
	DrawText(TEXT("ЖУРНАЛ РАДИОСВЯЗИ"), Printed, Left, Y + 18.0f, Large, Scale);
	DrawText(TEXT("Время     Сеть        От кого: содержание"), Printed, Left, Y + 58.0f * Scale, Small, Scale);
	const float Row = 34.0f * Scale;
	float Ry = Y + 90.0f * Scale;
	// The entries written, in his hand (the paper's own lines without its two printed headings, and without the line being
	// written, drawn below with its boxes).
	TArray<FString> Lines = Sim->GetDocumentLines(TEXT("radio_journal"));
	const bool bDraft = Sim->IsJournalDrafting();
	if (Lines.Num() >= 2)
	{
		Lines.RemoveAt(0, 2);
	}
	if (bDraft && Lines.Num() > 0)
	{
		Lines.Pop();
	}
	const float TextW = X + W - 24.0f - Left;
	for (const FString& L : Lines)
	{
		DrawLine(X + 8.0f, Ry + Row - 6.0f, X + W - 8.0f, Ry + Row - 6.0f, Rule, 1.0f);
		float Tw = 0.0f, Th = 0.0f;
		GetTextSize(L, Tw, Th, Medium, Scale);
		DrawText(L, Pen, Left, Ry + 4.0f, Medium, Tw > TextW ? Scale * TextW / Tw : Scale);
		Ry += Row;
	}
	// The line being written: the form's words printed, a box a group of digits, his digits in them.
	FString Lead;
	TArray<FString> Words, Typed;
	TArray<int32> Widths;
	int32 Box = 0;
	if (bDraft && Sim->GetJournalDraft(Lead, Words, Typed, Widths, Box))
	{
		DrawRect(FLinearColor(1.0f, 0.96f, 0.72f), X + 8.0f, Ry - 4.0f, W - 16.0f, Row + 14.0f);
		float Cx = Left;
		float Tw = 0.0f, Th = 0.0f;
		const float Ty = Ry + 6.0f;
		auto Print = [&](const FString& S, const FLinearColor& Ink)
		{
			GetTextSize(S, Tw, Th, Medium, Scale);
			DrawText(S, Ink, Cx, Ty, Medium, Scale);
			Cx += Tw;
		};
		Print(Lead + TEXT(" "), Printed);
		float Dw = 0.0f;
		GetTextSize(TEXT("0"), Dw, Th, Medium, Scale);
		const float Cell = Dw + 6.0f * Scale;
		for (int32 I = 0; I < Widths.Num(); ++I)
		{
			Print(Words.IsValidIndex(I) ? Words[I] : FString(), Printed);
			const bool bHere = I == Box;
			for (int32 K = 0; K < Widths[I]; ++K)
			{
				const float Bx = Cx + K * Cell;
				DrawRect(bHere ? Pencil : Printed, Bx, Ty + Th + 1.0f, Cell - 3.0f, bHere ? 2.0f : 1.0f);  // the box's line to write on
				if (K < Typed[I].Len())
				{
					DrawText(Typed[I].Mid(K, 1), Pen, Bx + 1.5f * Scale, Ty, Medium, Scale);
				}
			}
			if (bHere)
			{
				const float Px = Cx + FMath::Min(Typed[I].Len(), Widths[I] - 1) * Cell;
				DrawRect(FLinearColor(Pencil.R, Pencil.G, Pencil.B, 0.18f), Px - 1.0f, Ty - 2.0f, Cell - 1.0f, Th + 5.0f);
			}
			Cx += Widths[I] * Cell + 2.0f;
		}
		if (Words.Num() > Widths.Num())
		{
			Print(Words.Last(), Printed);
		}
	}
	// The keys, under the page.
	const FString Keys = Vehicle->IsWritingJournal()
		? (bDraft ? TEXT("Digits: write into the box   Backspace: rub out   Tab / Shift+Tab: next / last box   Enter: line done   S: short (number and time only)   Esc: put the page down")
				  : TEXT("Nothing more heard to write   Enter: the next message when one comes   Esc: put the page down"))
		: TEXT("Reading the radio journal   Esc: put the page down");
	float Kw = 0.0f, Kh = 0.0f;
	GetTextSize(Keys, Kw, Kh, Small);
	DrawText(Keys, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), (Canvas->ClipX - Kw) * 0.5f, Y + H + 12.0f, Small);
}

TArray<FString> ASimCabinHUD::Wrap(const FString& Text, float Width, UFont* Font, float Scale)
{
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "), true);
	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Try, W, H, Font, Scale);
		if (W > Width && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	return Lines;
}

void ASimCabinHUD::DrawView(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle)
{
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	const USimViewComponent* Views = Vehicle->GetViews();
	float Y = 12.0f;
	if (Views != nullptr && !Vehicle->IsInCabin())
	{
		const FString Label = Views->GetViewLabel();
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Label, W, H, Medium);
		DrawBacking(20.0f, Y, W, H);
		DrawText(Label, FLinearColor(0.95f, 0.95f, 0.85f), 20.0f, Y, Medium);
		Y += H + 6.0f;
	}
	const FString Note = Views != nullptr ? Views->GetViewMessage() : FString();
	if (!Note.IsEmpty())
	{
		DrawText(Note, FLinearColor(1.0f, 0.78f, 0.35f), 20.0f, Vehicle->IsInCabin() ? Canvas->ClipY * 0.5f + 52.0f : Y, Medium);
	}
	// Time compression: always on the screen while it is on.
	const float Scale = Sim->GetTimeScale();
	if (Scale != 1.0f)
	{
		const FString T = FString::Printf(TEXT("TIME x%g"), Scale);
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(T, W, H, Medium);
		DrawBacking((Canvas->ClipX - W) * 0.5f, 44.0f, W, H);
		DrawText(T, FLinearColor(1.0f, 0.78f, 0.35f), (Canvas->ClipX - W) * 0.5f, 44.0f, Medium);
	}
}

namespace
{
	// A wheel's value as its own scale reads it: the azimuth in thousandths ("52-82"), the range in km, else degrees.
	FString WheelText(const FString& Control, double Value)
	{
		if (Control.StartsWith(TEXT("dt2.range")))
		{
			return FString::Printf(TEXT("%.2f of the tube"), Value);  // the DT-2 marker's range: a fraction of the scale
		}
		if (Control.Contains(TEXT("range")))
		{
			return FString::Printf(TEXT("%.1f km"), Value / 1000.0);
		}
		if (Control.Contains(TEXT("az_wheel")))
		{
			double Deg = FMath::Fmod(Value, 360.0);
			if (Deg < 0.0)
			{
				Deg += 360.0;
			}
			const int32 Mils = FMath::RoundToInt32(Deg / 360.0 * 6000.0) % 6000;
			return FString::Printf(TEXT("%02d-%02d (%.0f°)"), Mils / 100, Mils % 100, Deg);
		}
		return FString::Printf(TEXT("%.1f°"), Value);
	}

	// A callout's words with its fields still to fill shown as "…" (the number of a target nobody designated).
	FString Words(const FString& Text)
	{
		FString Out;
		bool bInField = false;
		for (const TCHAR C : Text)
		{
			if (C == TEXT('{'))
			{
				bInField = true;
				Out += TEXT("…");
			}
			else if (C == TEXT('}'))
			{
				bInField = false;
			}
			else if (!bInField)
			{
				Out += C;
			}
		}
		return Out;
	}
}

namespace
{
	// The lesson's sizes grow with the window: laid out for 900 px high, up to twice that.
	float LessonScale(const UCanvas* Canvas)
	{
		return FMath::Clamp(Canvas->ClipY / 900.0f, 1.0f, 2.0f);
	}

	// A line of text at a scale (UE5's small, medium and large engine fonts are one size: the words are scaled, and the
	// font cache draws them sharp at that size).
	float LineHeight(const UFont* Font, float Scale)
	{
		return ((Font != nullptr ? Font->GetMaxCharHeight() : 14.0f) + 3.0f) * Scale;
	}
}

float ASimCabinHUD::DrawWheelGauge(const FSimTutorialStep& Step, float X, float Y, float Width, float S, float TextScale, bool bDraw)
{
	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	const FLinearColor Green(0.45f, 1.0f, 0.55f);
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const float Line = LineHeight(Font, TextScale);
	if (!Step.bTargetKnown)
	{
		if (bDraw)
		{
			DrawText(FString::Printf(TEXT("Wheel now: %s   the mark: not yet"), *WheelText(Step.Control, Step.Value)), Paper, X, Y, Font, TextScale);
		}
		return Line;
	}
	const double Err = Step.bWraps ? FMath::Fmod(Step.Value - Step.Target + 540.0, 360.0) - 180.0 : Step.Value - Step.Target;
	// A bar centred on the mark, its band the tolerance; the wheel's place on it.
	const float BarY = Y + Line + 6.0f * S;
	const float WayY = BarY + 18.0f * S;
	if (!bDraw)
	{
		return WayY + Line - Y;
	}
	DrawText(FString::Printf(TEXT("Wheel now: %s     the mark: %s"), *WheelText(Step.Control, Step.Value), *WheelText(Step.Control, Step.Target)),
		Paper, X, Y, Font, TextScale);
	const float Half = Width * 0.5f;
	const double Span = FMath::Max(Step.Tolerance * 6.0, FMath::Abs(Err) * 1.25);
	const float Cx = X + Half;
	DrawRect(FLinearColor(0.15f, 0.15f, 0.13f, 0.9f), X, BarY, Width, 8.0f * S);
	const float Band = static_cast<float>(Step.Tolerance / Span) * Half;
	DrawRect(FLinearColor(0.2f, 0.7f, 0.3f, 0.9f), Cx - Band, BarY, 2.0f * Band, 8.0f * S);
	DrawLine(Cx, BarY - 4.0f * S, Cx, BarY + 12.0f * S, Paper, 1.5f * S);
	const float At = Cx + static_cast<float>(FMath::Clamp(Err / Span, -1.0, 1.0)) * Half;
	DrawRect(Step.bOnMark ? Green : Amber, At - 3.0f * S, BarY - 5.0f * S, 6.0f * S, 18.0f * S);
	const FString Way = Step.bOnMark ? FString(TEXT("On the mark: hold it there"))
		: Err < 0.0 ? FString(TEXT("Turn it up: drag right, or scroll up")) : FString(TEXT("Turn it down: drag left, or scroll down"));
	const float Mark = Line * 0.7f;
	if (Step.bOnMark)
	{
		DrawTick(X, WayY + (Line - Mark) * 0.5f, Mark, Green);
	}
	else
	{
		// The arrow points the way the marker has to go on the bar.
		DrawArrowhead(FVector2f(X + (Err < 0.0 ? Mark : 0.0f), WayY + Line * 0.5f), FVector2f(Err < 0.0 ? 1.0f : -1.0f, 0.0f), Mark, Amber, 2.0f * S);
	}
	DrawText(Way, Step.bOnMark ? Green : Amber, X + Mark + 8.0f * S, WayY, Font, TextScale);
	return WayY + Line - Y;
}

void ASimCabinHUD::DrawTick(float X, float Y, float Size, const FLinearColor& Color)
{
	const float Thick = FMath::Max(2.0f, Size * 0.14f);
	DrawLine(X, Y + Size * 0.55f, X + Size * 0.38f, Y + Size * 0.9f, Color, Thick);
	DrawLine(X + Size * 0.38f, Y + Size * 0.9f, X + Size, Y + Size * 0.1f, Color, Thick);
}

void ASimCabinHUD::DrawArrowhead(FVector2f Tip, FVector2f Dir, float Size, const FLinearColor& Color, float Thickness)
{
	Dir.Normalize();
	const FVector2f Back = Tip - Dir * Size;
	const FVector2f Side(-Dir.Y * Size * 0.5f, Dir.X * Size * 0.5f);
	DrawLine(Tip.X, Tip.Y, Back.X + Side.X, Back.Y + Side.Y, Color, Thickness);
	DrawLine(Tip.X, Tip.Y, Back.X - Side.X, Back.Y - Side.Y, Color, Thickness);
	DrawLine(Back.X + Side.X, Back.Y + Side.Y, Back.X - Side.X, Back.Y - Side.Y, Color, Thickness);
}

void ASimCabinHUD::DrawLessonMarker(const FSimTutorialStep& Step, const ASimOsaVehicle* Vehicle)
{
	const bool bAction = Step.Phase == ESimTutorialPhase::Act || Step.Phase == ESimTutorialPhase::Wait;
	const FString Target = bAction && !Step.Control.IsEmpty() ? Step.Control : Step.Look;
	USimCabinComponent* Cabin = Vehicle->GetCabin();
	APlayerController* Pc = GetOwningPlayerController();
	if (Target.IsEmpty() || Cabin == nullptr || Pc == nullptr || !Vehicle->IsInCabin() || bShowKeys)
	{
		return;
	}
	FVector World;
	FVector Facing;
	FString Label;
	if (!Cabin->GetHighlightTarget(Target, World, Label, Facing))
	{
		return;
	}
	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const float S = LessonScale(Canvas);
	const float TextScale = 1.12f * S;
	const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.0f);
	const FLinearColor Green(0.35f, 1.0f, 0.5f, Pulse);
	FString Tag = Label;
	if (bAction && !Step.PositionLabel.IsEmpty())
	{
		Tag = FString::Printf(TEXT("%s  >  %s"), *Label, *Step.PositionLabel);
	}
	else if (bAction && Step.bWheel)
	{
		Tag = Label + (Step.bOnMark ? TEXT("  (on the mark)") : TEXT("  (turn it)"));
	}
	if (Step.Phase == ESimTutorialPhase::Wait)
	{
		Tag += TEXT("   (not yet)");
	}
	FVector CamLoc;
	FRotator CamRot;
	Pc->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Local = CamRot.UnrotateVector(World - CamLoc);  // X ahead, Y right, Z up
	const FVector Projected = Project(World);
	const FVector2f Screen(static_cast<float>(Projected.X), static_cast<float>(Projected.Y));
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Margin = 50.0f * S;
	const FLinearColor TagInk(0.55f, 1.0f, 0.65f);
	float Tw = 0.0f;
	float Th = 0.0f;
	if (Local.X > 1.0 && Screen.X > Margin && Screen.X < W - Margin && Screen.Y > Margin && Screen.Y < H - Margin)
	{
		// A bracket round it, the words beside, never over it (they would hide its own label).
		const float R = 26.0f * S;
		const float L = 10.0f * S;
		for (const FVector2f C : {FVector2f(-1.0f, -1.0f), FVector2f(1.0f, -1.0f), FVector2f(-1.0f, 1.0f), FVector2f(1.0f, 1.0f)})
		{
			const FVector2f P = Screen + C * R;
			DrawLine(P.X, P.Y, P.X - C.X * L, P.Y, Green, 2.0f * S);
			DrawLine(P.X, P.Y, P.X, P.Y - C.Y * L, Green, 2.0f * S);
		}
		GetTextSize(Tag, Tw, Th, Font, TextScale);
		const float Tx = Screen.X + R + 8.0f * S + Tw < W - 10.0f ? Screen.X + R + 8.0f * S : Screen.X - R - 8.0f * S - Tw;
		const float Ty = Screen.Y - R - Th * 0.5f;
		DrawBacking(Tx, Ty, Tw, Th);
		DrawText(Tag, TagInk, Tx, Ty, Font, TextScale);
		return;
	}
	// Out of view: an arrow at the edge, the way to turn the head.
	FVector2f Dir(static_cast<float>(Local.Y), static_cast<float>(-Local.Z));
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2f(1.0f, 0.0f);
	}
	Dir.Normalize();
	const FVector2f Centre(W * 0.5f, H * 0.5f);
	const float Sx = FMath::Abs(Dir.X) > 1e-4f ? (W * 0.5f - Margin) / FMath::Abs(Dir.X) : 1e6f;
	const float Sy = FMath::Abs(Dir.Y) > 1e-4f ? (H * 0.5f - Margin) / FMath::Abs(Dir.Y) : 1e6f;
	const FVector2f Tip = Centre + Dir * FMath::Min(Sx, Sy);
	DrawArrowhead(Tip, Dir, 26.0f * S, Green, 3.0f * S);
	const FString Words2 = FString::Printf(TEXT("%s   (L: look at it)"), *Tag);
	GetTextSize(Words2, Tw, Th, Font, TextScale);
	// The words inside the arrow, toward the centre.
	const FVector2f TextAt = Tip - Dir * (56.0f * S) - FVector2f(Tw * 0.5f * (1.0f + Dir.X), Th * 0.5f);
	const float Tx = FMath::Clamp(TextAt.X, 10.0f, W - Tw - 10.0f);
	const float Ty = FMath::Clamp(TextAt.Y, 10.0f, H - Th - 10.0f);
	DrawBacking(Tx, Ty, Tw, Th);
	DrawText(Words2, TagInk, Tx, Ty, Font, TextScale);
}

void ASimCabinHUD::DrawGuide(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle)
{
	if (!Sim->HasTutorial() || bShowKeys)
	{
		return;
	}
	const FSimTutorialStep Step = Sim->GetTutorialStep();
	if (Step.Count == 0)
	{
		return;
	}
	DrawLessonMarker(Step, Vehicle);
	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	const FLinearColor Dim(0.62f, 0.66f, 0.6f);
	const FLinearColor Green(0.45f, 1.0f, 0.55f);
	const FLinearColor Red(1.0f, 0.45f, 0.35f);
	const FLinearColor Blue(0.6f, 0.85f, 1.0f);
	// The card's words a little larger than the rest of the HUD's, the title larger still.
	const float S = LessonScale(Canvas);
	const float HeadScale = S;
	const float BodyScale = 1.12f * S;
	const float ActionScale = 1.25f * S;
	const float TitleScale = 1.5f * S;
	const float Width = FMath::Min(660.0f * S, Canvas->ClipX * 0.42f);
	const float Pad = 12.0f * S;
	const float Inner = Width - 2.0f * Pad;
	// The top right; the left, below the help, while the plotting board fills the right.
	const float X = bShowBoard ? 20.0f : Canvas->ClipX - Width - 20.0f;
	const float Top = bShowBoard ? Canvas->ClipY * 0.42f : 64.0f * S;
	const float Tx = X + Pad;

	// Laid out first (the card's height), then drawn.
	struct FLine
	{
		FString Text;
		FLinearColor Color;
		float Scale;
		float Y;
	};
	TArray<FLine> Lines;
	float Y = Top + Pad * 0.6f;
	auto Add = [&](const FString& Text, const FLinearColor& Color, float Scale, float GapAfter)
	{
		for (const FString& L : Wrap(Text, Inner, Font, Scale))
		{
			Lines.Add({L, Color, Scale, Y});
			Y += LineHeight(Font, Scale);
		}
		Y += GapAfter * S;
	};
	// Where: the lesson and its chapter (the step's number on the right), and a bar of the way through it.
	const float HeadY = Y;
	Y += LineHeight(Font, HeadScale) + 2.0f * S;
	const float BarY = Y;
	Y += 11.0f * S;
	// The step: its title, the action in one line (the control and the position to set, or the order or report), what
	// to do, and why.
	Add(Step.Title, Amber, TitleScale, 4.0f);
	const bool bAction = Step.Phase == ESimTutorialPhase::Act || Step.Phase == ESimTutorialPhase::Wait;
	if (bAction && !Step.Control.IsEmpty() && !Step.bWheel)
	{
		Add(FString::Printf(TEXT("%s   >   %s"), *Step.ControlLabel, *Step.PositionLabel), Green, ActionScale, 4.0f);
	}
	if (bAction && !Step.Order.IsEmpty())
	{
		Add(FString::Printf(TEXT("%s:  «%s»  (%s)"), Step.bReport ? TEXT("Your report") : TEXT("Your order"), *Words(Step.OrderRu), *Words(Step.OrderEn)),
			Green, ActionScale, 4.0f);
	}
	for (const FString& L : Step.Lines)
	{
		Add(L, Paper, BodyScale, 5.0f);
	}
	if (!Step.Why.IsEmpty())
	{
		Add(TEXT("Why: ") + Step.Why, Dim, BodyScale, 5.0f);
	}
	float GaugeY = -1.0f;
	if (bAction && Step.bWheel)
	{
		GaugeY = Y;
		Y += DrawWheelGauge(Step, Tx, Y, Inner, S, BodyScale, false) + 6.0f * S;
	}
	if (!Step.Feedback.IsEmpty())
	{
		Add(Step.Feedback, Red, BodyScale, 2.0f);
	}
	// Hints that grow while the learner hasn't found it: the keys, then the look key.
	FString Hint = Step.Hint;
	const bool bStuck = Step.Phase == ESimTutorialPhase::Act && Step.ShownSeconds > 12.0f;
	if (Step.Phase == ESimTutorialPhase::Act && Step.ShownSeconds > 25.0f && !Step.Control.IsEmpty())
	{
		Hint += TEXT("   Can't find it? Press L: your head turns onto it.");
	}
	const int32 HintFrom = Lines.Num();
	if (!Hint.IsEmpty())
	{
		Add(Hint, Blue, BodyScale, 4.0f);
	}
	const int32 HintTo = Lines.Num();
	// The status line: what happens next, always.
	FString Status;
	FLinearColor StatusColor = Amber;
	// A control or the crew menu needs the lesson's seat (driving or a view doesn't).
	const bool bAwayFromSeat = bAction && !Step.Seat.IsEmpty() && (!Step.Control.IsEmpty() || !Step.Order.IsEmpty()) &&
		(!Vehicle->IsInCabin() || Vehicle->GetStationId() != Step.Seat);
	switch (Step.Phase)
	{
	case ESimTutorialPhase::Read:
		Status = Step.bReview ? FString(TEXT("Gone back: what was done stays done.   Enter: on again")) : FString(TEXT("Enter: continue"));
		StatusColor = Green;
		break;
	case ESimTutorialPhase::Wait:
		Status = FString::Printf(TEXT("Waiting for %s%s"), *Step.WaitText, Sim->GetTimeScale() > 1.0f ? TEXT("   (time faster: the lesson speeds the wait)") : TEXT(""));
		break;
	case ESimTutorialPhase::Watch:
		Status = FString::Printf(TEXT("Watch: waiting for %s%s"), *Step.WaitText, Sim->GetTimeScale() > 1.0f ? TEXT("   (time faster)") : TEXT(""));
		break;
	case ESimTutorialPhase::Act:
		Status = Step.Order.IsEmpty() ? FString(TEXT("Your move.")) : FString(TEXT("Your move: Caps Lock opens the menu; the line is green (0 says it)."));
		StatusColor = Green;
		break;
	case ESimTutorialPhase::End:
		Status = TEXT("Lesson done.   Esc: the menu (R restarts, M the missions)");
		StatusColor = Green;
		break;
	default:
		break;
	}
	if (bAwayFromSeat)
	{
		Status = TEXT("Back to your seat first: L takes you there and looks at it (or F1 / Tab)");
		StatusColor = Red;
	}
	if (Step.bInstructor && !Step.bReview)
	{
		Status = Step.Phase == ESimTutorialPhase::Wait ? FString::Printf(TEXT("Skipped: the instructor does it once the wait is over (%s)"), *Step.WaitText)
													   : FString(TEXT("Skipped: the instructor is doing it. Watch the lit control."));
		StatusColor = Blue;
	}
	Y += 2.0f * S;
	if (!Status.IsEmpty())
	{
		Add(Status, StatusColor, BodyScale, 0.0f);
	}
	// The way out of a step, always on the card.
	Y += 4.0f * S;
	const FString Ways = FString::Printf(TEXT("%s%s"), Step.bCanGoBack ? TEXT("Backspace: back a step") : TEXT(""),
		Step.bLast ? TEXT("") : (Step.bCanGoBack ? TEXT("   ·   N: skip (the instructor does it)") : TEXT("N: skip (the instructor does it)")));
	if (!Ways.IsEmpty())
	{
		Add(Ways, Dim, HeadScale, 0.0f);
	}
	const float Height = Y - Top + Pad * 0.6f;
	GuideBottom = Top + Height;

	// The card, its top edge lit for a moment when a new step comes up.
	const float Flash = FMath::Clamp(1.0f - Step.ShownSeconds / 1.5f, 0.0f, 1.0f);
	DrawRect(FLinearColor(0.015f, 0.025f, 0.02f, 0.82f), X, Top, Width, Height);
	DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.4f + 0.6f * Flash), X, Top, Width, 3.0f * S);
	DrawText(FString::Printf(TEXT("%s%s%s"), *Step.Guide, Step.Chapter.IsEmpty() ? TEXT("") : TEXT("  ·  "), *Step.Chapter), Dim, Tx, HeadY, Font,
		HeadScale);
	const FString Of = FString::Printf(TEXT("%d / %d"), Step.Number, Step.Count);
	float Ow = 0.0f;
	float Oh = 0.0f;
	GetTextSize(Of, Ow, Oh, Font, HeadScale);
	DrawText(Of, Dim, X + Width - Ow - Pad, HeadY, Font, HeadScale);
	DrawRect(FLinearColor(0.2f, 0.22f, 0.2f, 1.0f), Tx, BarY, Inner, 3.0f * S);
	DrawRect(Amber, Tx, BarY, Inner * static_cast<float>(Step.Number) / static_cast<float>(FMath::Max(1, Step.Count)), 3.0f * S);
	const float HintPulse = bStuck ? 0.65f + 0.35f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.0f) : 1.0f;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		FLinearColor Color = Lines[I].Color;
		if (I >= HintFrom && I < HintTo)
		{
			Color.A = HintPulse;
		}
		DrawText(Lines[I].Text, Color, Tx, Lines[I].Y, Font, Lines[I].Scale);
	}
	if (GaugeY >= 0.0f)
	{
		DrawWheelGauge(Step, Tx, GaugeY, Inner, S, BodyScale, true);
	}
	// The step just done: a tick and its title, a moment, at the top of the screen.
	if (Step.LastDoneSeconds < 1.6f && !Step.LastDone.IsEmpty())
	{
		float Dw = 0.0f;
		float Dh = 0.0f;
		GetTextSize(Step.LastDone, Dw, Dh, Font, ActionScale);
		const float TickSize = Dh * 0.8f;
		const float Dx = (Canvas->ClipX - Dw + TickSize + 10.0f * S) * 0.5f;
		const float Dy = 76.0f * S;
		DrawRect(FLinearColor(0.05f, 0.25f, 0.08f, 0.85f), Dx - TickSize - 22.0f * S, Dy - 6.0f * S, Dw + TickSize + 34.0f * S, Dh + 12.0f * S);
		DrawTick(Dx - TickSize - 10.0f * S, Dy + (Dh - TickSize) * 0.5f, TickSize, Green);
		DrawText(Step.LastDone, Green, Dx, Dy, Font, ActionScale);
	}
}

void ASimCabinHUD::DrawKeys(const ASimOsaVehicle* Vehicle)
{
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	struct FGroup
	{
		const TCHAR* Title;
		TArray<const TCHAR*> Lines;
	};
	const TArray<FGroup> Left = {
		{TEXT("Camera views"),
			{TEXT("F1    the cabin; again: the next seat"), TEXT("F2    your vehicle; again: closer / further"),
				TEXT("F3    fly-by of what the camera follows"), TEXT("F4    chase; again: from ahead"),
				TEXT("F5    the nearest aircraft from your vehicle; again: the next"), TEXT("F6    your missile in flight; again: the next"),
				TEXT("F7    the battery's vehicles; again: the next"), TEXT("F8    the aircraft; again: the next"),
				TEXT("F10   the map from above (wheel: height, right drag: pan)"), TEXT("F11   free camera (W A S D Q E, Shift faster)"),
				TEXT("Right mouse + drag   turn the camera"), TEXT("Wheel   closer / further, the lens on F5")}},
		{TEXT("Time"), {TEXT("Ctrl+Z   faster (x2 .. x32)"), TEXT("Alt+Z    slower"), TEXT("Shift+Z  normal time")}},
		{TEXT("The game"), {TEXT("Esc   pause: resume, restart, the missions, quit; the debrief"), TEXT("G     this card"),
			TEXT("F9    the trainer's instructor panel (where the simulation runs)")}},
		{TEXT("Lessons"), {TEXT("Enter   continue (a reading step)"), TEXT("L       look at what the step highlights"),
			TEXT("N       skip a step: the instructor does it"), TEXT("Backspace   back a step, to read it again")}},
		{TEXT("The helper (the missions menu turns it on)"), {TEXT("H   off, hints, on, watch the crewman (kept for the next missions)"),
			TEXT("N   the helper makes its move for you (not in a lesson)")}},
	};
	const TArray<FGroup> Right = {
		{TEXT("Driving (outside)"), {TEXT("W / S   throttle, reverse"), TEXT("A / D   steer"), TEXT("Space   brake: a halt for the crew")}},
		{TEXT("In the cabin"),
			{TEXT("C   in and out of the cabin"), TEXT("Tab / F1   the next seat"), TEXT("Mouse   look round; the cross names the control"),
				TEXT("Left click   switch on, step forward, press"), TEXT("Right click   step back"),
				TEXT("Drag or wheel on a hand wheel   turn it (held: the head turns to its display)"),
				TEXT("X   get up and walk (W A S D); X again by a seat sits down")}},
		{TEXT("The crew"),
			{TEXT("T   take this seat, or give it back"),
				TEXT("Caps Lock   the crew menu: orders (commander) or reports; 1-8 or the mouse, 0 the green line"),
				TEXT("Caps Lock held   flick to a sector, on to a line, let go; Tab (commander) seat first"),
				TEXT("U / I / P   «Есть!» / «Выполнено!» / «Повторите!»"),
				TEXT("H   the helper: off, hints, on, watch the crewman"), TEXT("1-9   quick crew drills (outside the crew menu)"),
				TEXT("B   the PU-12 board (in a launcher: the designations, and what you have heard)"), TEXT("V   the other vehicle (battery link)")}},
		{TEXT("Headset and radios"),
			{TEXT("J   headset on / off: without it you hear only the cabin"), TEXT("K / Shift+K   turn your intercom box (ВС, РСТ-1..4)"),
				TEXT("Y   answer a ringing line or telephone (again: put it down)")}},
		{TEXT("PU-12M seats"), {TEXT("[ ] then 1-5   a board target to a launcher (commander)"),
		TEXT("Wheel over the board   the number to write (plotter)"), TEXT("Click on the board   write it there (plotter)")}},
	};
	const float W = FMath::Min(1180.0f, Canvas->ClipX - 40.0f);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y0 = FMath::Max(20.0f, Canvas->ClipY * 0.08f);
	DrawRect(FLinearColor(0.015f, 0.02f, 0.015f, 0.88f), X, Y0, W, FMath::Min(Canvas->ClipY - Y0 - 20.0f, 600.0f));
	DrawText(TEXT("Keys   (G closes)"), Amber, X + 16.0f, Y0 + 10.0f, Medium);
	auto Column = [&](const TArray<FGroup>& Groups, float Cx)
	{
		float Y = Y0 + 44.0f;
		for (const FGroup& G : Groups)
		{
			DrawText(G.Title, Amber, Cx, Y, Small);
			Y += 18.0f;
			for (const TCHAR* L : G.Lines)
			{
				DrawText(L, Paper, Cx + 8.0f, Y, Small);
				Y += 16.0f;
			}
			Y += 8.0f;
		}
	};
	Column(Left, X + 16.0f);
	Column(Right, X + W * 0.52f);
	if (Vehicle->GetViews() != nullptr)
	{
		DrawText(FString::Printf(TEXT("Now: %s"), *Vehicle->GetViews()->GetViewLabel()), FLinearColor(0.55f, 0.95f, 0.65f), X + W * 0.52f,
			Y0 + 10.0f, Small);
	}
}

// The trainer's instructor panel (roadmap 10.3): every target in the sky as it is, the ones he holds, his keys.
void ASimCabinHUD::DrawInstructor(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle)
{
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	const FLinearColor Dim(0.62f, 0.64f, 0.58f);
	const TArray<FSimInstructorTrack> Picture = Sim->GetInstructorPicture();
	const float W = FMath::Min(Canvas->ClipX - 40.0f, 760.0f);
	const float X = Canvas->ClipX - W - 20.0f;
	float Y = 20.0f;
	const float Rows = static_cast<float>(FMath::Max(1, Picture.Num()));
	DrawRect(FLinearColor(0.015f, 0.02f, 0.015f, 0.78f), X - 10.0f, Y - 8.0f, W + 20.0f, 96.0f + Rows * 20.0f);
	DrawText(TEXT("Instructor (9F632 trainer): two targets at a time"), Amber, X, Y, Medium);
	Y += 28.0f;
	if (Picture.Num() == 0)
	{
		DrawText(TEXT("No target in the sky"), Dim, X, Y, Small);
		Y += 20.0f;
	}
	for (const FSimInstructorTrack& T : Picture)
	{
		const bool bPicked = T.Id == Vehicle->GetInstructorPick();
		const FString Row = FString::Printf(TEXT("%s %-4s %5.1f km %03.0f   heading %03.0f   %5.0f m   %3.0f m/s%s%s   %s"),
			bPicked ? TEXT(">") : TEXT(" "), *T.Id, T.RangeKm, T.AzimuthDeg, T.HeadingDeg, T.AglM, T.SpeedMps,
			T.bHeld ? TEXT("   HELD") : TEXT(""), T.bHasJammer ? (T.bJamming ? TEXT("   jammer ON") : TEXT("   jammer off")) : TEXT(""), *T.Name);
		DrawText(Row, bPicked ? Amber : Paper, X, Y, Small);
		Y += 20.0f;
	}
	Y += 6.0f;
	DrawText(TEXT("Tab target   Left/Right turn 30   Up/Down height   -/= speed   E manoeuvre   J jammer   R let go   N new drone   F9 close"),
		Dim, X, Y, Small);
	Y += 20.0f;
	const FString Feedback = Vehicle->GetInstructorFeedback();
	if (!Feedback.IsEmpty())
	{
		DrawText(Feedback, Amber, X, Y, Small);
	}
}

void ASimCabinHUD::DrawMissionMenu(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle)
{
	UFont* Large = GEngine != nullptr ? GEngine->GetLargeFont() : nullptr;
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	const FLinearColor Dim(0.62f, 0.64f, 0.58f);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X = FMath::Max(24.0f, W * 0.06f);
	DrawRect(FLinearColor(0.015f, 0.02f, 0.015f, 0.82f), 0.0f, 0.0f, W, H);
	const bool bEnded = Sim->IsScenarioFinished();
	float Y = FMath::Max(24.0f, H * 0.06f);
	DrawText(bEnded ? TEXT("The scenario has ended") : TEXT("Paused"), Amber, X, Y, Large, 1.2f);
	Y += 44.0f;
	DrawText(Sim->GetScenarioName(), Paper, X, Y, Medium);
	Y += 34.0f;
	const bool bLeave = Vehicle->CanLeaveMission();
	const TCHAR* Lines[] = {TEXT("Enter  back to the mission"), TEXT("R      restart the mission"), TEXT("M      the missions"), TEXT("Q      quit"),
		TEXT("G      every key")};
	for (int32 I = 0; I < 5; ++I)
	{
		if (!bLeave && (I == 1 || I == 2))
		{
			continue;
		}
		DrawText(Lines[I], Paper, X, Y, Medium);
		Y += 26.0f;
	}
	// The helper (docs/crew/14_HELPER.md): H off, hints, on; kept for the next missions.
	DrawText(TEXT("H      the helper: ") + USimWorldSubsystem::HelperLevelLabel(Sim->GetAssistLevel()), Paper, X, Y, Medium);
	Y += 26.0f;
	if (!bLeave)
	{
		DrawText(TEXT("(a networked game runs on while this menu is open)"), Dim, X, Y, Small);
		Y += 22.0f;
	}
	if (!bEnded)
	{
		return;
	}
	// The debrief's summary: from its totals to the crew's grades (the event list above it is in the log).
	Y += 16.0f;
	DrawText(TEXT("Debrief"), Amber, X, Y, Medium);
	Y += 28.0f;
	TArray<FString> DebriefLines;
	Sim->GetDebriefText().ParseIntoArrayLines(DebriefLines, false);
	int32 From = FMath::Max(0, DebriefLines.Num() - 24);
	for (int32 I = 0; I < DebriefLines.Num(); ++I)
	{
		if (DebriefLines[I].StartsWith(TEXT("Procedures completed")))
		{
			From = I;
			break;
		}
	}
	for (int32 I = From; I < DebriefLines.Num() && Y < H - 30.0f; ++I)
	{
		DrawText(DebriefLines[I], Paper, X, Y, Small);
		Y += 17.0f;
	}
}

void ASimCabinHUD::DrawPost(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle, UFont* Font)
{
	// The PU-12M's aids at the battery commander's seat (the cabin's IT-45, glass and IT boards are the real picture): the
	// board's targets, his reading of it, and the launchers as the IT boards show them. The plotter has none: he picks and
	// writes on the board itself (the wheel and a click: ASimOsaVehicle::HandleBoard).
	const bool bCommander = Vehicle->GetStationId() == Sim->GetCommanderStation();
	if (!bCommander)
	{
		return;
	}
	auto Numbered = [Vehicle](FString Line, const TArray<int32>& Numbers, bool bTwoDigits)
	{
		for (int32 N : Numbers)
		{
			const FString Num = bTwoDigits ? FString::Printf(TEXT("%02d"), N) : FString::FromInt(N);
			// The picked one in brackets; the helper's in stars (the target it would give a launcher, or plot).
			Line += N == Vehicle->GetPickedTarget()        ? FString::Printf(TEXT("[%s] "), *Num)
					: N == Vehicle->GetHelpTargetNumber() ? FString::Printf(TEXT("*%s* "), *Num)
														  : Num + TEXT(" ");
		}
		return Numbers.Num() == 0 ? Line + TEXT("(nothing)  ") : Line;
	};
	const FString Line = Numbered(TEXT("Board: "), Sim->GetPostBoardNumbers(), false);
	// As the commander reads the board by eye: each track's range and, from two crosses a minute apart, when it is here.
	FString Read = TEXT("");
	if (bCommander)
	{
		const FString Command = Sim->GetPostCommandText();
		if (!Command.IsEmpty())
		{
			Read += FString::Printf(TEXT("Regiment: %s   |   "), *Command);
		}
		for (const FVector4f& R : Sim->GetPostBoardReadings())
		{
			Read += R.W >= 0.0f ? FString::Printf(TEXT("%04d %.0f km, here in %.1f min   "), FMath::RoundToInt(R.X), R.Y, R.W)
								: FString::Printf(TEXT("%04d %.0f km   "), FMath::RoundToInt(R.X), R.Y);
		}
	}
	const float X = 20.0f;
	float Y = PostAidsTop(Sim, Vehicle);
	const FLinearColor Ink(0.85f, 0.95f, 0.85f);
	DrawBacking(X, Y, Canvas->ClipX - 2.0f * X, Canvas->ClipY - 52.0f - Y);
	DrawText(Line, Ink, X, Y, Font);
	Y += 22.0f;
	if (!Read.IsEmpty())
	{
		DrawText(Read, FLinearColor(1.0f, 0.85f, 0.45f), X, Y, Font);
		Y += 22.0f;
	}
	if (bCommander)
	{
		FString Launchers = TEXT("");
		const TArray<FSimLauncherStatus> List = Sim->GetLauncherStatus();
		for (int32 I = 0; I < List.Num() && I < 5; ++I)
		{
			const FSimLauncherStatus& L = List[I];
			Launchers += FString::Printf(TEXT("%d: %s%s %s%s   "), I + 1, *L.Callsign, L.Callsign == Vehicle->GetHelpCallsign() ? TEXT("*") : TEXT(""), *L.State,
				L.Missiles >= 0 ? *FString::Printf(TEXT(" (%d)"), L.Missiles) : TEXT(""));
		}
		DrawText(Launchers + TEXT("|  [ ] pick a target, 1-5 give it to that launcher"), Ink, X, Y, Font);
		Y += 22.0f;
		DrawText(TEXT("Or on the instruments: the DT-2's wheels put the marker on a target (its number in the ДХ window), ЦУ then the ")
			TEXT("launcher's place (1-5) on the ДХ's keys; СБР clears"), Ink, X, Y, Font);
	}
}

void ASimCabinHUD::DrawCrew(const ASimOsaVehicle* Vehicle, UFont* Font)
{
	// The helper (docs/crew/14_HELPER.md): the move this seat's NPC would make now, shown as a lesson's step is (its control
	// pulses in the cabin, the crew menu's line is green) without the lesson's card: a line or two of what, and why.
	if (Vehicle->IsAssistOn())
	{
		const FSimSeatHelp Help = Vehicle->GetHelp();
		const FLinearColor Green(0.4f, 1.0f, 0.5f);
		const FLinearColor Soft(0.7f, 0.9f, 0.7f, 0.85f);
		float Y = 40.0f;
		DrawText(FString::Printf(TEXT("Helper: %s   (H the level, N let it do the move)"), *USimWorldSubsystem::HelperLevelLabel(Help.Level)), Soft,
			20.0f, Y, Font);
		Y += 20.0f;
		if (!Help.bShown)
		{
			const TCHAR* Why = !Vehicle->IsHoldingSeat() && Help.Level != TEXT("demonstrate") ? TEXT("(take this seat, T, to be helped in it)")
							   : Help.Level == TEXT("hint") && Help.Steps.Num() > 0  ? TEXT("(a hint comes if you hesitate)")
																					: TEXT("(nothing for this seat to do now)");
			DrawText(Why, Soft, 20.0f, Y, Font);
		}
		const USimWorldSubsystem* HelpSim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
		for (int32 I = 0; Help.bShown && I < Help.Steps.Num() && I < 3; ++I)
		{
			const FSimHelpStep& S = Help.Steps[I];
			FString Line = S.Text;
			if (S.Kind == TEXT("order") || S.Kind == TEXT("report"))
			{
				Line += TEXT("   (Caps Lock: the green line)");
			}
			else if (S.Kind == TEXT("designate"))
			{
				int32 Key = 0;
				const TArray<FSimLauncherStatus> List = HelpSim != nullptr ? HelpSim->GetLauncherStatus() : TArray<FSimLauncherStatus>();
				for (int32 L = 0; L < List.Num(); ++L)
				{
					Key = List[L].Callsign == S.Callsign ? L + 1 : Key;
				}
				Line += Key > 0 ? FString::Printf(TEXT("   ([ ] pick %02d, then %d)"), S.Number, Key) : FString::Printf(TEXT("   ([ ] pick %02d)"), S.Number);
			}
			else if (S.Kind == TEXT("plot"))
			{
				Line += TEXT("   (pick it with the wheel, click it onto the board)");
			}
			else if (S.Kind == TEXT("wheel"))
			{
				Line += S.bOnMark ? FString(TEXT("   (on it: keep it there)")) : FString(TEXT("   (hold the left button on the wheel and drag)"));
			}
			else if (S.Kind == TEXT("halt"))
			{
				Line += TEXT("   (brake)");
			}
			if (!S.Blocked.IsEmpty())
			{
				Line += TEXT("   - not yet: ") + S.Blocked;
			}
			else if (S.DueInSeconds > 2.0)
			{
				Line += FString::Printf(TEXT("   (in about %.0f s)"), S.DueInSeconds);
			}
			DrawText((I == 0 ? TEXT("> ") : TEXT("  ")) + Line, I == 0 ? Green : Soft, 20.0f, Y, Font);
			Y += 20.0f;
			if (I == 0 && !S.Rule.IsEmpty())
			{
				DrawText(TEXT("   because: ") + S.Rule, FLinearColor(0.6f, 0.8f, 0.6f, 0.75f), 20.0f, Y, Font);
				Y += 18.0f;
			}
		}
		// The last move off the drill, and what it cost (docs/crew/08 section 2: after a move off the plan).
		const FSimMoveVerdict V = Vehicle->GetLastVerdict();
		const USimWorldSubsystem* VSim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
		if (V.bValid && !V.bBest && VSim != nullptr && VSim->GetSimTimeSeconds() - V.TimeSeconds < 10.0)
		{
			Y += 4.0f;
			DrawText(FString::Printf(TEXT("Off the drill: that cost %.0f s. The best move was: %s"), V.CostSeconds,
						 V.BestDescription.IsEmpty() ? *V.BestControl : *V.BestDescription),
				FLinearColor(1.0f, 0.6f, 0.3f), 20.0f, Y, Font);
		}
	}
	if (Vehicle->IsCrewMenuOpen())
	{
		DrawCrewMenu(Vehicle, Font);
	}
}

// The radial crew menu (docs/crew/15_TALK.md section 5): eight places round the centre, the first at the top and on
// clockwise, each always in its place (a line that can't be said now greyed with what stops it, a sector with nothing of
// this seat's faint); the lesson's line or the helper's pick marked green, the place the mouse points at lit.
void ASimCabinHUD::DrawCrewMenu(const ASimOsaVehicle* Vehicle, UFont* Font)
{
	const TArray<FSimCrewMenuSlot> Ring = Vehicle->GetCrewMenuRing();
	const int32 Hover = Vehicle->GetCrewMenuHover();
	const float S = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.7f, 2.0f);
	const float R = 200.0f * S;
	const float Cx = Canvas->ClipX * 0.5f;
	const float Cy = FMath::Max(Canvas->ClipY * 0.5f, GuideBottom + R + 50.0f * S);  // below a lesson's card
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), Cx - R - 250.0f * S, Cy - R - 40.0f * S, 2.0f * R + 500.0f * S, 2.0f * R + 110.0f * S);
	// The centre: the ring's title, and the cursor from it.
	float Tw = 0.0f;
	float Th = 0.0f;
	const FString Title = Vehicle->GetCrewMenuTitle();
	GetTextSize(Title, Tw, Th, Font);
	DrawText(Title, FLinearColor(1.0f, 0.78f, 0.35f), Cx - 0.5f * Tw, Cy - Th - 4.0f, Font);
	const FVector2D Cursor = Vehicle->GetCrewMenuCursor() * S;
	DrawLine(Cx, Cy, Cx + Cursor.X, Cy + Cursor.Y, FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), 1.5f);
	DrawRect(FLinearColor::White, Cx + Cursor.X - 3.0f, Cy + Cursor.Y - 3.0f, 6.0f, 6.0f);
	if (Ring.IsValidIndex(Hover) && !Ring[Hover].Reason.IsEmpty())
	{
		const FString Why = TEXT("Not now: ") + Ring[Hover].Reason;
		GetTextSize(Why, Tw, Th, Font);
		DrawText(Why, FLinearColor(1.0f, 0.6f, 0.3f), Cx - 0.5f * Tw, Cy + 8.0f, Font);
	}
	for (int32 K = 0; K < Ring.Num() && K < 8; ++K)
	{
		const FSimCrewMenuSlot& Slot = Ring[K];
		const float A = FMath::DegreesToRadians(45.0f * static_cast<float>(K));
		const float Px = Cx + R * FMath::Sin(A);
		const float Py = Cy - R * FMath::Cos(A);
		const FString Top = FString::Printf(TEXT("%d  %s"), K + 1, *Words(Slot.Ru));
		const FString Under = Words(Slot.En);
		float W1 = 0.0f, H1 = 0.0f, W2 = 0.0f, H2 = 0.0f;
		GetTextSize(Top, W1, H1, Font);
		GetTextSize(Under, W2, H2, Font, 0.85f);
		const float W = FMath::Max(W1, W2) + 12.0f;
		const float H = H1 + H2 + 6.0f;
		// The box beside its point: to the right of it on the right half, to the left on the left, centred top and bottom.
		const float Side = FMath::Sin(A);
		const float Bx = Side > 0.3f ? Px - 10.0f : Side < -0.3f ? Px - W + 10.0f : Px - 0.5f * W;
		const float By = Py - 0.5f * H;
		const bool bGrey = !Slot.Reason.IsEmpty() || Slot.bEmpty;
		const FLinearColor Back = K == Hover ? FLinearColor(0.2f, 0.32f, 0.6f, 0.9f)
								  : Slot.bAdvised ? FLinearColor(0.1f, 0.45f, 0.15f, 0.75f)
												  : FLinearColor(0.05f, 0.05f, 0.05f, 0.8f);
		DrawRect(Back, Bx, By, W, H);
		if (Slot.bAdvised && K == Hover)
		{
			DrawRect(FLinearColor(0.6f, 1.0f, 0.65f), Bx, By + H - 2.0f, W, 2.0f);
		}
		const FLinearColor Ink = Slot.bEmpty ? FLinearColor(0.35f, 0.35f, 0.35f)
								 : bGrey ? FLinearColor(0.55f, 0.55f, 0.55f)
								 : Slot.bAdvised ? FLinearColor(0.6f, 1.0f, 0.65f)
												 : FLinearColor(1.0f, 0.95f, 0.8f);
		DrawText(Top, Ink, Bx + 6.0f, By + 2.0f, Font);
		DrawText(Under, Ink * 0.85f, Bx + 6.0f, By + 4.0f + H1, Font, 0.85f);
	}
	// The keys, under the ring.
	const bool bLines = Vehicle->GetCrewMenuLevel() == 1;
	FString Keys = bLines ? TEXT("1-8 or point and click: say it   0: the green line   Backspace: back")
						  : TEXT("1-8 or flick out: open   0: the green line   Esc or Caps Lock: close");
	if (bLines && Vehicle->GetCrewMenuPages() > 1)
	{
		Keys += FString::Printf(TEXT("   9 or wheel: more (%d/%d)"), Vehicle->GetCrewMenuPage() + 1, Vehicle->GetCrewMenuPages());
	}
	const USimWorldSubsystem* MenuSim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (!bLines && MenuSim != nullptr && Vehicle->GetStationId() == MenuSim->GetCommanderStation())
	{
		Keys += Vehicle->IsCrewMenuSeatFirst() ? TEXT("   Tab: by subject") : TEXT("   Tab: seat first");
	}
	GetTextSize(Keys, Tw, Th, Font);
	DrawText(Keys, FLinearColor(0.8f, 0.8f, 0.8f), Cx - 0.5f * Tw, Cy + R + 40.0f * S, Font);
}

float ASimCabinHUD::PostAidsTop(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle) const
{
	if (Vehicle != nullptr && Vehicle->IsInCabin() && Vehicle->IsCommandPost())
	{
		if (Vehicle->GetStationId() == Sim->GetCommanderStation())
		{
			return Canvas->ClipY - 140.0f;  // with the board's reading line
		}

	}
	return Canvas->ClipY - 54.0f;
}

void ASimCabinHUD::DrawBacking(float X, float Y, float W, float H)
{
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f), X - 6.0f, Y - 3.0f, W + 12.0f, H + 6.0f);
}

float ASimCabinHUD::DrawComms(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle, UFont* Font, float Bottom)
{
	// What is in front of the man in the seat: his headset, his box and what it is on, the РСТ lamps of the sets it reaches,
	// a call or a ringing line. Nothing about nets he is not on (first person, docs/command/).
	if (Vehicle == nullptr || !Vehicle->IsInCabin() || Vehicle->GetStationId().IsEmpty())
	{
		return 0.0f;
	}
	const FSimSeatComms C = Sim->GetSeatComms(Vehicle->GetStationId());
	if (!C.bVoice)
	{
		return 0.0f;
	}
	TArray<TPair<FString, FLinearColor>> Lines;
	const FLinearColor Plain(0.85f, 0.85f, 0.8f, 0.9f);
	const FLinearColor Warn(1.0f, 0.45f, 0.3f, 1.0f);
	if (C.bUnboxed)
	{
		Lines.Add({TEXT("No intercom box at this seat: you hear the cabin (the radiotelephonist reads the air picture to you)"), Plain});
	}
	else if (!C.bHeadset)
	{
		Lines.Add({TEXT("Headset off: you hear only the cabin   (J: put it on)"), Warn});
	}
	else
	{
		FString Lamps;
		for (int32 I = 0; I < C.SetNames.Num(); ++I)
		{
			Lamps += FString::Printf(TEXT("   %s %s"), C.SetLive[I] ? TEXT("●") : TEXT("○"), *C.SetNames[I]);
		}
		FString Box = FString::Printf(TEXT("%s: %s"), *C.BoxName, C.bPowered ? *C.Channel : TEXT("no power"));
		if (!C.Monitor.IsEmpty())
		{
			Box += FString::Printf(TEXT(" + %s"), *C.Monitor);
		}
		Lines.Add({Box + Lamps + TEXT("   (K: turn the box)"), Plain});
	}
	if (C.bCalled)
	{
		Lines.Add({TEXT("ВЫЗОВ: another seat calls your box"), Warn});
	}
	for (int32 L : C.RingingLines)
	{
		Lines.Add({FString::Printf(TEXT("ЛИНИЯ %d rings   (Y: answer)"), L), Warn});
	}
	for (int32 L : C.LinesUp)
	{
		Lines.Add({FString::Printf(TEXT("On line %d   (Y: put it down)"), L), Plain});
	}
	if (C.bPhoneRinging)
	{
		Lines.Add({TEXT("ТА-57 rings   (Y: take the handset)"), Warn});
	}
	else if (C.bPhoneTaken)
	{
		Lines.Add({TEXT("ТА-57 handset taken   (Y: put it back)"), Plain});
	}
	float Widest = 0.0f;
	for (const auto& L : Lines)
	{
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(L.Key, W, H, Font);
		Widest = FMath::Max(Widest, W);
	}
	const float Height = 16.0f * Lines.Num();
	float Y = Bottom - Height;
	DrawBacking(20.0f, Y, Widest, Height);
	for (const auto& L : Lines)
	{
		DrawText(L.Key, L.Value, 20.0f, Y, Font);
		Y += 16.0f;
	}
	return Height + 8.0f;
}

void ASimCabinHUD::DrawRadio(USimWorldSubsystem* Sim, UFont* Font, float Bottom, const FString& Station, bool bGloss)
{
	// The lines still fresh, packed at the bottom: the oldest fades. Only what reached the player's seat: his headset and
	// box, or the cabin (docs/crew/12_COMMUNICATIONS.md). What came over a radio stands out from the rest of the screen: a
	// colour per net, a bar down its left edge and the net's name in capitals; a line on a telephone is LINE; orders and
	// reports said in the cabin stay plain.
	const double Now = Sim->GetSimTimeSeconds();
	const TArray<FSimRadioMessage> Messages = Sim->GetRadioMessages(RadioLines);
	struct FLine
	{
		FString Text;
		FLinearColor Ink;
		bool bRadio = false;
	};
	TArray<FLine> Lines;
	float Widest = 0.0f;
	for (const FSimRadioMessage& M : Messages)
	{
		if (!M.HeardAt(Station))
		{
			continue;  // not on his headset, not in his cabin
		}
		const double Age = Now - M.TimeSeconds;
		const bool bReadOut = M.Text.Contains(TEXT("(read out)"));
		const float Life = M.Net == TEXT("air picture") || bReadOut ? AirPictureLineSeconds : RadioLineSeconds;
		if (Age > Life && Now > 0.0)
		{
			continue;
		}
		// Flags and lamps on the march are seen, not heard (voenizdat_osa_1991: the commander signals from the column's head).
		const bool bSignal = M.Net == TEXT("flag signals") || M.Net == TEXT("lamp signals");
		const bool bRadio = !M.Net.StartsWith(TEXT("cabin")) && M.Net != TEXT("intercom") && !bSignal;
		const bool bLine = M.Net.StartsWith(TEXT("line "));
		FLinearColor Ink = bSignal ? FLinearColor(1.0f, 0.55f, 0.45f)  // red-orange: a signal seen from the column's head
						   : M.Net == TEXT("air picture") || bReadOut ? FLinearColor(1.0f, 0.72f, 0.25f)	 // amber: the air picture
						   : M.Net == TEXT("command net")  ? FLinearColor(0.35f, 0.85f, 1.0f)  // cyan: designations
						   : M.Net == TEXT("regiment net") ? FLinearColor(0.78f, 0.55f, 1.0f)  // violet: the regiment (the R-111)
						   : M.Net == TEXT("covered net")  ? FLinearColor(0.55f, 0.9f, 0.55f)  // green: the covered unit (FuG-2)
						   : bLine						   ? FLinearColor(0.95f, 0.9f, 0.6f)   // pale yellow: a telephone line
						   : bRadio						   ? FLinearColor(0.6f, 0.7f, 1.0f)	   // blue: the battery net
														   : FLinearColor(0.85f, 0.85f, 0.8f);
		Ink.A = FMath::Clamp(1.0f - static_cast<float>(Age) / Life, M.Net == TEXT("air picture") || bReadOut ? 0.0f : 0.35f, 1.0f);
		const FString Text = bRadio ? FString::Printf(TEXT("RADIO %s  %s: %s"), *M.Net.ToUpper(), *M.From, *M.Text)
									: FString::Printf(TEXT("[%s] %s: %s"), *M.Net, *M.From, *M.Text);
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Text, W, H, Font);
		Widest = FMath::Max(Widest, W + 10.0f);
		Lines.Add({Text, Ink, bRadio});
		// With the helper on: what its abbreviations and azimuths mean, under it (docs/crew/15_TALK.md section 6).
		const FString Gloss = bGloss ? Sim->GlossLine(M.Text) : FString();
		if (!Gloss.IsEmpty())
		{
			const FString Under = TEXT("      = ") + Gloss;
			GetTextSize(Under, W, H, Font);
			Widest = FMath::Max(Widest, W + 10.0f);
			FLinearColor Faint = Ink * 0.8f;
			Faint.A = Ink.A * 0.8f;
			Lines.Add({Under, Faint, false});
		}
	}
	if (Lines.Num() == 0)
	{
		return;
	}
	float Y = Bottom - 16.0f * Lines.Num();
	DrawBacking(20.0f, Y, Widest, 16.0f * Lines.Num());
	for (const FLine& L : Lines)
	{
		if (L.bRadio)
		{
			DrawRect(L.Ink, 20.0f, Y + 1.0f, 3.0f, 14.0f);
		}
		DrawText(L.Text, L.Ink, L.bRadio ? 30.0f : 20.0f, Y, Font);
		Y += 16.0f;
	}
}

void ASimCabinHUD::DrawBoard(USimWorldSubsystem* Sim, UFont* Font, const FString& Station)
{
	// In the command post its glass plotting board (samsim_osa p20): range rings at 50-200 km, the battery at the centre,
	// north up, each target's marks with its number, height over code, and the minute stamp; older plots dimmer. In a
	// launcher the board is not in front of the crew (it is in the post): the same page holds only the designations the
	// crew has heard on the net, each target's latest (a training aid; first person).
	const bool bPost = Sim->IsCommandPost();
	// What I've heard (docs/crew/15_TALK.md section 6): in a launcher, top left, the targets as this seat's crewman knows
	// them, from his instruments and from what reached his ears, never the truth.
	if (!bPost && !Station.IsEmpty())
	{
		const TArray<FSimHeardTarget> Heard = Sim->GetSeatPicture(Station);
		const float Hx = 20.0f;
		float Hy = 40.0f;
		const int32 Rows = FMath::Min(Heard.Num(), 12);
		DrawBacking(Hx, Hy, 470.0f, 18.0f + 16.0f * FMath::Max(1, Rows));
		DrawText(TEXT("What you have heard (the last 5 minutes)"), FLinearColor(1.0f, 0.78f, 0.35f), Hx, Hy, Font);
		Hy += 18.0f;
		if (Heard.Num() == 0)
		{
			DrawText(TEXT("nothing yet"), FLinearColor(0.7f, 0.7f, 0.7f), Hx, Hy, Font);
		}
		for (int32 I = 0; I < Rows; ++I)
		{
			const FSimHeardTarget& H = Heard[I];
			FString Row = H.Number > 0 ? FString::Printf(TEXT("target %d"), H.Number) : FString(TEXT("a contact"));
			if (!H.Azimuth.IsEmpty())
			{
				Row += FString::Printf(TEXT(", azimuth %s%s"), *H.Azimuth, H.bTrueAzimuth ? TEXT(" (true)") : TEXT(""));
			}
			if (H.RangeKm >= 0.0)
			{
				Row += FString::Printf(TEXT(", %.0f km"), H.RangeKm);
			}
			if (H.HeightM >= 0.0)
			{
				Row += FString::Printf(TEXT(", height %.0f m"), H.HeightM);
			}
			Row += H.bFriendly ? TEXT(", a friend") : H.bNoReply ? TEXT(", no IFF reply") : TEXT("");
			Row += FString::Printf(TEXT("   %.0f s ago, %s"), H.AgeSeconds, *H.Source);
			const FLinearColor Ink = H.bFriendly ? FLinearColor(0.55f, 0.95f, 0.6f) : FLinearColor(0.9f, 0.9f, 0.85f, FMath::Clamp(1.0f - static_cast<float>(H.AgeSeconds) / 400.0f, 0.4f, 1.0f));
			DrawText(Row, Ink, Hx, Hy, Font);
			Hy += 16.0f;
		}
	}
	const float Size = Canvas->ClipY * 0.7f;
	const FVector2D Origin(Canvas->ClipX - Size - 30.0f, 40.0f);
	const FVector2D Centre = Origin + FVector2D(Size, Size) * 0.5f;
	DrawRect(FLinearColor(0.02f, 0.04f, 0.05f, 0.85f), Origin.X, Origin.Y, Size, Size);
	// The battery's other launchers under the board: missiles, target, what they are doing (battery_fire.hpp).
	{
		float Y = Origin.Y + Size + 8.0f;
		for (const FSimBatteryShooter& S : Sim->GetBatteryShooters())
		{
			DrawText(FString::Printf(TEXT("%s  %d missiles  %s%s"), *S.Callsign, S.Missiles,
						 S.Target.IsEmpty() ? TEXT("no target") : *FString::Printf(TEXT("%s, %s"), *S.Target, *S.Phase),
						 S.bDeployed ? TEXT("") : TEXT("  (not deployed)")),
				FLinearColor(0.8f, 0.9f, 1.0f), Origin.X, Y, Font);
			Y += 16.0f;
		}
	}
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
	DrawText(bPost ? FString::Printf(TEXT("PU-12 plotting board   plot %d   (B to close)"), Newest)
				   : FString::Printf(TEXT("Designations heard from the PU-12 (%d)   (B to close)"), Last.Num()),
		Grid, Origin.X + 8.f, Origin.Y + Size - 18.f, Font);
}

