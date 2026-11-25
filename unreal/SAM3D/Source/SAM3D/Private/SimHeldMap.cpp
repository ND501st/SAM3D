// The held map (track O, O2; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md sections 3 and 5): M in the cabin shows the
// map the player's character holds, full screen, drawn as a Soviet-pattern sheet: the numbered 1 km grid, the sheet's
// number and margin with the classification stamped in it, brown contours from the map's heights, green woods, blue
// water, roads by class and dark settlement blocks, and the marks in red (ours) and blue (the enemy) pencil with their
// labels and hours. Only simcore's HeldMap is drawn on it (what reached the holder); the ground is the map's own data.
// The mouse wheel zooms about the cursor, dragging moves the sheet, hovering a mark shows who told you, by what and when.
// At the battery commander's seat, with the task received: 1 / 2 / 3 and a click place the main, reserve and dummy
// positions, S picks the next launcher and two clicks give its sector's edges, X clears the draft, Enter gives the
// combat order from the map (the runner's SetBatteryDecision, then the existing combat order).
#include "SimCabinHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "SAM3D.h"
#include "SimMapActor.h"
#include "SimOsaVehicle.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"

#include <algorithm>
#include <map>
#include <vector>

namespace
{
std::string HmStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
FString HmF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
// An id the player reads, as words ("field_telephone" -> "field telephone").
FString HmWords(const std::string& S) { return HmF(S).Replace(TEXT("_"), TEXT(" ")); }

// The sheet's colours: paper, the printed map's inks and the pencils.
const FLinearColor CPaper(0.93f, 0.91f, 0.83f, 1.0f);
const FLinearColor CMargin(0.97f, 0.96f, 0.92f, 1.0f);
const FLinearColor CInk(0.08f, 0.08f, 0.08f, 1.0f);
const FLinearColor CGrid(0.15f, 0.15f, 0.15f, 0.55f);
const FLinearColor CContour(0.62f, 0.40f, 0.20f, 0.75f);
const FLinearColor CWood(0.67f, 0.83f, 0.58f, 1.0f);
const FLinearColor CWater(0.60f, 0.79f, 0.92f, 1.0f);
const FLinearColor CWaterLine(0.15f, 0.40f, 0.75f, 1.0f);
const FLinearColor CMainRoad(0.88f, 0.42f, 0.16f, 1.0f);
const FLinearColor CRoad(0.96f, 0.84f, 0.30f, 1.0f);
const FLinearColor CTown(0.27f, 0.24f, 0.22f, 0.9f);
const FLinearColor CRed(0.80f, 0.10f, 0.08f, 1.0f);
const FLinearColor CBlue(0.08f, 0.22f, 0.78f, 1.0f);
const FLinearColor CStamp(0.75f, 0.10f, 0.10f, 0.85f);

uint32 HmHash(uint32 X)
{
	X ^= X >> 16;
	X *= 0x7feb352dU;
	X ^= X >> 15;
	X *= 0x846ca68bU;
	X ^= X >> 16;
	return X;
}
}  // namespace

struct FSimHeldMapState
{
	simcore::command::HeldMap Map;
	FString Holder;
	double BuiltAt = -1e9;
	// The view: the sheet's centre (session ENU metres) and its scale.
	double CentreE = 0.0;
	double CentreN = 0.0;
	double MPerPx = 20.0;
	bool bCentred = false;
	bool bDragging = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FBox2D Sheet = FBox2D(ForceInit);
	// The ground's contours, by stride (computed once each).
	const simcore::MapData* Ground = nullptr;
	std::map<int, std::vector<simcore::command::ContourSegment>> Contours;
	// The battery commander's draft decision (not yet given).
	simcore::command::BatteryDecision Draft;
	bool bDraftDirty = false;
	int32 Tool = 0;  // 0 none, 1 main, 2 reserve, 3 dummy, 4 a sector's first edge, 5 its second
	int32 Launcher = -1;
	double SectorFrom = 0.0;
	std::vector<std::string> Launchers;
	FString Note;
	double NoteUntil = 0.0;
	static constexpr double kContourM = 5.0;  // sam3d: the interval of a flat country's 1:50 000 sheet (index every 25 m)

	FVector2D ToScreen(double E, double N) const
	{
		const FVector2D C = Sheet.GetCenter();
		return FVector2D(C.X + (E - CentreE) / MPerPx, C.Y - (N - CentreN) / MPerPx);
	}
	FVector2D ToMap(const FVector2D& S) const
	{
		const FVector2D C = Sheet.GetCenter();
		return FVector2D(CentreE + (S.X - C.X) * MPerPx, CentreN - (S.Y - C.Y) * MPerPx);
	}
};

// ---------------------------------------------------------------------------------------------- opening, the shot hook

void ASimCabinHUD::SetHeldMapOpen(APlayerController* Pc, ASimOsaVehicle* Vehicle, bool bOpen)
{
	bShowHeldMap = bOpen;
	if (Vehicle != nullptr)
	{
		Vehicle->SetHeldMapOpen(bOpen);
	}
	if (!HeldMapState.IsValid())
	{
		HeldMapState = MakeShared<FSimHeldMapState>();
	}
	HeldMapState->BuiltAt = -1e9;
	HeldMapState->bDragging = false;
	HeldMapState->Tool = 0;
	if (Pc == nullptr)
	{
		return;
	}
	Pc->SetShowMouseCursor(bOpen);
	if (bOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Pc->SetInputMode(Mode);
	}
	else
	{
		Pc->SetInputMode(FInputModeGameOnly());
	}
}

