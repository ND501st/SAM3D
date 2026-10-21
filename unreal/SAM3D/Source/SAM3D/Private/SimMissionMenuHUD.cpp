#include "SimMissionMenuHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "Engine/GameInstance.h"
#include "SimAirCampaignSubsystem.h"
#include "UnrealClient.h"

namespace
{
	const FLinearColor Paper(0.93f, 0.92f, 0.86f, 1.0f);
	const FLinearColor Dim(0.62f, 0.64f, 0.58f, 1.0f);
	const FLinearColor Amber(1.0f, 0.78f, 0.35f, 1.0f);
	const FLinearColor Bad(1.0f, 0.4f, 0.3f, 1.0f);
}

ASimMissionMenuHUD::ASimMissionMenuHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ASimMissionMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Sim->GetMissions(Missions, LoadError);
	}
	if (Missions.Num() > 0)
	{
		StartIndex = DefaultStart(Missions[0]);
	}
	UE_LOG(LogSAM3D, Log, TEXT("Missions menu: %d missions%s%s"), Missions.Num(), LoadError.IsEmpty() ? TEXT("") : TEXT(", "), *LoadError);
	FParse::Value(FCommandLine::Get(), TEXT("SimMenuTest="), TestFolder);
	FParse::Value(FCommandLine::Get(), TEXT("SimMenuPick="), TestPick);
}

TArray<ASimMissionMenuHUD::FStartChoice> ASimMissionMenuHUD::StartChoices(const FSimMission& Mission) const
{
	TArray<FStartChoice> Out;
	for (const FSimMissionVehicle& V : Mission.Vehicles)
	{
		const FString In = Mission.bLink ? V.Label + TEXT(": ") : FString();
		for (const FSimMissionSeat& S : V.Seats)
		{
			Out.Add({V.Id, S.Id, FString::Printf(TEXT("%s%s, %s"), *In, *S.Label, S.bCommands ? TEXT("in command") : TEXT("in the crew"))});
		}
		Out.Add({V.Id, FString(), FString::Printf(TEXT("%soutside, no seat (C, Tab and T take one)"), *In)});
	}
	return Out;
}

int32 ASimMissionMenuHUD::DefaultStart(const FSimMission& Mission) const
{
	const TArray<FStartChoice> Choices = StartChoices(Mission);
	// The mission's own choice (missions.json "start": the tutorial starts outside, to show the views first).
	if (!Mission.Start.IsEmpty() && Mission.Vehicles.Num() > 0)
	{
		const FString Seat = Mission.Start == TEXT("outside") ? FString() : Mission.Start;
		for (int32 I = 0; I < Choices.Num(); ++I)
		{
			if (Choices[I].Vehicle == Mission.Vehicles[0].Id && Choices[I].Seat == Seat)
			{
				return I;
			}
		}
	}
	// Otherwise the first vehicle's commanding seat: the commander's view of the whole drill.
	if (Mission.Vehicles.Num() > 0)
	{
		for (int32 I = 0; I < Choices.Num(); ++I)
		{
			for (const FSimMissionSeat& S : Mission.Vehicles[0].Seats)
			{
				if (S.bCommands && Choices[I].Vehicle == Mission.Vehicles[0].Id && Choices[I].Seat == S.Id)
				{
					return I;
				}
			}
		}
	}
	return 0;
}

bool ASimMissionMenuHUD::Select(const FString& MissionId, const FString& VehicleId, const FString& SeatId)
{
	for (int32 M = 0; M < Missions.Num(); ++M)
	{
		if (Missions[M].Id != MissionId)
		{
			continue;
		}
		// No vehicle named: the mission's first (the only one but in a battery link).
		const FString Want = VehicleId.IsEmpty() && Missions[M].Vehicles.Num() > 0 ? Missions[M].Vehicles[0].Id : VehicleId;
		const TArray<FStartChoice> Choices = StartChoices(Missions[M]);
		for (int32 I = 0; I < Choices.Num(); ++I)
		{
			if (Choices[I].Seat == SeatId && Choices[I].Vehicle == Want)
			{
				Selected = M;
				StartIndex = I;
				return true;
			}
		}
	}
	return false;
}

