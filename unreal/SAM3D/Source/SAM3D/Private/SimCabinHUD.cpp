#include "SimCabinHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "SimOsaVehicle.h"
#include "SimViewComponent.h"
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
	const APlayerController* Pc = OwnerPc;
	const ASimOsaVehicle* Vehicle = Pc != nullptr ? Cast<ASimOsaVehicle>(Pc->GetPawn()) : nullptr;
	// G: every key; N: the guide's next step.
	if (OwnerPc != nullptr && Vehicle != nullptr && OwnerPc->WasInputKeyJustPressed(EKeys::G))
	{
		bShowKeys = !bShowKeys;
		if (bShowKeys && Sim != nullptr)
		{
			Sim->NoteTutorialEvent(TEXT("keys_card"));
		}
	}
	if (OwnerPc != nullptr && Sim != nullptr && Sim->HasTutorial() && Vehicle != nullptr && !Vehicle->IsMissionMenuOpen() &&
		OwnerPc->WasInputKeyJustPressed(EKeys::N))
	{
		Sim->SkipTutorialStep();
	}
	if (Vehicle != nullptr && Sim != nullptr && Canvas != nullptr && Sim->IsScenarioRunning() && !Vehicle->IsMissionMenuOpen())
	{
		DrawView(Sim, Vehicle);
		DrawGuide(Sim);
	}
	if (Sim != nullptr && Canvas != nullptr && (Vehicle == nullptr || !Vehicle->IsMissionMenuOpen()))
	{
		DrawRadio(Sim, SmallFont, PostAidsTop(Sim, Vehicle) - 8.0f);
		if (bShowBoard)
		{
			DrawBoard(Sim, SmallFont);
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
	const FString Feedback = Vehicle->GetCabinFeedbackText();
	if (!Feedback.IsEmpty())
	{
		DrawText(Feedback, FLinearColor(1.0f, 0.4f, 0.3f), Cx + 16.0f, Cy + 32.0f, Font);
	}
	const FString CommanderStation = Sim != nullptr ? Sim->GetCommanderStation() : FString(TEXT("commander"));
	DrawText(FString::Printf(TEXT("%s%s   |   F1/Tab next seat   C or F2 outside   T %s   O %s   H help   B board   G keys%s"),
				 *Vehicle->GetCabinStationLabel(), Vehicle->IsHoldingSeat() ? TEXT(" (your seat)") : TEXT(""),
				 Vehicle->IsHoldingSeat() ? TEXT("give the seat back") : TEXT("take this seat"),
				 Vehicle->GetStationId() == CommanderStation ? TEXT("orders") : TEXT("reports"), *SwitchHint),
		Ink, 20.0f, Canvas->ClipY - 40.0f, Font);
	DrawCrew(Vehicle, Font);
	if (Sim != nullptr && Vehicle->IsCommandPost())
	{
		DrawPost(Sim, Vehicle, Font);
	}
}

TArray<FString> ASimCabinHUD::Wrap(const FString& Text, float Width, UFont* Font)
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
		GetTextSize(Try, W, H, Font);
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

void ASimCabinHUD::DrawGuide(USimWorldSubsystem* Sim)
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
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	const float Width = FMath::Min(560.0f, Canvas->ClipX * 0.36f);
	// The top right; the left, under the help, while the plotting board fills the right.
	const float X = bShowBoard ? 20.0f : Canvas->ClipX - Width - 24.0f;
	const float Top = bShowBoard ? Canvas->ClipY * 0.42f : 70.0f;
	TArray<TPair<FString, FLinearColor>> Rows;
	const FLinearColor Amber(1.0f, 0.78f, 0.35f);
	const FLinearColor Paper(0.93f, 0.92f, 0.86f);
	const FLinearColor Dim(0.65f, 0.7f, 0.62f);
	for (const FString& L : Step.Lines)
	{
		for (const FString& W : Wrap(L, Width - 16.0f, Small))
		{
			Rows.Emplace(W, Paper);
		}
		Rows.Emplace(FString(), Paper);
	}
	if (!Step.Hint.IsEmpty())
	{
		for (const FString& W : Wrap(Step.Hint, Width - 16.0f, Small))
		{
			Rows.Emplace(W, FLinearColor(0.55f, 0.95f, 0.65f));
		}
	}
	const float Height = 50.0f + 16.0f * Rows.Num() + 22.0f;
	// A new step flashes its frame for a moment.
	const float Flash = FMath::Clamp(1.0f - Step.ShownSeconds / 2.0f, 0.0f, 1.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.02f, 0.72f), X, Top, Width, Height);
	DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.35f + 0.65f * Flash), X, Top, Width, 3.0f);
	DrawText(FString::Printf(TEXT("%s   %d / %d"), *Step.Guide, Step.Number, Step.Count), Dim, X + 8.0f, Top + 8.0f, Small);
	DrawText(Step.Title, Amber, X + 8.0f, Top + 24.0f, Medium);
	float Y = Top + 50.0f;
	for (const TPair<FString, FLinearColor>& R : Rows)
	{
		DrawText(R.Key, R.Value, X + 8.0f, Y, Small);
		Y += 16.0f;
	}
	DrawText(Step.bLast ? TEXT("G every key   Esc the menu") : TEXT("N skip this step   G every key"), Dim, X + 8.0f, Y + 4.0f, Small);
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
		{TEXT("The game"), {TEXT("Esc   pause: resume, restart, the missions, quit; the debrief"), TEXT("G     this card"), TEXT("N     the guide's next step (tutorial)")}},
	};
	const TArray<FGroup> Right = {
		{TEXT("Driving (outside)"), {TEXT("W / S   throttle, reverse"), TEXT("A / D   steer"), TEXT("Space   brake: a halt for the crew")}},
		{TEXT("In the cabin"),
			{TEXT("C   in and out of the cabin"), TEXT("Tab / F1   the next seat"), TEXT("Mouse   look round; the cross names the control"),
				TEXT("Left click   switch on, step forward, press"), TEXT("Right click   step back"),
				TEXT("Drag or wheel on a hand wheel   turn it")}},
		{TEXT("The crew"),
			{TEXT("T   take this seat, or give it back"), TEXT("O   orders (commander's seat) or reports; 1-9 pick, 0 next page"),
				TEXT("H   training overlay: guided, hint, demonstrate, off"), TEXT("1-9   quick crew drills (outside the crew menu)"),
				TEXT("B   the PU-12 plotting board"), TEXT("V   the other vehicle (battery link)")}},
		{TEXT("PU-12M seats"), {TEXT("[ ]   pick a target"), TEXT("1-5   give it to a launcher (commander)"), TEXT("P   plot it (plotter)")}},
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
	// The PU-12M's aids (the cabin's IT-45, glass and IT boards are the real picture): what the seat can pick and the
	// keys. At the commander's seat the board's targets and the launchers as the IT boards show them; at the plotter's
	// the IT-45's numbers.
	const bool bCommander = Vehicle->GetStationId() == Sim->GetCommanderStation();
	const bool bPlotter = Vehicle->GetStationId() == Sim->GetPlotterStation();
	if (!bCommander && !bPlotter)
	{
		return;
	}
	const TArray<int32> Numbers = bCommander ? Sim->GetPostBoardNumbers() : Sim->GetPostTrackNumbers();
	FString Line = bCommander ? TEXT("Board: ") : TEXT("IT-45: ");
	for (int32 N : Numbers)
	{
		const FString Num = bCommander ? FString::FromInt(N) : FString::Printf(TEXT("%02d"), N);
		Line += N == Vehicle->GetPickedTarget() ? FString::Printf(TEXT("[%s] "), *Num) : Num + TEXT(" ");
	}
	if (Numbers.Num() == 0)
	{
		Line += TEXT("(nothing)");
	}
	const float X = 20.0f;
	float Y = PostAidsTop(Sim, Vehicle);
	const FLinearColor Ink(0.85f, 0.95f, 0.85f);
	DrawBacking(X, Y, Canvas->ClipX - 2.0f * X, Canvas->ClipY - 52.0f - Y);
	DrawText(Line, Ink, X, Y, Font);
	Y += 22.0f;
	if (bCommander)
	{
		FString Launchers = TEXT("");
		const TArray<FSimLauncherStatus> List = Sim->GetLauncherStatus();
		for (int32 I = 0; I < List.Num() && I < 5; ++I)
		{
			const FSimLauncherStatus& L = List[I];
			Launchers += FString::Printf(TEXT("%d: %s %s%s   "), I + 1, *L.Callsign, *L.State,
				L.Missiles >= 0 ? *FString::Printf(TEXT(" (%d)"), L.Missiles) : TEXT(""));
		}
		DrawText(Launchers + TEXT("|  [ ] pick a target, 1-5 give it to that launcher"), Ink, X, Y, Font);
		Y += 22.0f;
		DrawText(TEXT("Or on the instruments: the DT-2's wheels put the marker on a target (its number in the ДХ window), ЦУ then the ")
			TEXT("launcher's place (1-5) on the ДХ's keys; СБР clears"), Ink, X, Y, Font);
	}
	else
	{
		DrawText(Vehicle->IsHoldingSeat() ? TEXT("[ ] pick a target, P write it on the board")
										  : TEXT("[ ] pick a target, P write it on the board (take the seat first: T)"),
			Ink, X, Y, Font);
	}
}

