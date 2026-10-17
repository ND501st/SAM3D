#include "SimCrewPlayerController.h"

#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SAM3D.h"
#include "SimCrewSeatComponent.h"
#include "SimKnowledgeComponent.h"
#include "SimWorldSubsystem.h"
#include "TimerManager.h"

ASimCrewPlayerController::ASimCrewPlayerController()
{
	CrewSeat = CreateDefaultSubobject<USimCrewSeatComponent>(TEXT("CrewSeat"));
	Knowledge = CreateDefaultSubobject<USimKnowledgeComponent>(TEXT("Knowledge"));
}

void ASimCrewPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	FString Seat;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimSeat="), Seat) && !Seat.IsEmpty())
	{
		// A moment after joining, so the scenario is loaded on the server.
		GetWorldTimerManager().SetTimer(SeatTimer, FTimerDelegate::CreateWeakLambda(this, [this, Seat]() { CrewSeat->RequestSeat(Seat); }), 2.0f, false);
	}
	FString Try;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCrewTry="), Try) && Try.Contains(TEXT(":")))
	{
		CrewSeat->OnActionResult.AddDynamic(this, &ASimCrewPlayerController::LogActionResult);
		GetWorldTimerManager().SetTimer(TryTimer, this, &ASimCrewPlayerController::TryControl, 5.0f, false);
	}
	float Smoke = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCrewSmoke="), Smoke) && Smoke > 0.f && GetNetMode() == NM_Client)
	{
		GetWorldTimerManager().SetTimer(SmokeTimer, this, &ASimCrewPlayerController::LogSmoke, Smoke, false);
	}
	// The command post (P7): -SimCrewPlot=<seconds> writes the IT-45's first target on the board then (the plotter's
	// seat); -SimCrewDesignate=<seconds>:<launcher index> gives the board's first target to that launcher (the
	// commander's).
	float PlotAt = 0.f;
	FString Designate;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCrewPlot="), PlotAt) && PlotAt > 0.f)
	{
		GetWorldTimerManager().SetTimer(PlotTimer, this, &ASimCrewPlayerController::PlotFirstTrack, PlotAt, false);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCrewDesignate="), Designate) && Designate.Contains(TEXT(":")))
	{
		FString At, Index;
		Designate.Split(TEXT(":"), &At, &Index);
		GetWorldTimerManager().SetTimer(DesignateTimer, this, &ASimCrewPlayerController::DesignateFirstTarget,
			FMath::Max(1.f, FCString::Atof(*At)), false);
	}
	if (PlotAt > 0.f || !Designate.IsEmpty())
	{
		CrewSeat->OnActionResult.AddUniqueDynamic(this, &ASimCrewPlayerController::LogActionResult);
	}
}

void ASimCrewPlayerController::PlotFirstTrack()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const TArray<int32> Tube = Sim != nullptr ? Sim->GetPostTrackNumbers() : TArray<int32>();
	const bool bSent = Tube.Num() > 0 && Sim->PlotPostTrack(Tube[0]);
	UE_LOG(LogSAM3D, Display, TEXT("SIMCREW PLOT: IT-45 shows %d targets; %s"), Tube.Num(),
		bSent ? *FString::Printf(TEXT("plotting %02d"), Tube[0]) : TEXT("nothing sent"));
}

void ASimCrewPlayerController::DesignateFirstTarget()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	FString Designate, At, Index;
	FParse::Value(FCommandLine::Get(), TEXT("SimCrewDesignate="), Designate);
	Designate.Split(TEXT(":"), &At, &Index);
	const TArray<int32> Board = Sim != nullptr ? Sim->GetPostBoardNumbers() : TArray<int32>();
	const TArray<FSimLauncherStatus> Launchers = Sim != nullptr ? Sim->GetLauncherStatus() : TArray<FSimLauncherStatus>();
	const int32 L = FCString::Atoi(*Index);
	const bool bSent = Board.Num() > 0 && Launchers.IsValidIndex(L) && Sim->DesignateBatteryTarget(Launchers[L].Callsign, Board[0]);
	UE_LOG(LogSAM3D, Display, TEXT("SIMCREW DESIGNATE: board %d targets, %d launchers; %s"), Board.Num(), Launchers.Num(),
		bSent ? *FString::Printf(TEXT("%d to %s"), Board[0], *Launchers[L].Callsign) : TEXT("nothing sent"));
}