FString ASimMissionMenuHUD::OptionsFor(const FString& MissionId, const FString& VehicleId, const FString& SeatId, const FString& Tutorial)
{
	FString Options = FString::Printf(TEXT("SimMission=%s"), *MissionId);
	if (!VehicleId.IsEmpty())
	{
		Options += FString::Printf(TEXT("?SimVehicle=%s"), *VehicleId);
	}
	if (!SeatId.IsEmpty())
	{
		Options += FString::Printf(TEXT("?SimSeat=%s"), *SeatId);
	}
	if (!Tutorial.IsEmpty())
	{
		Options += FString::Printf(TEXT("?SimTutorial=%s"), *Tutorial);
	}
	return Options;
}

void ASimMissionMenuHUD::StartSelected()
{
	if (!Missions.IsValidIndex(Selected))
	{
		return;
	}
	const TArray<FStartChoice> Choices = StartChoices(Missions[Selected]);
	if (!Choices.IsValidIndex(StartIndex))
	{
		return;
	}
	const FString Options = OptionsFor(Missions[Selected].Id, Choices[StartIndex].Vehicle, Choices[StartIndex].Seat, Missions[Selected].Tutorial);
	// A mission on a map of its own (missions.json "level") opens that level; the rest reopen this one.
	const FString Level = Missions[Selected].Level.IsEmpty() ? UGameplayStatics::GetCurrentLevelName(this, true) : Missions[Selected].Level;
	UE_LOG(LogSAM3D, Log, TEXT("Missions menu: starting %s in %s (%s)"), *Missions[Selected].Id, *Level, *Options);
	UGameplayStatics::OpenLevel(this, FName(*Level), true, Options);
}

void ASimMissionMenuHUD::CycleHelper()
{
	const FString Next = USimWorldSubsystem::NextHelperLevel(USimWorldSubsystem::GetHelperPreference());
	USimWorldSubsystem::SetHelperPreference(Next);
	UE_LOG(LogSAM3D, Log, TEXT("Missions menu: the helper %s"), *Next);
}

void ASimMissionMenuHUD::MoveSelection(int32 Step)
{
	if (Missions.Num() == 0)
	{
		return;
	}
	Selected = (Selected + Step + Missions.Num()) % Missions.Num();
	StartIndex = DefaultStart(Missions[Selected]);
}

void ASimMissionMenuHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandleInput();
	if (!TestFolder.IsEmpty())
	{
		RunScriptedCheck(DeltaSeconds);
	}
}