void ASimCabinHUD::DrawCrew(const ASimOsaVehicle* Vehicle, UFont* Font)
{
	// The training overlay: the drill master's next move for this seat, why, and when.
	if (Vehicle->IsAssistOn())
	{
		const FSimDrillAdvice A = Vehicle->GetAdvice();
		const FLinearColor Green(0.4f, 1.0f, 0.5f);
		float Y = 40.0f;
		DrawText(FString::Printf(TEXT("Help, %s (H: next level): %s"), *Vehicle->GetAssistLevel(), *Vehicle->GetAssistTask()), Green, 20.0f, Y,
			Font);
		Y += 20.0f;
		if (!A.bValid)
		{
			DrawText(Vehicle->GetAssistLevel() == TEXT("hint") ? TEXT("(a hint comes if you hesitate)") : TEXT("nothing for this seat now"), Green,
				20.0f, Y, Font);
		}
		else
		{
			DrawText(FString::Printf(TEXT("%s: %s%s"), A.bDemonstrating ? TEXT("Watch the crewman") : TEXT("Next"), *A.Description,
						 A.DueInSeconds > 2.0 ? *FString::Printf(TEXT("  (in about %.0f s)"), A.DueInSeconds) : TEXT("  (now)")),
				Green, 20.0f, Y, Font);
			Y += 20.0f;
			for (int32 I = 0; I < A.Why.Num() && I < 3; ++I)
			{
				DrawText(FString::Printf(TEXT("   because: %s"), *A.Why[I]), FLinearColor(0.7f, 0.9f, 0.7f), 20.0f, Y, Font);
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
	// The crew menu: orders at the commander's seat, this seat's reports elsewhere.
	if (Vehicle->IsCrewMenuOpen())
	{
		const TArray<FSimCrewMenuEntry> Entries = Vehicle->GetCrewMenuEntries();
		const int32 Page = Vehicle->GetCrewMenuPage();
		const int32 Pages = FMath::Max(1, (Entries.Num() + 8) / 9);
		const float X = Canvas->ClipX * 0.5f - 260.0f;
		float Y = Canvas->ClipY * 0.25f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), X - 10.0f, Y - 10.0f, 860.0f, 230.0f);
		const USimWorldSubsystem* MenuSim = GetWorld() ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
		const bool bOrders = MenuSim != nullptr && Vehicle->GetStationId() == MenuSim->GetCommanderStation();
		DrawText(FString::Printf(TEXT("%s (page %d/%d: 0 next, Esc close)"), bOrders ? TEXT("Orders") : TEXT("Reports"),
					 Page + 1, Pages),
			FLinearColor::White, X, Y, Font);
		Y += 24.0f;
		for (int32 I = 0; I < 9; ++I)
		{
			const int32 Index = Page * 9 + I;
			if (!Entries.IsValidIndex(Index))
			{
				break;
			}
			const FSimCrewMenuEntry& E = Entries[Index];
			if (E.bAvailable)
			{
				DrawText(FString::Printf(TEXT("%d  %s  (%s)"), I + 1, *E.Ru, *E.En), FLinearColor(1.0f, 0.95f, 0.8f), X, Y, Font);
			}
			else  // honest greying: what stops it
			{
				DrawText(FString::Printf(TEXT("%d  %s  (%s): %s"), I + 1, *E.Ru, *E.En, *E.Reason), FLinearColor(0.5f, 0.5f, 0.5f), X, Y, Font);
			}
			Y += 20.0f;
		}
	}
}

float ASimCabinHUD::PostAidsTop(USimWorldSubsystem* Sim, const ASimOsaVehicle* Vehicle) const
{
	if (Vehicle != nullptr && Vehicle->IsInCabin() && Vehicle->IsCommandPost())
	{
		if (Vehicle->GetStationId() == Sim->GetCommanderStation())
		{
			return Canvas->ClipY - 118.0f;
		}
		if (Vehicle->GetStationId() == Sim->GetPlotterStation())
		{
			return Canvas->ClipY - 96.0f;
		}
	}
	return Canvas->ClipY - 54.0f;
}

void ASimCabinHUD::DrawBacking(float X, float Y, float W, float H)
{
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f), X - 6.0f, Y - 3.0f, W + 12.0f, H + 6.0f);
}