void ASimCabinHUD::TickHeldMapShot(USimWorldSubsystem* Sim, ASimOsaVehicle* Vehicle, APlayerController* Pc)
{
	if (!bHeldMapShotParsed)
	{
		bHeldMapShotParsed = true;
		FParse::Value(FCommandLine::Get(), TEXT("SimHeldMapShot="), HeldMapShotFile);
	}
	if (HeldMapShotFile.IsEmpty() || Sim == nullptr || Vehicle == nullptr || !Sim->IsScenarioRunning() || GetWorld() == nullptr)
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const simcore::ScenarioRunner* R = Sim->GetScenarioRunner();
	const bool bTask = R != nullptr && R->HasTask() && R->TaskReceivedS() >= 0.0;
	switch (HeldMapShotStep)
	{
		case 0:  // the regiment's task comes on its net a minute or two in: run the clock fast till then
			Sim->SetTimeScale(8.0f);
			HeldMapShotClock = Now;
			HeldMapShotStep = 1;
			break;
		case 1:
			if (bTask || Now - HeldMapShotClock > 120.0)
			{
				Sim->SetTimeScale(1.0f);
				SetHeldMapOpen(Pc, Vehicle, true);
				HeldMapShotClock = Now;
				HeldMapShotStep = 2;
				UE_LOG(LogSAM3D, Log, TEXT("Held map shot: the map open (%s)"), bTask ? TEXT("the task received") : TEXT("no task yet"));
			}
			break;
		case 2:
			if (Now - HeldMapShotClock > 2.0)
			{
				FScreenshotRequest::RequestScreenshot(HeldMapShotFile, false, false);
				HeldMapShotClock = Now;
				HeldMapShotStep = 3;
			}
			break;
		case 3:  // a decision drawn: the main 600 m west of the order's, launcher 23's sector turned north
			if (Now - HeldMapShotClock > 1.5 && HeldMapState.IsValid())
			{
				FSimHeldMapState& S = *HeldMapState;
				simcore::Vec3 Main;
				if (R != nullptr && R->HasTask() && Sim->GetTaskPlace(HmF(R->Task().main), Main))
				{
					S.Draft = simcore::command::BatteryDecision();
					S.Draft.has_main = true;
					S.Draft.main = simcore::Vec3{Main.x - 600.0, Main.y + 200.0, 0.0};
					for (const simcore::campaign::TaskSector& T : R->Task().sectors)
					{
						S.Draft.sectors.push_back({T.vehicle, T.from_deg, T.to_deg, T.duty});
					}
					if (S.Draft.sectors.size() > 2)
					{
						S.Draft.sectors[2].from_deg = 270.0;
						S.Draft.sectors[2].to_deg = 330.0;
					}
					S.bDraftDirty = true;
				}
				HeldMapShotClock = Now;
				HeldMapShotStep = 4;
			}
			break;
		case 4:
			if (Now - HeldMapShotClock > 1.5)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::GetPath(HeldMapShotFile), FPaths::GetBaseFilename(HeldMapShotFile) + TEXT("_decision.png")), false, false);
				HeldMapShotClock = Now;
				HeldMapShotStep = 5;
			}
			break;
		case 5:
			if (Now - HeldMapShotClock > 2.0)
			{
				UE_LOG(LogSAM3D, Log, TEXT("Held map shot: done"));
				FGenericPlatformMisc::RequestExit(false);
				HeldMapShotStep = 6;
			}
			break;
		default: break;
	}
}

// ---------------------------------------------------------------------------------------------- the sheet