void ASimMissionMenuHUD::HandleInput()
{
	APlayerController* Pc = PlayerOwner;
	if (Pc == nullptr || Missions.Num() == 0)
	{
		if (Pc != nullptr && Pc->WasInputKeyJustPressed(EKeys::Q))
		{
			FGenericPlatformMisc::RequestExit(false);
		}
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Down) || Pc->WasInputKeyJustPressed(EKeys::S))
	{
		MoveSelection(1);
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Up) || Pc->WasInputKeyJustPressed(EKeys::W))
	{
		MoveSelection(-1);
	}
	const int32 Starts = StartChoices(Missions[Selected]).Num();
	if (Pc->WasInputKeyJustPressed(EKeys::Right) || Pc->WasInputKeyJustPressed(EKeys::D))
	{
		StartIndex = (StartIndex + 1) % FMath::Max(1, Starts);
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Left) || Pc->WasInputKeyJustPressed(EKeys::A))
	{
		StartIndex = (StartIndex + Starts - 1) % FMath::Max(1, Starts);
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Enter) || Pc->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		StartSelected();
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::H))
	{
		CycleHelper();
	}
	// The air war: 1-4 a new campaign against that adversary, C continues the one under way.
	const FKey CampaignKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	for (int32 I = 0; I < 4; ++I)
	{
		if (Pc->WasInputKeyJustPressed(CampaignKeys[I]))
		{
			StartCampaign(I);
			return;
		}
	}
	if (Pc->WasInputKeyJustPressed(EKeys::C))
	{
		ContinueCampaign();
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::Q))
	{
		FGenericPlatformMisc::RequestExit(false);
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		float Mx = 0.0f;
		float My = 0.0f;
		if (Pc->GetMousePosition(Mx, My))
		{
			const FVector2D At(Mx, My);
			if (StartBox.bIsValid && StartBox.IsInside(At))
			{
				StartSelected();
				return;
			}
			if (HelperBox.bIsValid && HelperBox.IsInside(At))
			{
				CycleHelper();
				return;
			}
			for (int32 I = 0; I < CampaignBoxes.Num(); ++I)
			{
				if (CampaignBoxes[I].bIsValid && CampaignBoxes[I].IsInside(At))
				{
					if (I < 4)
					{
						StartCampaign(I);
					}
					else
					{
						ContinueCampaign();
					}
					return;
				}
			}
			for (int32 I = 0; I < RowBoxes.Num(); ++I)
			{
				if (RowBoxes[I].bIsValid && RowBoxes[I].IsInside(At) && I != Selected)
				{
					Selected = I;
					StartIndex = DefaultStart(Missions[Selected]);
				}
			}
		}
	}
}

float ASimMissionMenuHUD::DrawWrapped(const FString& Text, float X, float Y, float Width, UFont* Font, const FLinearColor& Colour, float LineGap)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	FString Line;
	float LineH = 0.0f;
	auto Flush = [&]()
	{
		if (Line.IsEmpty())
		{
			return;
		}
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Line, W, H, Font);
		DrawText(Line, Colour, X, Y, Font);
		LineH = FMath::Max(LineH, H);
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

void ASimMissionMenuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr || GEngine == nullptr)
	{
		return;
	}
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Margin = FMath::Max(24.0f, W * 0.04f);
	DrawRect(FLinearColor(0.015f, 0.02f, 0.015f, 0.86f), 0.0f, 0.0f, W, H);

	float Y = Margin * 0.8f;
	DrawText(TEXT("SAM3D"), Amber, Margin, Y, Large, 1.4f);
	Y += 46.0f;
	DrawText(TEXT("9K33M2 Osa-AK battery, 1980s: choose a mission and your seat"), Dim, Margin, Y, Medium);
	Y += 34.0f;
	DrawLine(Margin, Y, W - Margin, Y, FLinearColor(1.0f, 0.78f, 0.35f, 0.4f), 1.0f);
	Y += 14.0f;
	if (Missions.Num() == 0)
	{
		DrawWrapped(LoadError.IsEmpty() ? FString(TEXT("No missions.")) : TEXT("The missions don't load: ") + LoadError, Margin, Y, W - 2.0f * Margin, Medium, Bad);
		DrawText(TEXT("Q quit"), Dim, Margin, H - Margin, Medium);
		return;
	}

	// The list, by group.
	const float ListW = W * 0.38f;
	const float Top = Y;
	RowBoxes.SetNum(Missions.Num());
	FString Group;
	for (int32 I = 0; I < Missions.Num(); ++I)
	{
		const FSimMission& M = Missions[I];
		if (M.GroupId != Group)
		{
			Group = M.GroupId;
			Y += I == 0 ? 0.0f : 10.0f;
			DrawText(M.GroupLabel.ToUpper(), Amber, Margin, Y, Medium);
			Y += 26.0f;
		}
		const float RowH = 24.0f;
		RowBoxes[I] = FBox2D(FVector2D(Margin, Y - 2.0f), FVector2D(Margin + ListW, Y + RowH - 2.0f));
		if (I == Selected)
		{
			DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.18f), Margin - 6.0f, Y - 2.0f, ListW, RowH);
		}
		DrawText(M.Title, I == Selected ? Paper : Dim, Margin + 8.0f, Y, Medium);
		Y += RowH;
	}

	// The selected mission.
	const FSimMission& M = Missions[Selected];
	const float X = Margin + ListW + Margin;
	const float Width = W - X - Margin;
	Y = Top;
	Y = DrawWrapped(M.Name, X, Y, Width, Medium, Paper, 4.0f) + 8.0f;
	Y = DrawWrapped(M.Description, X, Y, Width, Small, Paper, 3.0f) + 12.0f;
	Y = DrawWrapped(M.GroupNotes, X, Y, Width, Small, Dim, 3.0f) + 18.0f;
	const TArray<FStartChoice> Choices = StartChoices(M);
	if (Choices.IsValidIndex(StartIndex))
	{
		DrawText(TEXT("Start as"), Amber, X, Y, Medium);
		Y += 26.0f;
		DrawText(FString::Printf(TEXT("<  %s  >"), *Choices[StartIndex].Label), Paper, X, Y, Medium);
		Y += 26.0f;
		DrawText(FString::Printf(TEXT("%d of %d: Left and Right choose"), StartIndex + 1, Choices.Num()), Dim, X, Y, Small);
		Y += 30.0f;
	}
	// The helper: what the seat's NPC would do, highlighted like a lesson's step (docs/crew/13_HELPER.md).
	DrawText(TEXT("Helper"), Amber, X, Y, Medium);
	Y += 26.0f;
	const FString HelperLine = FString::Printf(TEXT("<  %s  >"), *USimWorldSubsystem::HelperLevelLabel(USimWorldSubsystem::GetHelperPreference()));
	float Hw = 0.0f;
	float Hh = 0.0f;
	GetTextSize(HelperLine, Hw, Hh, Medium);
	HelperBox = FBox2D(FVector2D(X, Y - 2.0f), FVector2D(X + Hw, Y + Hh + 2.0f));
	DrawText(HelperLine, Paper, X, Y, Medium);
	Y += 26.0f;
	DrawText(TEXT("H turns it: the switch, wheel, order or report your seat's crewman would take next, lit in the cabin"), Dim, X, Y, Small);
	Y += 30.0f;
	const float Bw = 160.0f;
	const float Bh = 34.0f;
	StartBox = FBox2D(FVector2D(X, Y), FVector2D(X + Bw, Y + Bh));
	DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.85f), X, Y, Bw, Bh);
	DrawText(TEXT("Start"), FLinearColor(0.05f, 0.05f, 0.04f, 1.0f), X + 52.0f, Y + 7.0f, Medium);

	const float BoxW = FMath::Min(420.0f, W * 0.3f);
	DrawCampaignBox(W - Margin - BoxW, H - Margin - 200.0f, BoxW);

	DrawText(TEXT("Up / Down a mission    Left / Right where you start    H the helper    Enter start    1-4 air war campaign    Q quit    (in a mission: Esc for the menu)"),
		Dim, Margin, H - Margin, Small);
}

namespace
{
	const TCHAR* const CampaignFactions[] = {TEXT("rebels"), TEXT("neutral"), TEXT("warsaw_pact"), TEXT("nato")};
	const TCHAR* const CampaignLabels[] = {TEXT("1  Rebels: a civil war, light air power"), TEXT("2  The neutral neighbour: raids along the front"),
		TEXT("3  The Warsaw Pact: massed blows, basic SEAD"), TEXT("4  NATO: packages, SEAD and DEAD, jamming")};
}

void ASimMissionMenuHUD::StartCampaign(int32 Index)
{
	USimAirCampaignSubsystem* AirWar = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
	if (AirWar == nullptr || Index < 0 || Index >= 4)
	{
		return;
	}
	FString Error;
	const int32 Seed = 1 + static_cast<int32>(FDateTime::Now().GetTicks() % 100000);
	if (!AirWar->NewCampaign(CampaignFactions[Index], Seed, Error))
	{
		LoadError = TEXT("The campaign doesn't start: ") + Error;
		return;
	}
	USimAirCampaignSubsystem::OpenCampaignScreen(this);
}

