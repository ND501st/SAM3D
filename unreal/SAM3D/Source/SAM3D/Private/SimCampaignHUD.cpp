#include "SimCampaignHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
#include "UnrealClient.h"

namespace
{
	const FLinearColor CPaper(0.93f, 0.92f, 0.86f, 1.0f);
	const FLinearColor CDim(0.62f, 0.64f, 0.58f, 1.0f);
	const FLinearColor CAmber(1.0f, 0.78f, 0.35f, 1.0f);
	const FLinearColor CRed(1.0f, 0.38f, 0.3f, 1.0f);
	const FLinearColor CGreen(0.55f, 0.85f, 0.5f, 1.0f);
	const FLinearColor CBlue(0.55f, 0.75f, 1.0f, 1.0f);

	struct FSeatChoice
	{
		const TCHAR* Id;
		const TCHAR* Label;
	};
	const FSeatChoice Seats[] = {
		{TEXT("commander"), TEXT("the commander's seat, in command")},
		{TEXT("deputy_commander"), TEXT("the deputy commander's seat, in the crew")},
		{TEXT("senior_operator"), TEXT("the senior operator's seat, in the crew")},
		{TEXT("operator"), TEXT("the operator's seat, in the crew")},
		{TEXT("driver"), TEXT("the driver's seat, in the crew")},
		{TEXT(""), TEXT("outside, no seat")},
	};
	constexpr int32 NumSeats = UE_ARRAY_COUNT(Seats);
	constexpr int32 NumPostureRows = 8;

	FString CampaignToF(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}
}

ASimCampaignHUD::ASimCampaignHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ASimCampaignHUD::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("SimCampaignTest="), TestFolder);
	if (USimAirCampaignSubsystem* C = Campaign())
	{
		Message = C->GetLastRaidReport();
	}
	UE_LOG(LogSAM3D, Log, TEXT("Air war: the campaign screen"));
}

USimAirCampaignSubsystem* ASimCampaignHUD::Campaign() const
{
	const UGameInstance* Gi = GetGameInstance();
	return Gi != nullptr ? Gi->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
}

void ASimCampaignHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandleInput();
	if (!TestFolder.IsEmpty())
	{
		RunScriptedCheck(DeltaSeconds);
	}
}

void ASimCampaignHUD::Advance()
{
	USimAirCampaignSubsystem* C = Campaign();
	if (C == nullptr || !C->HasCampaign() || C->IsEnded())
	{
		return;
	}
	if (C->HasPendingRaid())
	{
		Message = TEXT("A raid is waiting: fight it (Enter) or let it pass (L).");
		return;
	}
	if (C->AdvanceToNextRaid(24.0f))
	{
		Message = TEXT("Raid warning.");
	}
	else
	{
		Message = TEXT("Twenty-four hours without a raid through the battery's sector.");
	}
	Scroll = 0;
}

void ASimCampaignHUD::Fight()
{
	USimAirCampaignSubsystem* C = Campaign();
	if (C == nullptr || !C->HasPendingRaid())
	{
		return;
	}
	FString Error;
	if (!C->FightPendingRaid(this, Seats[SeatIndex].Id, Error))
	{
		Message = TEXT("The raid can't be played here (") + Error + TEXT("); it was fought without you.");
	}
}

void ASimCampaignHUD::ChangePosture(int32 Row, int32 Step)
{
	USimAirCampaignSubsystem* C = Campaign();
	if (C == nullptr || !C->HasCampaign())
	{
		return;
	}
	FSimBatteryPosture P = C->GetPosture();
	const float F = 0.1f * static_cast<float>(Step);
	switch (Row)
	{
		case 0: P.Emission = FMath::Clamp(P.Emission + F, 0.0f, 1.0f); break;
		case 1: P.EmconDiscipline = FMath::Clamp(P.EmconDiscipline + F, 0.0f, 1.0f); break;
		case 2: P.RadioDiscipline = FMath::Clamp(P.RadioDiscipline + F, 0.0f, 1.0f); break;
		case 3: P.bDummyTraffic = !P.bDummyTraffic; break;
		case 4: P.bDecoyEmitters = !P.bDecoyEmitters; break;
		case 5: P.bNets = !P.bNets; break;
		case 6: P.bFightSilent = !P.bFightSilent; break;
		case 7: P.RelocateEveryH = FMath::Clamp(P.RelocateEveryH + 4.0f * static_cast<float>(Step), 0.0f, 48.0f); break;
		default: break;
	}
	C->SetPosture(P);
}