void ASimCabinHUD::DrawRadio(USimWorldSubsystem* Sim, UFont* Font, float Bottom)
{
	// The lines still fresh, packed at the bottom: the oldest fades.
	const double Now = Sim->GetSimTimeSeconds();
	const TArray<FSimRadioMessage> Messages = Sim->GetRadioMessages(RadioLines);
	TArray<TPair<FString, float>> Lines;
	float Widest = 0.0f;
	for (const FSimRadioMessage& M : Messages)
	{
		const double Age = Now - M.TimeSeconds;
		if (Age > RadioLineSeconds && Now > 0.0)
		{
			continue;
		}
		const FString Text = FString::Printf(TEXT("[%s] %s: %s"), *M.Net, *M.From, *M.Text);
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Text, W, H, Font);
		Widest = FMath::Max(Widest, W);
		Lines.Emplace(Text, FMath::Clamp(1.0f - static_cast<float>(Age) / RadioLineSeconds, 0.35f, 1.0f));
	}
	if (Lines.Num() == 0)
	{
		return;
	}
	float Y = Bottom - 16.0f * Lines.Num();
	DrawBacking(20.0f, Y, Widest, 16.0f * Lines.Num());
	for (const TPair<FString, float>& L : Lines)
	{
		DrawText(L.Key, FLinearColor(0.75f, 0.95f, 0.75f, L.Value), 20.0f, Y, Font);
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
	DrawText(FString::Printf(TEXT("PU-12 plotting board   plot %d   (B to close)"), Newest), Grid, Origin.X + 8.f, Origin.Y + Size - 18.f, Font);
}