void ASimMissionMenuHUD::ContinueCampaign()
{
	USimAirCampaignSubsystem* AirWar = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
	if (AirWar == nullptr)
	{
		return;
	}
	if (!AirWar->HasCampaign())
	{
		FString Error;
		if (!AirWar->HasSavedCampaign() || !AirWar->LoadSavedCampaign(Error))
		{
			LoadError = Error.IsEmpty() ? FString(TEXT("No campaign to continue.")) : TEXT("The saved campaign does not load: ") + Error;
			return;
		}
	}
	USimAirCampaignSubsystem::OpenCampaignScreen(this);
}

void ASimMissionMenuHUD::DrawCampaignBox(float X, float Y, float Width)
{
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const USimAirCampaignSubsystem* AirWar = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
	DrawRect(FLinearColor(1.0f, 0.78f, 0.35f, 0.08f), X - 10.0f, Y - 6.0f, Width + 20.0f, 172.0f);
	DrawText(TEXT("AIR WAR CAMPAIGN"), Amber, X, Y, Medium);
	Y += 26.0f;
	DrawText(TEXT("The enemy's air staff plans the raids; you fight what comes."), Dim, X, Y, Small);
	Y += 22.0f;
	CampaignBoxes.SetNum(5);
	for (int32 I = 0; I < 4; ++I)
	{
		CampaignBoxes[I] = FBox2D(FVector2D(X - 4.0f, Y - 2.0f), FVector2D(X + Width, Y + 20.0f));
		DrawText(CampaignLabels[I], Paper, X, Y, Small);
		Y += 22.0f;
	}
	if (AirWar != nullptr && (AirWar->HasCampaign() || AirWar->HasSavedCampaign()))
	{
		CampaignBoxes[4] = FBox2D(FVector2D(X - 4.0f, Y - 2.0f), FVector2D(X + Width, Y + 20.0f));
		DrawText(AirWar->HasCampaign() ? FString(TEXT("C  Continue the campaign under way"))
									   : FString::Printf(TEXT("C  Continue the saved campaign (%s)"), *AirWar->GetSavedCampaignLabel()),
			Amber, X, Y, Small);
	}
	else
	{
		CampaignBoxes[4] = FBox2D(ForceInit);
	}
}

void ASimMissionMenuHUD::RunScriptedCheck(float DeltaSeconds)
{
	TestClock += DeltaSeconds;
	if (TestStep == 0 && TestClock >= 4.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(TestFolder, TEXT("menu.png")), false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Missions menu check: menu.png"));
		TestStep = 1;
	}
	else if (TestStep == 1 && TestClock >= 6.0f)
	{
		if (TestPick.IsEmpty())
		{
			FGenericPlatformMisc::RequestExit(false);
			return;
		}
		TArray<FString> Parts;
		TestPick.ParseIntoArray(Parts, TEXT(":"), false);
		while (Parts.Num() < 3)
		{
			Parts.Add(FString());
		}
		const bool bFound = Select(Parts[0], Parts[1], Parts[2]);
		UE_LOG(LogSAM3D, Log, TEXT("Missions menu check: pick %s -> %s"), *TestPick, bFound ? TEXT("found") : TEXT("NOT FOUND"));
		if (!bFound)
		{
			FGenericPlatformMisc::RequestExit(false);
			return;
		}
		TestStep = 2;
	}
	else if (TestStep == 2 && TestClock >= 8.0f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(TestFolder, TEXT("menu_pick.png")), false, false);
		TestStep = 3;
	}
	else if (TestStep == 3 && TestClock >= 10.0f)
	{
		TestStep = 4;
		StartSelected();
	}
}