void ASimCampaignHUD::HandleInput()
{
	APlayerController* Pc = PlayerOwner;
	USimAirCampaignSubsystem* C = Campaign();
	if (Pc == nullptr)
	{
		return;
	}
	auto Pressed = [&](const FKey& K) { return Pc->WasInputKeyJustPressed(K); };
	if (Pressed(EKeys::Q) || Pressed(EKeys::Escape))
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("SimMenu"));
		return;
	}
	if (C == nullptr || !C->HasCampaign())
	{
		return;
	}
	if (Pressed(EKeys::Tab))
	{
		Panel = static_cast<EPanel>((static_cast<int32>(Panel) + 1) % 4);
		Scroll = 0;
	}
	const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	for (int32 I = 0; I < 4; ++I)
	{
		if (Pressed(Digits[I]))
		{
			Panel = static_cast<EPanel>(I);
			Scroll = 0;
		}
	}
	if (Panel == EPanel::Posture)
	{
		if (Pressed(EKeys::Down))
		{
			PostureRow = (PostureRow + 1) % NumPostureRows;
		}
		if (Pressed(EKeys::Up))
		{
			PostureRow = (PostureRow + NumPostureRows - 1) % NumPostureRows;
		}
		if (Pressed(EKeys::Right))
		{
			ChangePosture(PostureRow, 1);
		}
		if (Pressed(EKeys::Left))
		{
			ChangePosture(PostureRow, -1);
		}
	}
	else
	{
		if (Pressed(EKeys::Down) || Pressed(EKeys::PageDown) || Pressed(EKeys::MouseScrollDown))
		{
			++Scroll;
		}
		if ((Pressed(EKeys::Up) || Pressed(EKeys::PageUp) || Pressed(EKeys::MouseScrollUp)) && Scroll > 0)
		{
			--Scroll;
		}
	}
	if (Pressed(EKeys::LeftBracket))
	{
		SeatIndex = (SeatIndex + NumSeats - 1) % NumSeats;
	}
	if (Pressed(EKeys::RightBracket))
	{
		SeatIndex = (SeatIndex + 1) % NumSeats;
	}
	if (Pressed(EKeys::SpaceBar))
	{
		Advance();
	}
	if (Pressed(EKeys::Enter))
	{
		Fight();
		return;
	}
	if (Pressed(EKeys::L) && C->HasPendingRaid())
	{
		C->LetPendingRaidPass();
		Message = TEXT("The raid was fought without you.");
	}
	if (Pressed(EKeys::M) && bHasMark)
	{
		FString Why;
		if (C->RelocateBattery(MarkKm, Why))
		{
			Message = TEXT("The battery has moved to the marked position.");
			bHasMark = false;
		}
		else
		{
			Message = TEXT("The battery can't move there: ") + Why + TEXT(".");
		}
	}
	if (Pressed(EKeys::E))
	{
		C->EndCampaign();
	}
	if (Pressed(EKeys::LeftMouseButton))
	{
		float Mx = 0.0f;
		float My = 0.0f;
		if (!Pc->GetMousePosition(Mx, My))
		{
			return;
		}
		const FVector2D At(Mx, My);
		for (const FClick& Click : Clicks)
		{
			if (!Click.Box.bIsValid || !Click.Box.IsInside(At))
			{
				continue;
			}
			const FString& Id = Click.Id;
			if (Id.StartsWith(TEXT("tab:")))
			{
				Panel = static_cast<EPanel>(FCString::Atoi(*Id.Mid(4)));
				Scroll = 0;
			}
			else if (Id == TEXT("advance"))
			{
				Advance();
			}
			else if (Id == TEXT("fight"))
			{
				Fight();
			}
			else if (Id == TEXT("pass"))
			{
				C->LetPendingRaidPass();
				Message = TEXT("The raid was fought without you.");
			}
			else if (Id == TEXT("seat-"))
			{
				SeatIndex = (SeatIndex + NumSeats - 1) % NumSeats;
			}
			else if (Id == TEXT("seat+"))
			{
				SeatIndex = (SeatIndex + 1) % NumSeats;
			}
			else if (Id == TEXT("end"))
			{
				C->EndCampaign();
			}
			else if (Id == TEXT("menu"))
			{
				UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("SimMenu"));
			}
			else if (Id.StartsWith(TEXT("posture:")))
			{
				TArray<FString> Parts;
				Id.ParseIntoArray(Parts, TEXT(":"));
				if (Parts.Num() == 3)
				{
					PostureRow = FCString::Atoi(*Parts[1]);
					ChangePosture(PostureRow, FCString::Atoi(*Parts[2]));
				}
			}
			return;
		}
		if (MapBox.bIsValid && MapBox.IsInside(At) && !C->IsEnded())
		{
			MarkKm = ToTheatre(At) / 1000.0;
			bHasMark = true;
			Message = FString::Printf(TEXT("Marked %.0f km east, %.0f km north: M moves the battery there."), MarkKm.X, MarkKm.Y);
		}
	}
}

float ASimCampaignHUD::DrawWrapped(const FString& Text, float X, float Y, float Width, UFont* Font, const FLinearColor& Colour, float LineGap,
	float Bottom)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	FString Line;
	auto Flush = [&]()
	{
		if (Line.IsEmpty())
		{
			return;
		}
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Line, W, H, Font);
		if (Y + H <= Bottom)
		{
			DrawText(Line, Colour, X, Y, Font);
		}
		Y += H + LineGap;
		Line.Reset();
	};
	for (const FString& Word : Words)
	{
		const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Try, W, H, Font);
		if (W > Width && !Line.IsEmpty())
		{
			Flush();
			Line = Word;
		}
		else
		{
			Line = Try;
		}
	}
	Flush();
	return Y;
}

bool ASimCampaignHUD::Button(const FString& Label, float X, float Y, float W, float H, bool bPrimary)
{
	DrawRect(bPrimary ? FLinearColor(1.0f, 0.78f, 0.35f, 0.85f) : FLinearColor(1.0f, 0.78f, 0.35f, 0.18f), X, Y, W, H);
	float Tw = 0.0f;
	float Th = 0.0f;
	UFont* Font = GEngine->GetMediumFont();
	GetTextSize(Label, Tw, Th, Font);
	DrawText(Label, bPrimary ? FLinearColor(0.05f, 0.05f, 0.04f, 1.0f) : CPaper, X + (W - Tw) * 0.5f, Y + (H - Th) * 0.5f, Font);
	return true;
}

FVector2D ASimCampaignHUD::ToScreen(double EastM, double NorthM) const
{
	return FVector2D(MapOrigin.X + (EastM - MapMin.X) * MapScale, MapOrigin.Y - (NorthM - MapMin.Y) * MapScale);
}