void ASimCabinHUD::DrawHeldMap(USimWorldSubsystem* Sim, ASimOsaVehicle* Vehicle, APlayerController* Pc)
{
	using namespace simcore::command;
	if (!HeldMapState.IsValid())
	{
		HeldMapState = MakeShared<FSimHeldMapState>();
	}
	FSimHeldMapState& S = *HeldMapState;
	UFont* Small = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	UFont* Medium = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	const double Now = GetWorld() != nullptr ? GetWorld()->GetRealTimeSeconds() : 0.0;
	const FString Station = Vehicle->GetStationId();
	const simcore::ScenarioRunner* R = Sim->GetScenarioRunner();

	// The map as it is now held: rebuilt twice a second (a new report, the task, his decision).
	if (Now - S.BuiltAt > 0.5)
	{
		S.Map = Sim->GetHeldMap(Station, S.Holder);
		S.BuiltAt = Now;
	}
	const HeldMapValues Values = R != nullptr ? R->HeldMapValuesOf() : HeldMapValues();
	const bool bDecide = Sim->CanDecideOnMap(Station) && R != nullptr;

	// The sheet: a margin round the drawing, the legend's column on the right.
	const float Cw = Canvas->ClipX;
	const float Ch = Canvas->ClipY;
	const float LegendW = FMath::Min(300.0f, Cw * 0.24f);
	DrawRect(CMargin, 0.0f, 0.0f, Cw, Ch);
	S.Sheet = FBox2D(FVector2D(46.0f, 58.0f), FVector2D(Cw - LegendW - 30.0f, Ch - 64.0f));
	const FBox2D& Box = S.Sheet;
	const float SheetX0 = static_cast<float>(Box.Min.X);
	const float SheetY0 = static_cast<float>(Box.Min.Y);
	const float SheetX1 = static_cast<float>(Box.Max.X);
	const float SheetY1 = static_cast<float>(Box.Max.Y);
	if (!S.bCentred)
	{
		S.bCentred = true;
		if (S.Map.has_centre)
		{
			S.CentreE = S.Map.centre.x;
			S.CentreN = S.Map.centre.y;
		}
		else if (R != nullptr && R->GetBattery() != nullptr && R->GetBattery()->Own() != nullptr)
		{
			S.CentreE = R->GetBattery()->Own()->position.x;
			S.CentreN = R->GetBattery()->Own()->position.y;
		}
		S.MPerPx = 18000.0 / FMath::Max(200.0, Box.GetSize().X);  // about 18 km across to start
	}

	// ---- the mouse and keys
	float Mx = 0.0f;
	float My = 0.0f;
	const bool bMouse = Pc != nullptr && Pc->GetMousePosition(Mx, My);
	const FVector2D Mouse(Mx, My);
	const bool bInSheet = bMouse && Box.IsInside(Mouse);
	auto Pressed = [&](const FKey& K) { return Pc != nullptr && Pc->WasInputKeyJustPressed(K); };
	if (bInSheet)
	{
		const int32 Notches = (Pressed(EKeys::MouseScrollUp) ? 1 : 0) - (Pressed(EKeys::MouseScrollDown) ? 1 : 0);
		if (Notches != 0)
		{
			const FVector2D At = S.ToMap(Mouse);
			S.MPerPx = FMath::Clamp(S.MPerPx * FMath::Pow(0.8, static_cast<double>(Notches)), 2.0, 150.0);
			const FVector2D After = S.ToMap(Mouse);
			S.CentreE += At.X - After.X;
			S.CentreN += At.Y - After.Y;
		}
	}
	const bool bDragKey = Pc != nullptr && (Pc->IsInputKeyDown(EKeys::RightMouseButton) || (S.Tool == 0 && Pc->IsInputKeyDown(EKeys::LeftMouseButton)));
	if (bDragKey && bMouse && (S.bDragging || bInSheet))
	{
		if (S.bDragging)
		{
			S.CentreE -= (Mouse.X - S.DragLast.X) * S.MPerPx;
			S.CentreN += (Mouse.Y - S.DragLast.Y) * S.MPerPx;
		}
		S.bDragging = true;
		S.DragLast = Mouse;
	}
	else
	{
		S.bDragging = false;
	}
	auto Say = [&](const FString& Text, double Seconds)
	{
		S.Note = Text;
		S.NoteUntil = Now + Seconds;
	};
	// The commander's draft starts from his given decision, or the order's own points.
	if (bDecide && !S.bDraftDirty)
	{
		BatteryDecision D;
		if (const BatteryDecision* Given = R->BatteryDecisionHeld())
		{
			D = *Given;
		}
		simcore::Vec3 P;
		if (!D.has_main && Sim->GetTaskPlace(HmF(R->Task().main), P))
		{
			D.has_main = true;
			D.main = P;
		}
		if (D.reserve.empty())
		{
			for (const std::string& Id : R->Task().reserve)
			{
				if (Sim->GetTaskPlace(HmF(Id), P)) D.reserve.push_back(P);
			}
		}
		if (D.dummies.empty())
		{
			for (const std::string& Id : R->Task().dummies)
			{
				if (Sim->GetTaskPlace(HmF(Id), P)) D.dummies.push_back(P);
			}
		}
		if (D.sectors.empty())
		{
			for (const simcore::campaign::TaskSector& T : R->Task().sectors) D.sectors.push_back({T.vehicle, T.from_deg, T.to_deg, T.duty});
		}
		S.Draft = D;
		S.Launchers.clear();
		for (const simcore::campaign::TaskSector& T : R->Task().sectors) S.Launchers.push_back(T.vehicle);
	}
	if (bDecide)
	{
		if (Pressed(EKeys::One)) { S.Tool = 1; Say(TEXT("Click the main position"), 4.0); }
		if (Pressed(EKeys::Two)) { S.Tool = 2; Say(TEXT("Click the reserve position"), 4.0); }
		if (Pressed(EKeys::Three)) { S.Tool = 3; Say(TEXT("Click a dummy position"), 4.0); }
		if (Pressed(EKeys::S) && !S.Launchers.empty())
		{
			S.Launcher = (S.Launcher + 1) % static_cast<int32>(S.Launchers.size());
			S.Tool = 4;
			Say(FString::Printf(TEXT("Launcher %s: click its sector's left edge, then its right (clockwise)"), *HmF(S.Launchers[static_cast<size_t>(S.Launcher)])), 6.0);
		}
		if (Pressed(EKeys::X))
		{
			S.bDraftDirty = false;
			S.Tool = 0;
			Say(TEXT("The draft cleared: the order's points"), 3.0);
		}
		if (Pressed(EKeys::LeftMouseButton) && bInSheet && S.Tool != 0)
		{
			const FVector2D At = S.ToMap(Mouse);
			const simcore::Vec3 P{At.X, At.Y, 0.0};
			const simcore::Vec3 Centre = S.Draft.has_main ? S.Draft.main : P;
			const double Bearing = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(P.x - Centre.x, P.y - Centre.y)) + 360.0, 360.0);
			switch (S.Tool)
			{
				case 1:
					S.Draft.has_main = true;
					S.Draft.main = P;
					S.Tool = 0;
					break;
				case 2:
					S.Draft.reserve = {P};
					S.Tool = 0;
					break;
				case 3:
					S.Draft.dummies.push_back(P);
					S.Tool = 0;
					break;
				case 4:
					S.SectorFrom = Bearing;
					S.Tool = 5;
					break;
				case 5:
					if (S.Launchers.size() > static_cast<size_t>(FMath::Max(0, S.Launcher)))
					{
						const std::string& V = S.Launchers[static_cast<size_t>(FMath::Max(0, S.Launcher))];
						bool bFound = false;
						for (DecidedSector& D : S.Draft.sectors)
						{
							if (D.vehicle == V)
							{
								D.from_deg = FMath::RoundToDouble(S.SectorFrom);
								D.to_deg = FMath::RoundToDouble(Bearing);
								bFound = true;
							}
						}
						if (!bFound) S.Draft.sectors.push_back({V, FMath::RoundToDouble(S.SectorFrom), FMath::RoundToDouble(Bearing), false});
					}
					S.Tool = 0;
					break;
				default: break;
			}
			S.bDraftDirty = true;
		}
		if (Pressed(EKeys::Enter))
		{
			TArray<FString> Breaches;
			FString Why;
			const bool bGiven = Sim->GiveCombatOrderFromMap(S.Draft, Breaches, Why);
			FString Result = bGiven ? FString(TEXT("The combat order given from the map")) : FString(TEXT("The decision kept; the order not given: ")) + Why;
			if (Breaches.Num() > 0)
			{
				Result += TEXT(". Outside the order: ") + FString::Join(Breaches, TEXT("; "));
			}
			Say(Result, 8.0);
			S.bDraftDirty = false;
			S.BuiltAt = -1e9;
		}
	}

	// ---- clipping and the pencil
	auto Clip = [&](FVector2D& A, FVector2D& B) -> bool
	{
		// Liang-Barsky against the sheet.
		double T0 = 0.0;
		double T1 = 1.0;
		const double Dx = B.X - A.X;
		const double Dy = B.Y - A.Y;
		const double P[4] = {-Dx, Dx, -Dy, Dy};
		const double Q[4] = {A.X - Box.Min.X, Box.Max.X - A.X, A.Y - Box.Min.Y, Box.Max.Y - A.Y};
		for (int32 I = 0; I < 4; ++I)
		{
			if (FMath::Abs(P[I]) < 1e-12)
			{
				if (Q[I] < 0.0) return false;
				continue;
			}
			const double T = Q[I] / P[I];
			if (P[I] < 0.0) T0 = FMath::Max(T0, T);
			else T1 = FMath::Min(T1, T);
			if (T0 > T1) return false;
		}
		const FVector2D A0 = A;
		A = FVector2D(A0.X + Dx * T0, A0.Y + Dy * T0);
		B = FVector2D(A0.X + Dx * T1, A0.Y + Dy * T1);
		return true;
	};
	auto Line = [&](FVector2D A, FVector2D B, const FLinearColor& Colour, float Thick)
	{
		if (Clip(A, B))
		{
			DrawLine(static_cast<float>(A.X), static_cast<float>(A.Y), static_cast<float>(B.X), static_cast<float>(B.Y), Colour, Thick);
		}
	};
	auto Dashed = [&](FVector2D A, FVector2D B, const FLinearColor& Colour, float Thick, float On, float Off)
	{
		const double L = FVector2D::Distance(A, B);
		if (L < 1e-3) return;
		const FVector2D U = (B - A) / L;
		for (double T = 0.0; T < L; T += On + Off)
		{
			Line(A + U * T, A + U * FMath::Min(L, T + On), Colour, Thick);
		}
	};
	// A coloured pencil's line: a little unsteady, the same each frame.
	auto Pencil = [&](FVector2D A, FVector2D B, const FLinearColor& Colour, float Thick, uint32 Seed)
	{
		const double L = FVector2D::Distance(A, B);
		const int32 N = FMath::Clamp(static_cast<int32>(L / 22.0), 1, 64);
		const FVector2D Nrm = L > 1e-3 ? FVector2D(-(B.Y - A.Y) / L, (B.X - A.X) / L) : FVector2D::ZeroVector;
		FVector2D Prev = A;
		for (int32 I = 1; I <= N; ++I)
		{
			const double T = static_cast<double>(I) / N;
			const double Wob = I == N ? 0.0 : ((HmHash(Seed * 131u + static_cast<uint32>(I)) % 1000) / 1000.0 - 0.5) * 1.4;
			const FVector2D P = A + (B - A) * T + Nrm * Wob;
			Line(Prev, P, Colour, Thick);
			Prev = P;
		}
	};
	auto Fill = [&](const std::vector<FVector2D>& Poly, const FLinearColor& Colour, float Step)
	{
		if (Poly.size() < 3) return;
		double Y0 = 1e18;
		double Y1 = -1e18;
		for (const FVector2D& P : Poly)
		{
			Y0 = FMath::Min(Y0, P.Y);
			Y1 = FMath::Max(Y1, P.Y);
		}
		Y0 = FMath::Max(Y0, static_cast<double>(Box.Min.Y));
		Y1 = FMath::Min(Y1, static_cast<double>(Box.Max.Y));
		std::vector<double> Xs;
		for (double Y = Y0; Y < Y1; Y += Step)
		{
			Xs.clear();
			const double Yc = Y + Step * 0.5;
			for (size_t I = 0, J = Poly.size() - 1; I < Poly.size(); J = I++)
			{
				const FVector2D& A = Poly[I];
				const FVector2D& B = Poly[J];
				if ((A.Y > Yc) != (B.Y > Yc)) Xs.push_back(A.X + (Yc - A.Y) / (B.Y - A.Y) * (B.X - A.X));
			}
			std::sort(Xs.begin(), Xs.end());
			for (size_t K = 0; K + 1 < Xs.size(); K += 2)
			{
				const double X0 = FMath::Max(Xs[K], static_cast<double>(Box.Min.X));
				const double X1 = FMath::Min(Xs[K + 1], static_cast<double>(Box.Max.X));
				if (X1 > X0) DrawRect(Colour, static_cast<float>(X0), static_cast<float>(Y), static_cast<float>(X1 - X0), Step);
			}
		}
	};
	auto Circle = [&](const FVector2D& C, double Rpx, const FLinearColor& Colour, float Thick, bool bDash, uint32 Seed)
	{
		const int32 N = FMath::Clamp(static_cast<int32>(Rpx * 0.6), 12, 72);
		for (int32 I = 0; I < N; ++I)
		{
			if (bDash && I % 2 == 1) continue;
			const double A0 = 2.0 * PI * I / N;
			const double A1 = 2.0 * PI * (I + 1) / N;
			Pencil(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Rpx, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Rpx, Colour, Thick, Seed + static_cast<uint32>(I));
		}
	};
	auto Toward = [](double Deg) { return FVector2D(FMath::Sin(FMath::DegreesToRadians(Deg)), -FMath::Cos(FMath::DegreesToRadians(Deg))); };  // a screen direction
	auto Text = [&](const FString& T, const FLinearColor& Colour, double X, double Y, UFont* Font)
	{
		if (X < Box.Min.X || Y < Box.Min.Y || X > Box.Max.X - 20.0 || Y > Box.Max.Y - 12.0) return;
		DrawText(T, Colour, static_cast<float>(X), static_cast<float>(Y), Font);
	};

	// ---- the printed map
	DrawRect(CPaper, SheetX0, SheetY0, SheetX1 - SheetX0, SheetY1 - SheetY0);
	const FVector2D ViewLo = S.ToMap(FVector2D(Box.Min.X, Box.Max.Y));
	const FVector2D ViewHi = S.ToMap(FVector2D(Box.Max.X, Box.Min.Y));
	const ASimMapActor* MapActor = Sim->GetGroundMap();
	const simcore::MapData* Ground = MapActor != nullptr ? MapActor->GetMapData() : nullptr;
	if (Ground != S.Ground)
	{
		S.Ground = Ground;
		S.Contours.clear();
	}
	auto Poly = [&](const std::vector<simcore::MapPoint>& Points)
	{
		std::vector<FVector2D> Out;
		Out.reserve(Points.size());
		for (const simcore::MapPoint& P : Points) Out.push_back(S.ToScreen(P.east_m, P.north_m));
		return Out;
	};
	if (Ground != nullptr)
	{
		const simcore::MapDef& D = Ground->Def();
		for (const simcore::MapPolygonDef& F : D.forests)
		{
			const std::vector<FVector2D> P = Poly(F.points);
			Fill(P, CWood, 2.0f);
			for (size_t I = 0; I + 1 <= P.size() && P.size() > 1; ++I) Line(P[I], P[(I + 1) % P.size()], FLinearColor(0.35f, 0.55f, 0.30f, 1.0f), 1.0f);
		}
		for (const simcore::MapLakeDef& L : D.lakes)
		{
			const std::vector<FVector2D> P = Poly(L.points);
			Fill(P, CWater, 2.0f);
			for (size_t I = 0; I < P.size() && P.size() > 1; ++I) Line(P[I], P[(I + 1) % P.size()], CWaterLine, 1.0f);
		}
		for (const simcore::MapPolygonDef& M : D.marshes)
		{
			const std::vector<FVector2D> P = Poly(M.points);
			for (size_t I = 0; I < P.size() && P.size() > 1; ++I) Dashed(P[I], P[(I + 1) % P.size()], CWaterLine, 1.0f, 5.0f, 4.0f);
			// The marsh's sign: short blue strokes across it.
			double Y0 = 1e18, Y1 = -1e18, X0 = 1e18, X1 = -1e18;
			for (const FVector2D& Q : P)
			{
				Y0 = FMath::Min(Y0, Q.Y); Y1 = FMath::Max(Y1, Q.Y); X0 = FMath::Min(X0, Q.X); X1 = FMath::Max(X1, Q.X);
			}
			for (double Y = Y0 + 4.0; Y < Y1; Y += 7.0)
			{
				for (double X = X0 + (static_cast<int32>(Y) % 14 == 0 ? 0.0 : 6.0); X < X1; X += 14.0)
				{
					bool bIn = false;
					for (size_t I = 0, J = P.size() - 1; I < P.size(); J = I++)
					{
						if ((P[I].Y > Y) != (P[J].Y > Y) && X < P[I].X + (Y - P[I].Y) / (P[J].Y - P[I].Y) * (P[J].X - P[I].X)) bIn = !bIn;
					}
					if (bIn) Line(FVector2D(X, Y), FVector2D(X + 6.0, Y), CWaterLine, 1.0f);
				}
			}
		}
		// Contours: brown, every 5 m, every fifth heavier; coarser nodes when the sheet is zoomed out.
		const int32 Stride = S.MPerPx < 8.0 ? 1 : (S.MPerPx < 20.0 ? 2 : (S.MPerPx < 45.0 ? 4 : 8));
		auto It = S.Contours.find(Stride);
		if (It == S.Contours.end())
		{
			const simcore::MapGrid& G = Ground->CoreGrid();
			It = S.Contours.emplace(Stride, ContourSegments(G, FSimHeldMapState::kContourM, D.west_m, D.south_m, D.east_m, D.north_m, Stride)).first;
		}
		for (const ContourSegment& C : It->second)
		{
			if ((C.a.x < ViewLo.X && C.b.x < ViewLo.X) || (C.a.x > ViewHi.X && C.b.x > ViewHi.X) || (C.a.y < ViewLo.Y && C.b.y < ViewLo.Y) ||
				(C.a.y > ViewHi.Y && C.b.y > ViewHi.Y))
			{
				continue;
			}
			Line(S.ToScreen(C.a.x, C.a.y), S.ToScreen(C.b.x, C.b.y), CContour, C.index ? 1.6f : 0.8f);
		}
		for (const simcore::MapLineDef& Rv : D.rivers)
		{
			const std::vector<FVector2D> P = Poly(Rv.points);
			const float W = FMath::Clamp(static_cast<float>(Rv.width_m / S.MPerPx), 1.5f, 8.0f);
			for (size_t I = 0; I + 1 < P.size(); ++I) Line(P[I], P[I + 1], CWaterLine, W);
		}
		for (const simcore::MapLineDef& Rl : D.railways)
		{
			const std::vector<FVector2D> P = Poly(Rl.points);
			for (size_t I = 0; I + 1 < P.size(); ++I)
			{
				Line(P[I], P[I + 1], CInk, 3.0f);
				Dashed(P[I], P[I + 1], CPaper, 1.2f, 6.0f, 6.0f);
			}
		}
		// Roads by class: main red-orange and secondary yellow, cased in black; tracks thin dashed black.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (const simcore::MapLineDef& Rd : D.roads)
			{
				const std::vector<FVector2D> P = Poly(Rd.points);
				const bool bMain = Rd.line_class == "main";
				const bool bTrack = Rd.line_class == "track";
				for (size_t I = 0; I + 1 < P.size(); ++I)
				{
					if (bTrack)
					{
						if (Pass == 1) Dashed(P[I], P[I + 1], CInk, 1.0f, 7.0f, 4.0f);
						continue;
					}
					if (Pass == 0) Line(P[I], P[I + 1], CInk, bMain ? 5.0f : 4.0f);
					else Line(P[I], P[I + 1], bMain ? CMainRoad : CRoad, bMain ? 3.0f : 2.2f);
				}
			}
		}
		// Settlements: dark blocks, named.
		auto Town = [&](const simcore::MapSettlementDef& T)
		{
			const FVector2D C = S.ToScreen(T.centre.east_m, T.centre.north_m);
			const double Rpx = T.radius_m / S.MPerPx;
			const double Bpx = FMath::Max(2.0, 90.0 / S.MPerPx);  // blocks of about 90 m
			const int32 N = FMath::Clamp(static_cast<int32>(Rpx / Bpx), 1, 40);
			for (int32 Iy = -N; Iy <= N; ++Iy)
			{
				for (int32 Ix = -N; Ix <= N; ++Ix)
				{
					const double Bx = C.X + Ix * Bpx * 1.35;
					const double By = C.Y + Iy * Bpx * 1.35;
					if (FMath::Square(Bx - C.X) + FMath::Square(By - C.Y) > Rpx * Rpx) continue;
					if ((HmHash(static_cast<uint32>((Ix + 64) * 977 + (Iy + 64) * 131) ^ static_cast<uint32>(static_cast<int32>(T.centre.east_m))) % 5) == 0) continue;
					if (Bx < Box.Min.X || By < Box.Min.Y || Bx + Bpx > Box.Max.X || By + Bpx > Box.Max.Y) continue;
					DrawRect(CTown, static_cast<float>(Bx), static_cast<float>(By), static_cast<float>(Bpx), static_cast<float>(Bpx));
				}
			}
			const FString Name = T.kind == "village" ? HmF(T.name) : HmF(T.name).ToUpper();
			Text(Name, CInk, C.X + Rpx * 0.75 + 3.0, C.Y - 7.0, T.kind == "village" ? Small : Medium);
		};
		for (const simcore::MapSettlementDef& T : D.settlements) Town(T);
		for (const simcore::MapSettlementDef& T : D.detail_settlements) Town(T);
	}
	// The kilometre grid, numbered in the margin by its last two figures.
	const double GridStep = S.MPerPx > 40.0 ? 2000.0 : 1000.0;
	for (double E = FMath::FloorToDouble(ViewLo.X / GridStep) * GridStep; E <= ViewHi.X; E += GridStep)
	{
		const FVector2D A = S.ToScreen(E, ViewLo.Y);
		Line(FVector2D(A.X, Box.Min.Y), FVector2D(A.X, Box.Max.Y), CGrid, 1.0f);
		if (A.X > Box.Min.X + 6.0 && A.X < Box.Max.X - 12.0)
		{
			const FString N = FString::Printf(TEXT("%02d"), GridNumber(E + 1.0, Values.false_easting_km));
			DrawText(N, CInk, static_cast<float>(A.X) - 6.0f, SheetY0 - 15.0f, Small);
			DrawText(N, CInk, static_cast<float>(A.X) - 6.0f, SheetY1 + 2.0f, Small);
		}
	}
	for (double N = FMath::FloorToDouble(ViewLo.Y / GridStep) * GridStep; N <= ViewHi.Y; N += GridStep)
	{
		const FVector2D A = S.ToScreen(ViewLo.X, N);
		Line(FVector2D(Box.Min.X, A.Y), FVector2D(Box.Max.X, A.Y), CGrid, 1.0f);
		if (A.Y > Box.Min.Y + 6.0 && A.Y < Box.Max.Y - 6.0)
		{
			const FString T = FString::Printf(TEXT("%02d"), GridNumber(N + 1.0, Values.false_northing_km));
			DrawText(T, CInk, SheetX0 - 20.0f, static_cast<float>(A.Y) - 7.0f, Small);
			DrawText(T, CInk, SheetX1 + 4.0f, static_cast<float>(A.Y) - 7.0f, Small);
		}
	}

	// ---- the marks, in pencil
	struct FHover
	{
		const HeldMark* Mark = nullptr;
		double Dist = 12.0;
	} Hover;
	auto Near = [&](const HeldMark& M, const FVector2D& P)
	{
		if (!bInSheet) return;
		const double Dd = FVector2D::Distance(P, Mouse);
		if (Dd < Hover.Dist)
		{
			Hover.Mark = &M;
			Hover.Dist = Dd;
		}
	};
	auto NearSegment = [&](const HeldMark& M, const FVector2D& A, const FVector2D& B)
	{
		if (!bInSheet) return;
		const FVector2D Ab = B - A;
		const double L2 = Ab.SizeSquared();
		const double T = L2 > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(Mouse - A, Ab) / L2, 0.0, 1.0) : 0.0;
		const double Dd = FVector2D::Distance(A + Ab * T, Mouse);
		if (Dd < Hover.Dist)
		{
			Hover.Mark = &M;
			Hover.Dist = Dd;
		}
	};
	// The marks' labels, each stepped down a line until it clears those already written (two launchers sharing a
	// direction, a reserve beside a TZM point).
	TArray<FBox2D> Written;
	auto Unclutter = [&](const FString& L, double X, double Y) -> double
	{
		float Tw = 0.0f, Tl = 0.0f;
		GetTextSize(L, Tw, Tl, Small);
		const double Step = FMath::Max(static_cast<double>(Tl), 10.0);
		for (int32 Tries = 0; Tries < 8; ++Tries)
		{
			const FBox2D B(FVector2D(X, Y), FVector2D(X + Tw, Y + Tl));
			bool bClear = true;
			for (const FBox2D& O : Written) bClear = bClear && !B.Intersect(O);
			if (bClear) break;
			Y += Step;
		}
		Written.Add(FBox2D(FVector2D(X, Y), FVector2D(X + Tw, Y + Tl)));
		return Y;
	};
	auto Label = [&](const HeldMark& M, const FVector2D& P, const FLinearColor& Colour)
	{
		FString L = HmF(M.label);
		if (!M.as_of.empty() && M.origin == MarkOrigin::Copied && M.as_of != "the plan") L += TEXT(" ") + HmF(M.as_of);
		Text(L, Colour, P.X + 7.0, Unclutter(L, P.X + 7.0, P.Y - 6.0), Small);
	};
	auto DrawMark = [&](const HeldMark& M, uint32 Seed)
	{
		FLinearColor Colour = M.side == MarkSide::Ours ? CRed : CBlue;
		if (M.stale) Colour.A = 0.5f;
		const bool bDecision = M.origin == MarkOrigin::Decision;
		const float Th = bDecision ? 2.6f : 1.8f;
		if (M.points.empty()) return;
		const FVector2D P0 = S.ToScreen(M.points[0].x, M.points[0].y);
		if (M.has_arc)
		{
			const double Rpx = M.radius_m / S.MPerPx;
			double Span = FMath::Fmod(M.to_deg - M.from_deg + 360.0, 360.0);
			if (Span < 1e-6) Span = 360.0;
			const bool bSector = M.kind == "sector";
			const FVector2D E0 = P0 + Toward(M.from_deg) * Rpx;
			const FVector2D E1 = P0 + Toward(M.to_deg) * Rpx;
			if (bSector)
			{
				Pencil(P0, E0, Colour, Th, Seed);
				Pencil(P0, E1, Colour, Th, Seed + 7u);
			}
			else
			{
				Dashed(P0 + Toward(M.from_deg) * Rpx * 0.3, E0, Colour, 1.0f, 6.0f, 5.0f);
				Dashed(P0 + Toward(M.to_deg) * Rpx * 0.3, E1, Colour, 1.0f, 6.0f, 5.0f);
			}
			const int32 N = FMath::Clamp(static_cast<int32>(Span / 4.0), 4, 90);
			const double ArcR = bSector ? Rpx : Rpx;
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2D A = P0 + Toward(M.from_deg + Span * I / N) * ArcR;
				const FVector2D B = P0 + Toward(M.from_deg + Span * (I + 1) / N) * ArcR;
				if (M.kind == "jammer") Pencil(A, B, Colour, Th, Seed + static_cast<uint32>(I));
				else if (bSector) Pencil(A, B, Colour, Th * 0.7f, Seed + static_cast<uint32>(I));
				else if (I % 2 == 0) Line(A, B, Colour, 1.0f);
			}
			const double Mid = M.from_deg + Span * 0.5;
			if (!bSector)
			{
				// The direction's arrow, coming in toward the covered object.
				const FVector2D Tail = P0 + Toward(Mid) * Rpx * 0.95;
				const FVector2D Head = P0 + Toward(Mid) * Rpx * 0.35;
				Pencil(Tail, Head, Colour, 2.4f, Seed + 3u);
				const FVector2D U = (Head - Tail).GetSafeNormal();
				const FVector2D Nn(-U.Y, U.X);
				Pencil(Head, Head - U * 12.0 + Nn * 6.0, Colour, 2.4f, Seed + 4u);
				Pencil(Head, Head - U * 12.0 - Nn * 6.0, Colour, 2.4f, Seed + 5u);
				NearSegment(M, Tail, Head);
				Label(M, Tail, Colour);
			}
			else
			{
				const FVector2D Lp = P0 + Toward(Mid) * Rpx * 0.85;
				Text(HmF(M.label), Colour, Lp.X, Unclutter(HmF(M.label), Lp.X, Lp.Y), Small);
				NearSegment(M, P0, P0 + Toward(Mid) * Rpx);
			}
			return;
		}
		if (M.kind == "corridor" && M.points.size() >= 2)
		{
			for (size_t I = 0; I + 1 < M.points.size(); ++I)
			{
				const FVector2D A = S.ToScreen(M.points[I].x, M.points[I].y);
				const FVector2D B = S.ToScreen(M.points[I + 1].x, M.points[I + 1].y);
				const FVector2D U = (B - A).GetSafeNormal();
				const FVector2D Nn = FVector2D(-U.Y, U.X) * (M.half_width_m / S.MPerPx);
				Dashed(A + Nn, B + Nn, Colour, 1.4f, 10.0f, 6.0f);
				Dashed(A - Nn, B - Nn, Colour, 1.4f, 10.0f, 6.0f);
				NearSegment(M, A + Nn, B + Nn);
				NearSegment(M, A - Nn, B - Nn);
			}
			const FVector2D Mid = (S.ToScreen(M.points[0].x, M.points[0].y) + S.ToScreen(M.points.back().x, M.points.back().y)) * 0.5;
			// Its label where it crosses the sheet: the middle of the view's part of it.
			FVector2D A = S.ToScreen(M.points[0].x, M.points[0].y);
			FVector2D B = S.ToScreen(M.points.back().x, M.points.back().y);
			Label(M, Clip(A, B) ? (A + B) * 0.5 : Mid, Colour);
			return;
		}
		if (M.kind == "duty_zone")
		{
			Circle(P0, M.half_width_m / S.MPerPx, Colour, 1.4f, true, Seed);
			Label(M, P0, Colour);
			Near(M, P0);
			return;
		}
		if (M.kind == "forward_edge" && M.points.size() >= 2)
		{
			for (size_t I = 0; I + 1 < M.points.size(); ++I)
			{
				const FVector2D A = S.ToScreen(M.points[I].x, M.points[I].y);
				const FVector2D B = S.ToScreen(M.points[I + 1].x, M.points[I + 1].y);
				Pencil(A, B, Colour, 2.6f, Seed + static_cast<uint32>(I));
				const FVector2D U = (B - A).GetSafeNormal();
				Line((A + B) * 0.5, (A + B) * 0.5 + FVector2D(-U.Y, U.X) * 6.0, Colour, 1.6f);
				NearSegment(M, A, B);
			}
			Label(M, S.ToScreen(M.points[M.points.size() / 2].x, M.points[M.points.size() / 2].y), Colour);
			return;
		}
		// A place: its sign.
		if (M.kind == "main")
		{
			Circle(P0, 7.0, Colour, Th, false, Seed);
			DrawRect(Colour, static_cast<float>(P0.X) - 2.0f, static_cast<float>(P0.Y) - 2.0f, 4.0f, 4.0f);
			if (!bDecision && S.Map.leeway_m > 0.0) Circle(P0, S.Map.leeway_m / S.MPerPx, FLinearColor(Colour.R, Colour.G, Colour.B, 0.45f), 1.0f, true, Seed + 11u);
		}
		else if (M.kind == "reserve")
		{
			Circle(P0, 6.0, Colour, Th, true, Seed);
		}
		else if (M.kind == "dummy")
		{
			Circle(P0, 6.0, Colour, 1.2f, false, Seed);
			Pencil(P0 + FVector2D(-5.0, 5.0), P0 + FVector2D(5.0, -5.0), Colour, 1.2f, Seed + 2u);
		}
		else if (M.kind == "covered")
		{
			Pencil(P0 + FVector2D(-8.0, -8.0), P0 + FVector2D(8.0, -8.0), Colour, Th, Seed);
			Pencil(P0 + FVector2D(8.0, -8.0), P0 + FVector2D(8.0, 8.0), Colour, Th, Seed + 1u);
			Pencil(P0 + FVector2D(8.0, 8.0), P0 + FVector2D(-8.0, 8.0), Colour, Th, Seed + 2u);
			Pencil(P0 + FVector2D(-8.0, 8.0), P0 + FVector2D(-8.0, -8.0), Colour, Th, Seed + 3u);
		}
		else if (M.kind == "sam_unit")
		{
			Circle(P0, 6.0, Colour, Th, false, Seed);
			Pencil(P0 + FVector2D(0.0, 6.0), P0 + FVector2D(0.0, -12.0), Colour, Th, Seed + 1u);
		}
		else if (M.kind == "axis")
		{
			Pencil(P0 + FVector2D(-8.0, 0.0), P0 + FVector2D(8.0, 0.0), Colour, 2.4f, Seed);
			Pencil(P0 + FVector2D(0.0, -8.0), P0 + FVector2D(0.0, 8.0), Colour, 2.4f, Seed + 1u);
		}
		else
		{
			DrawRect(Colour, static_cast<float>(P0.X) - 3.0f, static_cast<float>(P0.Y) - 3.0f, 6.0f, 6.0f);
		}
		if (P0.X > Box.Min.X && P0.X < Box.Max.X && P0.Y > Box.Min.Y && P0.Y < Box.Max.Y) Label(M, P0, Colour);
		Near(M, P0);
	};
	// The draft, while it is not given, stands for his decision on the sheet.
	HeldMap Draft;
	if (bDecide && S.bDraftDirty)
	{
		MarkSource Own;
		Own.source = "your decision";
		Own.means = "own decision";
		Own.as_of = "not yet given";
		simcore::Vec3 OrderMain;
		const bool bOrderMain = Sim->GetTaskPlace(HmF(R->Task().main), OrderMain);
		AddDecisionMarks(Draft, S.Draft, bOrderMain ? &OrderMain : nullptr, Own, Values);
	}
	uint32 Seed = 1;
	for (const HeldMark& M : S.Map.marks)
	{
		++Seed;
		if (M.kind == "note") continue;
		if (bDecide && S.bDraftDirty && M.origin == MarkOrigin::Decision) continue;
		if (bDecide && S.bDraftDirty && M.kind == "sector") continue;  // the draft's sectors replace the order's
		DrawMark(M, Seed * 977u);
	}
	for (const HeldMark& M : Draft.marks)
	{
		++Seed;
		DrawMark(M, Seed * 977u);
	}
	if (S.Tool == 5 && bMouse && S.Draft.has_main)
	{
		const FVector2D C = S.ToScreen(S.Draft.main.x, S.Draft.main.y);
		Pencil(C, C + Toward(S.SectorFrom) * (Values.sector_drawn_m / S.MPerPx), CRed, 2.0f, 5u);
		Dashed(C, Mouse, CRed, 1.0f, 5.0f, 4.0f);
	}

	// ---- the margin: the sheet's number, its scale and contours, whose map it is, the stamp
	DrawLine(SheetX0, SheetY0, SheetX1, SheetY0, CInk, 2.0f);
	DrawLine(SheetX1, SheetY0, SheetX1, SheetY1, CInk, 2.0f);
	DrawLine(SheetX1, SheetY1, SheetX0, SheetY1, CInk, 2.0f);
	DrawLine(SheetX0, SheetY1, SheetX0, SheetY0, CInk, 2.0f);
	const FString SheetName = Ground != nullptr ? HmF(Ground->Def().name) : FString(TEXT("Kesmark"));
	DrawText(FString::Printf(TEXT("L-34-87-В   %s"), *SheetName.ToUpper()), CInk, SheetX0, 8.0f, Medium);
	DrawText(TEXT("1:50 000   contours every 5 m   grid 1 km"), CInk, SheetX0, 30.0f, Small);
	const FString Whose = FString::Printf(TEXT("The map of %s   (now %s)"), S.Map.holder_label.empty() ? *S.Holder : *HmF(S.Map.holder_label), *HmF(S.Map.now));
	float Tw = 0.0f;
	float Th = 0.0f;
	GetTextSize(Whose, Tw, Th, Small);
	DrawText(Whose, CInk, (SheetX0 + SheetX1 - Tw) * 0.5f, 30.0f, Small);
	{
		const FString Stamp = TEXT("СЕКРЕТНО");
		const FString Copy = TEXT("Экз. № 1");
		GetTextSize(Stamp, Tw, Th, Medium);
		const float Sx = SheetX1 - Tw - 24.0f;
		const float Sy = 6.0f;
		DrawLine(Sx - 6.0f, Sy, Sx + Tw + 6.0f, Sy, CStamp, 2.0f);
		DrawLine(Sx + Tw + 6.0f, Sy, Sx + Tw + 6.0f, Sy + 42.0f, CStamp, 2.0f);
		DrawLine(Sx + Tw + 6.0f, Sy + 42.0f, Sx - 6.0f, Sy + 42.0f, CStamp, 2.0f);
		DrawLine(Sx - 6.0f, Sy + 42.0f, Sx - 6.0f, Sy, CStamp, 2.0f);
		DrawText(Stamp, CStamp, Sx, Sy + 3.0f, Medium);
		DrawText(Copy, CStamp, Sx, Sy + 24.0f, Small);
	}

	// ---- the legend, the situation's hours and the reports in the margin
	const float Lx = SheetX1 + 22.0f;
	float Ly = SheetY0;
	const float Lw = Cw - Lx - 10.0f;
	DrawText(TEXT("LEGEND"), CInk, Lx, Ly, Medium);
	Ly += 22.0f;
	auto Key = [&](const FString& Name, TFunctionRef<void(float, float)> Sign)
	{
		Sign(Lx + 14.0f, Ly + 7.0f);
		DrawText(Name, CInk, Lx + 34.0f, Ly, Small);
		Ly += 16.0f;
	};
	auto Swatch = [this](const FLinearColor& C) { return [this, C](float X, float Y) { DrawRect(C, X - 10.0f, Y - 5.0f, 20.0f, 10.0f); }; };
	auto Stroke = [this](const FLinearColor& C, float W) { return [this, C, W](float X, float Y) { DrawLine(X - 11.0f, Y, X + 11.0f, Y, C, W); }; };
	Key(TEXT("ours (red pencil)"), Stroke(CRed, 2.2f));
	Key(TEXT("the enemy (blue pencil)"), Stroke(CBlue, 2.2f));
	Key(TEXT("main position; leeway ring"), [this](float X, float Y) { DrawRect(CRed, X - 2.0f, Y - 2.0f, 4.0f, 4.0f); DrawLine(X - 7.0f, Y - 7.0f, X + 7.0f, Y - 7.0f, CRed, 1.5f); DrawLine(X - 7.0f, Y + 7.0f, X + 7.0f, Y + 7.0f, CRed, 1.5f); });
	Key(TEXT("reserve / dummy position"), [this](float X, float Y) { DrawLine(X - 7.0f, Y, X - 2.0f, Y, CRed, 1.5f); DrawLine(X + 2.0f, Y, X + 7.0f, Y, CRed, 1.5f); });
	Key(TEXT("covered object"), [this](float X, float Y) { DrawLine(X - 7.0f, Y - 6.0f, X + 7.0f, Y - 6.0f, CRed, 1.5f); DrawLine(X - 7.0f, Y + 6.0f, X + 7.0f, Y + 6.0f, CRed, 1.5f); });
	Key(TEXT("a launcher's sector"), [this](float X, float Y) { DrawLine(X - 10.0f, Y + 6.0f, X, Y - 6.0f, CRed, 1.5f); DrawLine(X + 10.0f, Y + 6.0f, X, Y - 6.0f, CRed, 1.5f); });
	Key(TEXT("attack, helicopter, jammer line"), Stroke(CBlue, 2.4f));
	Key(TEXT("our aviation's corridor, zone"), [this](float X, float Y) { DrawLine(X - 11.0f, Y - 4.0f, X - 2.0f, Y - 4.0f, CRed, 1.2f); DrawLine(X + 2.0f, Y + 4.0f, X + 11.0f, Y + 4.0f, CRed, 1.2f); });
	Key(TEXT("forward edge (as reported)"), Stroke(CRed, 3.0f));
	Key(TEXT("woods"), Swatch(CWood));
	Key(TEXT("water"), Swatch(CWater));
	Key(TEXT("main road / road / track"), [this](float X, float Y) { DrawLine(X - 11.0f, Y - 4.0f, X + 11.0f, Y - 4.0f, CMainRoad, 3.0f); DrawLine(X - 11.0f, Y + 3.0f, X + 11.0f, Y + 3.0f, CRoad, 2.5f); });
	Key(TEXT("built-up area"), Swatch(CTown));
	Key(TEXT("contour (every fifth heavier)"), Stroke(CContour, 1.4f));
	Ly += 8.0f;
	// The situation's hour: the newest copied mark's and the oldest's.
	const HeldMark* Newest = nullptr;
	const HeldMark* Oldest = nullptr;
	for (const HeldMark& M : S.Map.marks)
	{
		// Dated copies only: the plan from before the war has no hour (a fact from before the session's start has a negative one).
		if (M.origin != MarkOrigin::Copied || M.as_of.empty() || M.means == "the plan") continue;
		if (Newest == nullptr || M.as_of_h > Newest->as_of_h) Newest = &M;
		if (Oldest == nullptr || M.as_of_h < Oldest->as_of_h) Oldest = &M;
	}
	DrawText(TEXT("THE SITUATION"), CInk, Lx, Ly, Small);
	Ly += 16.0f;
	if (Newest == nullptr)
	{
		DrawText(TEXT("nothing has reached you"), CInk, Lx, Ly, Small);
		Ly += 16.0f;
	}
	else
	{
		DrawText(FString::Printf(TEXT("as of %s (the oldest %s)"), *HmF(Newest->as_of), *HmF(Oldest->as_of)), CInk, Lx, Ly, Small);
		Ly += 16.0f;
	}
	int32 Notes = 0;
	for (const HeldMark& M : S.Map.marks)
	{
		if (M.kind != "note" || Notes >= 8 || Ly > Ch - 160.0f) continue;
		++Notes;
		for (const FString& L : Wrap(FString::Printf(TEXT("%s (%s, %s)"), *HmF(M.label), *HmF(M.as_of), *HmWords(M.means)), Lw, Small))
		{
			DrawText(L, M.side == MarkSide::Ours ? CRed : CBlue, Lx, Ly, Small);
			Ly += 14.0f;
		}
	}
	// What the draft breaks of the order: listed as it is drawn (never refused).
	if (bDecide)
	{
		Ly += 8.0f;
		DrawText(TEXT("YOUR DECISION"), CInk, Lx, Ly, Small);
		Ly += 16.0f;
		const std::vector<DecisionBreach> Breaches =
			CheckDecision(R->Task(), S.Draft, [Sim](const std::string& Id, simcore::Vec3& Out) { return Sim->GetTaskPlace(HmF(Id), Out); }, Values);
		if (Breaches.empty())
		{
			DrawText(TEXT("within the order"), CInk, Lx, Ly, Small);
			Ly += 16.0f;
		}
		for (const DecisionBreach& B : Breaches)
		{
			for (const FString& L : Wrap(HmF(B.text), Lw, Small))
			{
				DrawText(L, CStamp, Lx, Ly, Small);
				Ly += 14.0f;
			}
		}
		if (S.Launcher >= 0 && static_cast<size_t>(S.Launcher) < S.Launchers.size())
		{
			DrawText(FString::Printf(TEXT("launcher picked: %s"), *HmF(S.Launchers[static_cast<size_t>(S.Launcher)])), CInk, Lx, Ly, Small);
			Ly += 16.0f;
		}
	}

	// ---- a mark's source on hover
	if (Hover.Mark != nullptr)
	{
		const HeldMark& M = *Hover.Mark;
		TArray<FString> Lines;
		Lines.Add(HmF(M.label));
		Lines.Add(FString::Printf(TEXT("%s, %s"), M.side == MarkSide::Ours ? TEXT("ours") : TEXT("the enemy's"),
			M.origin == MarkOrigin::Decision ? TEXT("your own decision") : TEXT("copied from what you were told")));
		Lines.Add(TEXT("told by: ") + HmF(M.source));
		Lines.Add(TEXT("by: ") + HmWords(M.means));
		Lines.Add(TEXT("as of: ") + HmF(M.as_of) + (M.stale ? TEXT(" (stale)") : TEXT("")));
		Lines.Add(TEXT("how sure: ") + HmF(MarkCertaintyName(M.certainty)));
		if (!M.points.empty()) Lines.Add(TEXT("square ") + HmF(GridSquare(M.points[0], Values)));
		float W = 0.0f;
		for (const FString& L : Lines)
		{
			GetTextSize(L, Tw, Th, Small);
			W = FMath::Max(W, Tw);
		}
		const float Bx = FMath::Min(Mx + 16.0f, Cw - W - 20.0f);
		const float By = FMath::Min(My + 16.0f, Ch - 16.0f * Lines.Num() - 16.0f);
		DrawRect(FLinearColor(0.99f, 0.98f, 0.94f, 0.97f), Bx, By, W + 16.0f, 16.0f * Lines.Num() + 8.0f);
		DrawLine(Bx, By, Bx + W + 16.0f, By, CInk, 1.0f);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			DrawText(Lines[I], I == 0 ? (M.side == MarkSide::Ours ? CRed : CBlue) : CInk, Bx + 8.0f, By + 4.0f + 16.0f * I, Small);
		}
	}

	// ---- the keys, and the last word
	FString Keys = TEXT("M / Esc close   wheel zoom   drag move   hover a mark: who told you");
	if (bDecide)
	{
		Keys += TEXT("   |   1 main  2 reserve  3 dummy (then click)   S next launcher, click its sector's edges   X clear   Enter: the combat order");
	}
	DrawText(Keys, CInk, SheetX0, Ch - 40.0f, Small);
	if (Now < S.NoteUntil && !S.Note.IsEmpty())
	{
		DrawText(S.Note, CStamp, SheetX0, Ch - 22.0f, Small);
	}
}