void ASimCrewPlayerController::TryControl()
{
	FString Try;
	FParse::Value(FCommandLine::Get(), TEXT("SimCrewTry="), Try);
	FString Control, Pos;
	if (Try.Split(TEXT(":"), &Control, &Pos))
	{
		USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
		if (Sim != nullptr)
		{
			const ESimPlayerActionResult R = Sim->PlayerSetControl(Control, FCString::Atoi(*Pos));
			UE_LOG(LogSAM3D, Display, TEXT("SIMCREW TRY: %s -> %d (result %s)"), *Control, FCString::Atoi(*Pos),
				*StaticEnum<ESimPlayerActionResult>()->GetNameStringByValue(static_cast<int64>(R)));
		}
	}
}

void ASimCrewPlayerController::LogActionResult(const FString& ControlId, bool bMoved, const FString& Why)
{
	UE_LOG(LogSAM3D, Display, TEXT("SIMCREW RESULT: %s %s%s%s"), *ControlId, bMoved ? TEXT("moved") : TEXT("refused"),
		Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why);
}

void ASimCrewPlayerController::LogSmoke()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	int32 Gta = -1;
	if (Sim != nullptr)
	{
		Gta = Sim->GetControlPosition(TEXT("pwr.gta"));
	}
	const simcore::em::ViewScope& Sc = Knowledge->GetView().scope;
	UE_LOG(LogSAM3D, Display, TEXT("SIMCREW SMOKE client: seat '%s', procedure '%s' step %d, console snapshots applied %d, pwr.gta %d, mirror-only %d; knowledge: %s; scope: valid %d, %d search echoes, %d track echoes, antenna el %.1f deg"),
		*CrewSeat->SeatRole, *CrewSeat->SeatProcedureId, CrewSeat->SeatProcedureStep, CrewSeat->GetConsoleSnapshotsApplied(), Gta,
		Sim != nullptr && Sim->IsMirrorOnly() ? 1 : 0, *Knowledge->GetKnowledgeSummary(), Sc.valid ? 1 : 0,
		static_cast<int32>(Sc.search.size()), static_cast<int32>(Sc.track_returns.size()), Sc.antenna_el_deg);
	int32 Ground = 0;
	if (Sim != nullptr)
	{
		for (const FSimEntityState& E : Sim->GetEntityStates())
		{
			Ground += E.bIsGround ? 1 : 0;
		}
		const TArray<FSimRadioMessage> Radio = Sim->GetRadioMessages(3);
		UE_LOG(LogSAM3D, Display, TEXT("SIMCREW BATTERY client: %d board marks, %d ground entities, last radio: %s"),
			Sim->GetBoardMarks().Num(), Ground, Radio.Num() > 0 ? *(Radio.Last().From + TEXT(": ") + Radio.Last().Text) : TEXT("-"));
		if (Sim->IsCommandPost())
		{
			// The post's own picture from the stream: the IT-45, the board, the IT boards (post_feed.hpp).
			FString Boards;
			for (const FSimLauncherStatus& L : Sim->GetLauncherStatus())
			{
				Boards += FString::Printf(TEXT(" %s:%s(%d)"), *L.Callsign, *L.State, L.Missiles);
			}
			UE_LOG(LogSAM3D, Display, TEXT("SIMCREW POST client: IT-45 %d targets, board %d targets, mast %.2f, IT boards%s"),
				Sim->GetPostTrackNumbers().Num(), Sim->GetPostBoardNumbers().Num(), Sim->GetPostMastExtension(), *Boards);
		}
	}
	FGenericPlatformMisc::RequestExit(false);
}

ASimCrewGameMode::ASimCrewGameMode()
{
	PlayerControllerClass = ASimCrewPlayerController::StaticClass();
}

void ASimCrewGameMode::BeginPlay()
{
	Super::BeginPlay();
	float Smoke = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCrewSmoke="), Smoke) && Smoke > 0.f && GetNetMode() == NM_DedicatedServer)
	{
		GetWorldTimerManager().SetTimer(SmokeTimer, FTimerDelegate::CreateWeakLambda(this, [this]() {
			USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
			if (Sim != nullptr)
			{
				FString Seats;
				if (const simcore::ScenarioRunner* R = Sim->GetScenarioRunner())
				{
					for (const simcore::ConsoleStation& St : R->Console().stations)
					{
						const FString Id = UTF8_TO_TCHAR(St.id.c_str());
						Seats += FString::Printf(TEXT(" %s '%s',"), *Id, *Sim->GetSeatHolder(Id));
					}
				}
				UE_LOG(LogSAM3D, Display, TEXT("SIMCREW SMOKE server: t %.1f s, seats:%s pwr.gta %d, board %d targets"),
					Sim->GetSimTimeSeconds(), *Seats, Sim->GetControlPosition(TEXT("pwr.gta")), Sim->GetPostBoardNumbers().Num());
			}
			FGenericPlatformMisc::RequestExit(false);
		}), Smoke, false);
	}
}