FVector2D ASimCampaignHUD::ToTheatre(const FVector2D& Screen) const
{
	return FVector2D(MapMin.X + (Screen.X - MapOrigin.X) / MapScale, MapMin.Y + (MapOrigin.Y - Screen.Y) / MapScale);
}

void ASimCampaignHUD::DrawMap(float X, float Y, float W, float H)
{
	using namespace simcore::airwar;
	const USimAirCampaignSubsystem* Sub = Campaign();
	const AirCampaign* C = Sub != nullptr ? Sub->GetCampaign() : nullptr;
	DrawRect(FLinearColor(0.03f, 0.05f, 0.04f, 0.9f), X, Y, W, H);
	// The drawing area, with a strip under it for the hint.
	const float HintH = 20.0f;
	H -= HintH;
	MapBox = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	DrawLine(X, Y + H, X + W, Y + H, FLinearColor(1.0f, 0.78f, 0.35f, 0.2f), 1.0f);
	DrawText(TEXT("Click the map to mark a new position"), CDim, X + 8.0f, Y + H + 3.0f, GEngine->GetSmallFont());
	if (C == nullptr)
	{
		return;
	}
	const TheatreDef& Th = C->Data().theatre;
	const FrontDef* Front = Th.FrontFor(C->Data().faction.front);
	// Bounds: our objects and the active front with its entry points.
	FVector2D Lo(1e18, 1e18);
	FVector2D Hi(-1e18, -1e18);
	auto Grow = [&](double E, double N)
	{
		Lo.X = FMath::Min(Lo.X, E);
		Lo.Y = FMath::Min(Lo.Y, N);
		Hi.X = FMath::Max(Hi.X, E);
		Hi.Y = FMath::Max(Hi.Y, N);
	};
	for (const ObjectState& S : C->Truth())
	{
		Grow(S.pos.x, S.pos.y);
	}
	if (Front != nullptr)
	{
		for (const simcore::Vec3& P : Front->line)
		{
			Grow(P.x, P.y);
		}
		for (const EntryPoint& E : Front->entries)
		{
			Grow(E.pos.x, E.pos.y);
		}
	}
	const FVector2D Pad = (Hi - Lo) * 0.06 + FVector2D(5000.0, 5000.0);
	Lo -= Pad;
	Hi += Pad;
	MapMin = Lo;
	MapMax = Hi;
	MapScale = static_cast<float>(FMath::Min(W / (Hi.X - Lo.X), H / (Hi.Y - Lo.Y)));
	const float UsedW = static_cast<float>((Hi.X - Lo.X) * MapScale);
	const float UsedH = static_cast<float>((Hi.Y - Lo.Y) * MapScale);
	MapOrigin = FVector2D(X + (W - UsedW) * 0.5f, Y + H - (H - UsedH) * 0.5f);

	auto Circle = [&](const FVector2D& Centre, float R, const FLinearColor& Colour, float Thick)
	{
		const int32 N = 40;
		for (int32 I = 0; I < N; ++I)
		{
			const float A0 = 2.0f * PI * I / N;
			const float A1 = 2.0f * PI * (I + 1) / N;
			MapLine(FVector2D(Centre.X + R * FMath::Cos(A0), Centre.Y + R * FMath::Sin(A0)),
				FVector2D(Centre.X + R * FMath::Cos(A1), Centre.Y + R * FMath::Sin(A1)), Colour, Thick);
		}
	};
	auto Diamond = [&](const FVector2D& P, float S, const FLinearColor& Colour, bool bFill)
	{
		if (bFill)
		{
			DrawRect(Colour, P.X - S * 0.5f, P.Y - S * 0.5f, S, S);
		}
		DrawLine(P.X, P.Y - S, P.X + S, P.Y, Colour, 1.5f);
		DrawLine(P.X + S, P.Y, P.X, P.Y + S, Colour, 1.5f);
		DrawLine(P.X, P.Y + S, P.X - S, P.Y, Colour, 1.5f);
		DrawLine(P.X - S, P.Y, P.X, P.Y - S, Colour, 1.5f);
	};
	UFont* Small = GEngine->GetSmallFont();
	// Symbols and labels only where they fit inside the drawing area.
	auto Inside = [&](const FVector2D& P, float Margin)
	{
		return P.X >= MapBox.Min.X + Margin && P.X <= MapBox.Max.X - Margin && P.Y >= MapBox.Min.Y + Margin && P.Y <= MapBox.Max.Y - Margin;
	};
	auto Label = [&](const FString& Text, const FLinearColor& Colour, float Lx, float Ly)
	{
		float Tw = 0.0f;
		float Th = 0.0f;
		GetTextSize(Text, Tw, Th, Small);
		if (Lx + Tw > MapBox.Max.X - 2.0f)
		{
			Lx = MapBox.Max.X - 2.0f - Tw;
		}
		if (Lx >= MapBox.Min.X + 2.0f && Ly >= MapBox.Min.Y + 2.0f && Ly + Th <= MapBox.Max.Y - 2.0f)
		{
			DrawText(Text, Colour, Lx, Ly, Small);
		}
	};

	// The other fronts faintly, the enemy's front in red with its entry points.
	for (const FrontDef& F : Th.fronts)
	{
		const bool bActive = &F == Front;
		for (int32 I = 0; I + 1 < static_cast<int32>(F.line.size()); ++I)
		{
			const FVector2D A = ToScreen(F.line[I].x, F.line[I].y);
			const FVector2D B = ToScreen(F.line[I + 1].x, F.line[I + 1].y);
			MapLine(A, B, bActive ? CRed : FLinearColor(0.5f, 0.3f, 0.3f, 0.35f), bActive ? 3.0f : 1.0f);
		}
	}
	if (Front != nullptr)
	{
		for (const EntryPoint& E : Front->entries)
		{
			const FVector2D P = ToScreen(E.pos.x, E.pos.y);
			if (!Inside(P, 6.0f))
			{
				continue;
			}
			DrawLine(P.X - 6.0f, P.Y + 5.0f, P.X + 6.0f, P.Y + 5.0f, CRed, 2.0f);
			DrawLine(P.X + 6.0f, P.Y + 5.0f, P.X, P.Y - 6.0f, CRed, 2.0f);
			DrawLine(P.X, P.Y - 6.0f, P.X - 6.0f, P.Y + 5.0f, CRed, 2.0f);
			Label(CampaignToF(E.id), CRed, P.X + 8.0f, P.Y - 8.0f);
		}
	}
	// The raids seen in the last 24 hours, the battery's sector brighter.
	const std::vector<ObservedRaid>& Seen = C->ObservedRaids();
	for (std::size_t Ri = 0; Ri < Seen.size(); ++Ri)
	{
		const ObservedRaid& R = Seen[Ri];
		if (R.h < C->Now() - 24.0 || R.h > C->Now() + 0.01)
		{
			continue;
		}
		const simcore::command::InfoNet* RaidNet = Sub != nullptr ? Sub->GetInfoNet() : nullptr;
		if (RaidNet != nullptr && RaidNet->Known(RaidNet->Player(), RaidNet->FactIndex("raid:" + std::to_string(Ri))) == nullptr)
		{
			continue;  // not reported to you
		}
		const FVector2D A = ToScreen(R.entry_pos.x, R.entry_pos.y);
		const FVector2D B = ToScreen(R.target_pos.x, R.target_pos.y);
		const float Fade = static_cast<float>(FMath::Clamp(1.0 - (C->Now() - R.h) / 24.0, 0.25, 1.0));
		MapLine(A, B, R.near_battery ? FLinearColor(1.0f, 0.55f, 0.2f, 0.9f * Fade) : FLinearColor(1.0f, 0.55f, 0.2f, 0.3f * Fade),
			R.near_battery ? 2.0f : 1.0f);
	}
	// Our side: towns, objects, the battery and its dummies.
	const int32 Player = C->PlayerIndex();
	for (int32 I = 0; I < static_cast<int32>(Th.objects.size()); ++I)
	{
		const TheatreObject& O = Th.objects[I];
		const ObjectState& S = C->Truth()[I];
		const FVector2D P = ToScreen(S.pos.x, S.pos.y);
		if (O.kind == ObjectKind::City)
		{
			Circle(P, FMath::Max(3.0f, static_cast<float>(O.radius_m) * MapScale), FLinearColor(0.7f, 0.7f, 0.7f, 0.5f), 1.0f);
			// The town by the battery is named under it, the battery's label goes above.
			const FVector2D Bp0 = ToScreen(C->Truth()[Player].pos.x, C->Truth()[Player].pos.y);
			const bool bByBattery = FVector2D::Distance(P, Bp0) < 40.0f;
			Label(CampaignToF(O.name), CDim, P.X + 6.0f, bByBattery ? P.Y + 10.0f : P.Y + 4.0f);
			continue;
		}
		if (I == Player || O.decoy || !Inside(P, 4.0f))
		{
			continue;
		}
		// The player's map holds where our side's objects are, not how they stand: damage only as it was reported.
		const FLinearColor Col(0.55f, 0.62f, 0.55f, 0.8f);
		if (IsAirDefence(O.kind))
		{
			Diamond(P, 4.0f, Col, false);
		}
		else
		{
			DrawRect(Col, P.X - 3.0f, P.Y - 3.0f, 6.0f, 6.0f);
		}
		const simcore::command::InfoNet* Net = Sub != nullptr ? Sub->GetInfoNet() : nullptr;
		const int32 Wreck = Net != nullptr ? Net->FindFact(simcore::command::FactKind::OwnDamage, O.id) : -1;
		if (Wreck >= 0 && Net->Known(Net->Player(), Wreck) != nullptr && S.function <= 0.05)
		{
			DrawLine(P.X - 6.0f, P.Y - 6.0f, P.X + 6.0f, P.Y + 6.0f, CRed, 2.0f);
			DrawLine(P.X - 6.0f, P.Y + 6.0f, P.X + 6.0f, P.Y - 6.0f, CRed, 2.0f);
		}
	}
	// The district's villages and the posts of the chain of command (data/command/places_pannonia.json).
	if (const simcore::command::InfoNet* PlaceNet = Sub != nullptr ? Sub->GetInfoNet() : nullptr)
	{
		for (int32 Pi = 0; Pi < static_cast<int32>(PlaceNet->Data().places.size()); ++Pi)
		{
			const simcore::command::Place& Pl = PlaceNet->Data().places[static_cast<std::size_t>(Pi)];
			const simcore::Vec3 At = PlaceNet->PlacePos(Pi, C->Now());
			const FVector2D P = ToScreen(At.x, At.y);
			if (!Inside(P, 3.0f) || !Pl.theatre_object.empty())
			{
				continue;
			}
			if (Pl.kind == simcore::command::PlaceKind::Village)
			{
				DrawRect(CDim, P.X - 1.5f, P.Y - 1.5f, 3.0f, 3.0f);
				Label(CampaignToF(Pl.name), CDim, P.X + 4.0f, P.Y - 6.0f);
			}
			else if (Pl.kind == simcore::command::PlaceKind::MilitaryPost)
			{
				DrawLine(P.X - 3.0f, P.Y + 3.0f, P.X + 3.0f, P.Y + 3.0f, CBlue, 1.0f);
				DrawLine(P.X - 3.0f, P.Y + 3.0f, P.X, P.Y - 3.0f, CBlue, 1.0f);
				DrawLine(P.X + 3.0f, P.Y + 3.0f, P.X, P.Y - 3.0f, CBlue, 1.0f);
			}
		}
	}
	for (int32 D : C->Dummies())
	{
		const ObjectState& S = C->Truth()[D];
		const FVector2D P = ToScreen(S.pos.x, S.pos.y);
		if (Inside(P, 5.0f))
		{
			Diamond(P, 5.0f, S.function > 0.0 ? FLinearColor(1.0f, 0.78f, 0.35f, 0.6f) : CRed, false);
		}
	}
	const ObjectState& B = C->Truth()[Player];
	const FVector2D Bp = ToScreen(B.pos.x, B.pos.y);
	Circle(Bp, static_cast<float>(10000.0 * MapScale), FLinearColor(1.0f, 0.78f, 0.35f, 0.5f), 1.0f);
	Diamond(Bp, 8.0f, B.function > 0.3 ? CAmber : CRed, true);
	Label(TEXT("Battery 21"), CAmber, Bp.X - 24.0f, Bp.Y - 24.0f);
	if (bHasMark)
	{
		const FVector2D M = ToScreen(MarkKm.X * 1000.0, MarkKm.Y * 1000.0);
		std::string Why;
		const bool bOk = C->CanRelocateBattery(simcore::Vec3{MarkKm.X * 1000.0, MarkKm.Y * 1000.0, 0.0}, &Why);
		const FLinearColor Col = bOk ? CBlue : CRed;
		MapLine(Bp, M, Col, 1.0f);
		DrawLine(M.X - 7.0f, M.Y, M.X + 7.0f, M.Y, Col, 2.0f);
		DrawLine(M.X, M.Y - 7.0f, M.X, M.Y + 7.0f, Col, 2.0f);
		Label(bOk ? FString(TEXT("M: move here")) : CampaignToF(Why), Col, M.X + 9.0f, M.Y + 4.0f);
	}
}

