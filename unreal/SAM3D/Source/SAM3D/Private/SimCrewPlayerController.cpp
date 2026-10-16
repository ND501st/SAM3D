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
				UE_LOG(LogSAM3D, Display, TEXT("SIMCREW SMOKE server: t %.1f s, driver '%s', commander '%s', senior_operator '%s', pwr.gta %d"),
					Sim->GetSimTimeSeconds(), *Sim->GetSeatHolder(TEXT("driver")), *Sim->GetSeatHolder(TEXT("commander")),
					*Sim->GetSeatHolder(TEXT("senior_operator")), Sim->GetControlPosition(TEXT("pwr.gta")));
			}
			FGenericPlatformMisc::RequestExit(false);
		}), Smoke, false);
	}
}