bool ASimCampaignHUD::MapLine(FVector2D A, FVector2D B, const FLinearColor& Colour, float Thickness)
{
	// Liang-Barsky: the part of A-B inside MapBox.
	const FVector2D D = B - A;
	double T0 = 0.0;
	double T1 = 1.0;
	const double P[4] = {-D.X, D.X, -D.Y, D.Y};
	const double Q[4] = {A.X - MapBox.Min.X, MapBox.Max.X - A.X, A.Y - MapBox.Min.Y, MapBox.Max.Y - A.Y};
	for (int32 I = 0; I < 4; ++I)
	{
		if (FMath::Abs(P[I]) < 1e-9)
		{
			if (Q[I] < 0.0)
			{
				return false;
			}
			continue;
		}
		const double T = Q[I] / P[I];
		if (P[I] < 0.0)
		{
			T0 = FMath::Max(T0, T);
		}
		else
		{
			T1 = FMath::Min(T1, T);
		}
		if (T0 > T1)
		{
			return false;
		}
	}
	const FVector2D From = A + D * T0;
	const FVector2D To = A + D * T1;
	DrawLine(From.X, From.Y, To.X, To.Y, Colour, Thickness);
	return true;
}

float ASimCampaignHUD::DrawPanel(float X, float Y, float W, float H)
{
	using namespace simcore::airwar;
	USimAirCampaignSubsystem* Sub = Campaign();
	const AirCampaign* C = Sub != nullptr ? Sub->GetCampaign() : nullptr;
	if (C == nullptr)
	{
		return Y;
	}
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const double Start = C->Data().theatre.start_hour;
	const float Bottom = Y + H;
	const simcore::command::InfoNet* Net = Sub->GetInfoNet();

	if (Sub->IsEnded())
	{
		DrawText(TEXT("THE ENEMY'S STAFF, AS IT REALLY WAS"), CAmber, X, Y, Medium);
		Y += 26.0f;
		TArray<FString> Lines;
		CampaignToF(C->EnemyMind()).ParseIntoArrayLines(Lines);
		int32 Skip = Scroll;
		for (const FString& L : Lines)
		{
			if (Skip-- > 0)
			{
				continue;
			}
			Y = DrawWrapped(L, X + (L.StartsWith(TEXT("      ")) ? 16.0f : 0.0f), Y, W - 16.0f, Small,
				L.StartsWith(TEXT("      ")) ? CDim : CPaper, 2.0f, Bottom);
			if (Y > Bottom)
			{
				break;
			}
		}
		return Y;
	}

	// Tabs.
	const TCHAR* Names[] = {TEXT("1 What you know"), TEXT("2 Your chain"), TEXT("3 Posture"), TEXT("4 Raids you know of")};
	float Tx = X;
	for (int32 I = 0; I < 4; ++I)
	{
		float Tw = 0.0f;
		float Th = 0.0f;
		GetTextSize(Names[I], Tw, Th, Medium);
		const bool bOn = static_cast<int32>(Panel) == I;
		DrawRect(bOn ? FLinearColor(1.0f, 0.78f, 0.35f, 0.25f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), Tx, Y, Tw + 16.0f, Th + 8.0f);
		DrawText(Names[I], bOn ? CPaper : CDim, Tx + 8.0f, Y + 4.0f, Medium);
		PendingClicks.Add({FBox2D(FVector2D(Tx, Y), FVector2D(Tx + Tw + 16.0f, Y + Th + 8.0f)), FString::Printf(TEXT("tab:%d"), I)});
		Tx += Tw + 22.0f;
	}
	Y += 34.0f;

	switch (Panel)
	{
		case EPanel::Intsum:
		{
			// First person (docs/command/01_PRINCIPLES.md): only what has reached the player's character, each with who
			// told them, how and when.
			if (Net == nullptr)
			{
				DrawWrapped(TEXT("No word reaches you: the chain of command's data did not load."), X, Y, W, Small, CDim);
				break;
			}
			const std::vector<simcore::command::Delivery> Known = Net->KnownBy(Net->Player());
			if (Known.empty())
			{
				DrawWrapped(TEXT("Nothing has reached you yet."), X, Y, W, Small, CDim);
			}
			int32 Skip = Scroll;
			for (int32 I = static_cast<int32>(Known.size()) - 1; I >= 0 && Y < Bottom; --I)
			{
				if (Skip-- > 0)
				{
					continue;
				}
				const simcore::command::Delivery& D = Known[static_cast<std::size_t>(I)];
				const simcore::command::Fact& F = Net->Facts()[static_cast<std::size_t>(D.fact)];
				Y = DrawWrapped(CampaignToF(Net->Describe(D)), X, Y, W, Small, CDim, 2.0f, Bottom);
				FString Body = CampaignToF(D.full ? F.text : F.Told());
				if (F.document && !D.full)
				{
					Body += TEXT(" (The paper has not reached you yet.)");
				}
				Y = DrawWrapped(Body, X + 16.0f, Y, W - 16.0f, Small, F.kind == simcore::command::FactKind::AirWarning ? CAmber : CPaper, 2.0f, Bottom) + 6.0f;
			}
			break;
		}
		case EPanel::Staff:
		{
			if (Net == nullptr || Net->Player() < 0)
			{
				DrawWrapped(TEXT("The chain of command's data did not load."), X, Y, W, Small, CDim);
				break;
			}
			const simcore::command::ChainData& Chain = Net->Data();
			const int32 Me = Net->Player();
			const simcore::command::Position& P = Chain.positions[static_cast<std::size_t>(Me)];
			auto Where = [&](int32 Pi)
			{
				const int Pl = Net->PlaceOf(Pi, C->Now());
				return Pl >= 0 ? CampaignToF(Chain.places[static_cast<std::size_t>(Pl)].name) : FString();
			};
			Y = DrawWrapped(FString::Printf(TEXT("You: %s %s, %s. Now at %s."), *CampaignToF(P.holder.rank), *CampaignToF(P.holder.name), *CampaignToF(P.title),
								*Where(Me)),
					X, Y, W, Small, CPaper, 2.0f, Bottom) +
				6.0f;
			if (const simcore::command::Position* Sup = Chain.FindPosition(P.superior))
			{
				Y = DrawWrapped(FString::Printf(TEXT("You answer to %s, at %s."), *CampaignToF(Sup->Label()), *Where(Chain.PositionIndex(Sup->id))), X, Y, W,
						Small, CPaper, 2.0f, Bottom) +
					4.0f;
			}
			for (const std::string& L : P.links)
			{
				const int32 Li = Chain.PositionIndex(L);
				Y = DrawWrapped(FString::Printf(TEXT("You deal with %s, at %s."), *CampaignToF(Chain.positions[static_cast<std::size_t>(Li)].Label()), *Where(Li)),
						X, Y, W, Small, CDim, 2.0f, Bottom) +
					2.0f;
			}
			FString Under;
			for (const simcore::command::Position& Q : Chain.positions)
			{
				if (Q.superior == P.id)
				{
					Under += (Under.IsEmpty() ? TEXT("") : TEXT("; ")) + CampaignToF(Q.holder.rank + " " + Q.holder.name + " (" + Q.title + ")");
				}
			}
			if (!Under.IsEmpty())
			{
				Y = DrawWrapped(TEXT("Under you: ") + Under + TEXT("."), X, Y + 4.0f, W, Small, CDim, 2.0f, Bottom) + 4.0f;
			}
			Y += 8.0f;
			DrawText(TEXT("Coming contacts"), CAmber, X, Y, Small);
			Y += 20.0f;
			int32 Shown = 0;
			for (const auto& Contact : Net->NextContacts(Me, C->Now(), 48.0))
			{
				if (Shown++ >= 7 || Y > Bottom)
				{
					break;
				}
				const simcore::command::Routine& R = *Contact.second;
				const FString At = R.place.empty() ? FString() : TEXT(", at ") + CampaignToF(Net->PlaceName(R.place));
				Y = DrawWrapped(FString::Printf(TEXT("%s  %s%s"), *CampaignToF(simcore::command::FormatClock(Contact.first, Start)), *CampaignToF(R.name), *At), X, Y, W,
						Small, CPaper, 2.0f, Bottom) +
					2.0f;
			}
			Y = DrawWrapped(TEXT("Nothing reaches you except through these people, the radio, the telephone, a runner, a courier or the news; "
								 "and what you don't pass on, the people under you don't learn."),
				X, Y + 8.0f, W, Small, CDim, 2.0f, Bottom);
			break;
		}
		case EPanel::Posture:
		{
			const FSimBatteryPosture P = Sub->GetPosture();
			auto Pct = [](float V) { return FString::Printf(TEXT("%.0f%%"), V * 100.0f); };
			const FString Rows[NumPostureRows][2] = {
				{TEXT("Radars radiate (share of each hour, between fights)"), Pct(P.Emission)},
				{TEXT("EMCON discipline (off when ARMs come, short bursts)"), Pct(P.EmconDiscipline)},
				{TEXT("Radio discipline (little voice traffic)"), Pct(P.RadioDiscipline)},
				{TEXT("Dummy radio traffic"), P.bDummyTraffic ? TEXT("on") : TEXT("off")},
				{TEXT("Decoy emitters at the dummies"), P.bDecoyEmitters ? TEXT("on") : TEXT("off")},
				{TEXT("Camouflage nets"), P.bNets ? TEXT("on") : TEXT("off")},
				{TEXT("Fight silent (TV and the PU-12's designations)"), P.bFightSilent ? TEXT("yes") : TEXT("no")},
				{TEXT("Move every"), P.RelocateEveryH > 0.0f ? FString::Printf(TEXT("%.0f h"), P.RelocateEveryH) : TEXT("only when ordered")},
			};
			for (int32 R = 0; R < NumPostureRows; ++R)
			{
				const bool bSel = R == PostureRow;
				if (bSel)
				{
					DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.15f), X - 4.0f, Y - 2.0f, W, 24.0f);
				}
				DrawText(Rows[R][0], bSel ? CPaper : CDim, X, Y, Small);
				const float Vx = X + W - 150.0f;
				DrawText(TEXT("<"), CAmber, Vx, Y, Medium);
				DrawText(Rows[R][1], CPaper, Vx + 24.0f, Y, Small);
				DrawText(TEXT(">"), CAmber, Vx + 120.0f, Y, Medium);
				PendingClicks.Add({FBox2D(FVector2D(Vx - 4.0f, Y - 2.0f), FVector2D(Vx + 16.0f, Y + 20.0f)), FString::Printf(TEXT("posture:%d:-1"), R)});
				PendingClicks.Add({FBox2D(FVector2D(Vx + 116.0f, Y - 2.0f), FVector2D(Vx + 136.0f, Y + 20.0f)), FString::Printf(TEXT("posture:%d:1"), R)});
				Y += 28.0f;
			}
			Y += 8.0f;
			Y = DrawWrapped(TEXT("Every second the radars radiate is a fix for the enemy's ELINT; every word on the net, a bearing for its "
								 "intercept service. Moving makes its picture stale; dummies and decoy emitters give it something else to "
								 "strike. Up/Down a row, Left/Right change it."),
				X, Y, W, Small, CDim, 2.0f, Bottom);
			break;
		}
		case EPanel::Raids:
		{
			int32 Skip = Scroll;
			const std::vector<ObservedRaid>& Rs = C->ObservedRaids();
			for (int32 I = static_cast<int32>(Rs.size()) - 1; I >= 0 && Y < Bottom; --I)
			{
				const ObservedRaid& R = Rs[I];
				if (Net != nullptr && Net->Known(Net->Player(), Net->FactIndex("raid:" + std::to_string(I))) == nullptr)
				{
					continue;  // not reported to you (yet)
				}
				if (Skip-- > 0)
				{
					continue;
				}
				const FString Line = FString::Printf(TEXT("%s  from %s, %d aircraft at %s (%s): %d shot down%s%s"), *CampaignToF(FormatHour(R.h, Start)),
					*CampaignToF(R.entry), R.aircraft, *CampaignToF(R.target), *CampaignToF(DayPartName(R.part)), R.lost,
					R.delivered ? TEXT(", bombs dropped") : TEXT(""), R.played ? TEXT("  [you fought it]") : (R.near_battery ? TEXT("  [the battery's sector]") : TEXT("")));
				Y = DrawWrapped(Line, X, Y, W, Small, R.near_battery ? CAmber : CPaper, 2.0f, Bottom) + 2.0f;
			}
			break;
		}
	}
	return Y;
}

void ASimCampaignHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr || GEngine == nullptr)
	{
		return;
	}
	PendingClicks.Reset();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Margin = FMath::Max(20.0f, W * 0.025f);
	DrawRect(FLinearColor(0.015f, 0.02f, 0.015f, 0.92f), 0.0f, 0.0f, W, H);

	USimAirCampaignSubsystem* Sub = Campaign();
	const simcore::airwar::AirCampaign* C = Sub != nullptr ? Sub->GetCampaign() : nullptr;
	float Y = Margin * 0.7f;
	if (C == nullptr)
	{
		DrawText(TEXT("AIR WAR"), CAmber, Margin, Y, Large, 1.3f);
		DrawText(TEXT("No campaign: start one from the missions menu (1-4)."), CPaper, Margin, Y + 50.0f, Medium);
		Button(TEXT("The missions (Q)"), Margin, H - Margin - 34.0f, 200.0f, 34.0f, true);
		PendingClicks.Add({FBox2D(FVector2D(Margin, H - Margin - 34.0f), FVector2D(Margin + 200.0f, H - Margin)), TEXT("menu")});
		Clicks = PendingClicks;
		return;
	}

	// Header.
	DrawText(TEXT("AIR WAR: ") + CampaignToF(C->Data().faction.name).ToUpper(), CAmber, Margin, Y, Large, 1.1f);
	Y += 40.0f;
	DrawText(FString::Printf(TEXT("%s   weather %s   crew %s   battery %s   shot down by your battery: %d"),
				 *CampaignToF(C->Clock()), *CampaignToF(simcore::airwar::WeatherName(C->WeatherNow())), *CampaignToF(C->CrewTier()),
				 C->Truth()[C->PlayerIndex()].function > 0.3 ? TEXT("in action") : TEXT("being repaired"), C->KillsByPlayer()),
		CDim, Margin, Y, Small);
	Y += 20.0f;
	// What the enemy knows of the battery: only the regiment's estimate, once it has reached you.
	FString Est = TEXT("The regiment has not told you what the enemy knows of the battery.");
	FLinearColor EstColour = CDim;
	if (const simcore::command::InfoNet* Net = Sub->GetInfoNet())
	{
		const std::vector<simcore::command::Delivery> Known = Net->KnownBy(Net->Player());
		for (auto It = Known.rbegin(); It != Known.rend(); ++It)
		{
			const simcore::command::Fact& F = Net->Facts()[static_cast<std::size_t>(It->fact)];
			if (F.kind == simcore::command::FactKind::EnemyKnowsBattery)
			{
				Est = CampaignToF(F.text);
				EstColour = F.precedence >= 1 ? CRed : CAmber;
				break;
			}
		}
	}
	DrawText(Est, EstColour, Margin, Y, Small);
	Y += 26.0f;
	DrawLine(Margin, Y, W - Margin, Y, FLinearColor(1.0f, 0.78f, 0.35f, 0.4f), 1.0f);
	Y += 10.0f;

	// Map left, panel right, actions below.
	const float ActionsH = 120.0f;
	const float BodyH = H - Y - ActionsH - Margin;
	const float MapW = FMath::Min(W * 0.46f, BodyH * 1.15f);
	DrawMap(Margin, Y, MapW, BodyH);
	const float Px = Margin + MapW + Margin;
	DrawPanel(Px, Y, W - Px - Margin, BodyH);

	// Actions.
	float Ay = H - Margin - ActionsH + 10.0f;
	if (!Message.IsEmpty())
	{
		DrawWrapped(Message, Margin, Ay, W - 2.0f * Margin, Small, CDim);
	}
	Ay += 24.0f;
	const float Bh = 34.0f;
	float Bx = Margin;
	auto AddButton = [&](const FString& Label, const TCHAR* Id, float Bw, bool bPrimary)
	{
		Button(Label, Bx, Ay + 40.0f, Bw, Bh, bPrimary);
		PendingClicks.Add({FBox2D(FVector2D(Bx, Ay + 40.0f), FVector2D(Bx + Bw, Ay + 40.0f + Bh)), Id});
		Bx += Bw + 12.0f;
	};
	if (Sub->IsEnded())
	{
		DrawText(TEXT("The campaign is over. Scroll the reveal with the wheel or Up/Down."), CAmber, Margin, Ay, Medium);
		AddButton(TEXT("The missions (Q)"), TEXT("menu"), 220.0f, true);
	}
	else if (Sub->HasPendingRaid())
	{
		DrawWrapped(TEXT("RAID WARNING. ") + Sub->GetPendingRaidWarning(), Margin, Ay, W - 2.0f * Margin, Medium, CAmber);
		DrawText(FString::Printf(TEXT("Fight it from  [  %s  ]"), Seats[SeatIndex].Label), CPaper, Margin, Ay + 40.0f + Bh + 8.0f, Small);
		AddButton(TEXT("Fight it (Enter)"), TEXT("fight"), 200.0f, true);
		AddButton(TEXT("Let it pass (L)"), TEXT("pass"), 180.0f, false);
		AddButton(TEXT("<  seat  ["), TEXT("seat-"), 120.0f, false);
		AddButton(TEXT("]  seat  >"), TEXT("seat+"), 120.0f, false);
	}
	else
	{
		DrawText(TEXT("Set the battery's posture, move it if the enemy knows where it is, and wait for the next raid."), CPaper, Margin, Ay, Small);
		AddButton(TEXT("Next raid (Space)"), TEXT("advance"), 220.0f, true);
		AddButton(TEXT("End the campaign (E)"), TEXT("end"), 230.0f, false);
		AddButton(TEXT("The missions (Q)"), TEXT("menu"), 200.0f, false);
	}
	Clicks = PendingClicks;
}

void ASimCampaignHUD::RunScriptedCheck(float DeltaSeconds)
{
	TestClock += DeltaSeconds;
	USimAirCampaignSubsystem* C = Campaign();
	if (TestStep == 0)
	{
		if (C != nullptr && !C->HasCampaign())
		{
			FString Error;
			C->NewCampaign(TEXT("nato"), 1, Error);
		}
		TestStep = 1;
	}
	else if (TestStep == 1 && TestClock >= 3.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(TestFolder, TEXT("campaign.png")), false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Air war check: campaign.png"));
		TestStep = 2;
	}
	else if (TestStep == 2 && TestClock >= 4.0f)
	{
		for (int32 I = 0; I < 4 && C != nullptr && !C->HasPendingRaid(); ++I)
		{
			C->AdvanceToNextRaid(24.0f);
		}
		UE_LOG(LogSAM3D, Log, TEXT("Air war check: %s"), C != nullptr && C->HasPendingRaid() ? *C->GetPendingRaidWarning() : TEXT("no raid in four days"));
		TestStep = 3;
	}
	else if (TestStep == 3 && TestClock >= 6.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(TestFolder, TEXT("campaign_raid.png")), false, false);
		TestStep = 4;
	}
	else if (TestStep == 4 && TestClock >= 8.0f)
	{
		UE_LOG(LogSAM3D, Log, TEXT("Air war check: done"));
		FGenericPlatformMisc::RequestExit(false);
	}
}
