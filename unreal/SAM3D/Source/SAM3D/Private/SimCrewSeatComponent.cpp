#include "SimCrewSeatComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "SimWorldSubsystem.h"

USimCrewSeatComponent::USimCrewSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USimCrewSeatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(USimCrewSeatComponent, SeatRole, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(USimCrewSeatComponent, SeatProcedureId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(USimCrewSeatComponent, SeatProcedureStep, COND_OwnerOnly);
}

USimWorldSubsystem* USimCrewSeatComponent::Sim() const
{
	return GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
}

void USimCrewSeatComponent::BeginPlay()
{
	Super::BeginPlay();
	USimWorldSubsystem* S = Sim();
	if (S == nullptr)
	{
		return;
	}
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		S->RegisterCrewSeat(this);
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC != nullptr && PC->IsLocalController())
	{
		S->SetLocalCrewSeat(this);
	}
}

void USimCrewSeatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USimWorldSubsystem* S = Sim())
	{
		if (GetOwner() != nullptr && GetOwner()->HasAuthority())
		{
			if (!SeatRole.IsEmpty())
			{
				S->LeaveCrewSeat(SeatRole);
			}
			S->UnregisterCrewSeat(this);
		}
		S->ClearLocalCrewSeat(this);
	}
	Super::EndPlay(EndPlayReason);
}

FString USimCrewSeatComponent::DebriefName() const
{
	if (!PlayerName.IsEmpty())
	{
		return PlayerName;
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC != nullptr && PC->PlayerState != nullptr && !PC->PlayerState->GetPlayerName().IsEmpty())
	{
		return PC->PlayerState->GetPlayerName();
	}
	return TEXT("Player");
}

void USimCrewSeatComponent::RequestSeat(const FString& Role)
{
	ServerRequestSeat(Role);
}

void USimCrewSeatComponent::SetControl(const FString& ControlId, int32 Position)
{
	ServerSetControl(ControlId, Position);
}

void USimCrewSeatComponent::TurnWheel(const FString& ControlId, double Value)
{
	ServerTurnWheel(ControlId, Value);
}

void USimCrewSeatComponent::Designate(const FString& Callsign, int32 Number)
{
	ServerDesignate(Callsign, Number);
}

void USimCrewSeatComponent::PlotTrack(int32 Number)
{
	ServerPlotTrack(Number);
}

void USimCrewSeatComponent::ServerDesignate_Implementation(const FString& Callsign, int32 Number)
{
	FString Why = TEXT("no seat");
	const bool bOk = !SeatRole.IsEmpty() && Sim() != nullptr && Sim()->SeatDesignate(SeatRole, Callsign, Number, Why);
	ClientActionResult(TEXT("designate"), bOk, Why);
}

void USimCrewSeatComponent::ServerPlotTrack_Implementation(int32 Number)
{
	FString Why = TEXT("no seat");
	const bool bOk = !SeatRole.IsEmpty() && Sim() != nullptr && Sim()->SeatPlot(SeatRole, Number, Why);
	ClientActionResult(TEXT("plot"), bOk, Why);
}

void USimCrewSeatComponent::ServerRequestSeat_Implementation(const FString& Role)
{
	USimWorldSubsystem* S = Sim();
	if (S == nullptr)
	{
		return;
	}
	if (!SeatRole.IsEmpty())
	{
		S->LeaveCrewSeat(SeatRole);
		SeatRole.Reset();
	}
	if (!Role.IsEmpty() && S->TakeCrewSeat(Role, DebriefName()))
	{
		SeatRole = Role;
	}
	OnRep_SeatRole();  // the server's own copy (a listen server's player)
}

void USimCrewSeatComponent::ServerSetControl_Implementation(const FString& ControlId, int32 Position)
{
	FString Why;
	const bool bMoved = !SeatRole.IsEmpty() && Sim() != nullptr && Sim()->SeatSetControl(SeatRole, ControlId, Position, Why);
	if (SeatRole.IsEmpty())
	{
		Why = TEXT("no seat");
	}
	ClientActionResult(ControlId, bMoved, Why);
}

void USimCrewSeatComponent::ServerTurnWheel_Implementation(const FString& ControlId, double Value)
{
	if (!SeatRole.IsEmpty() && Sim() != nullptr)
	{
		FString Why;
		Sim()->SeatTurnWheel(SeatRole, ControlId, Value, Why);
	}
}

void USimCrewSeatComponent::ClientReceiveConsole_Implementation(const TArray<uint8>& Bytes)
{
	USimWorldSubsystem* S = Sim();
	if (S == nullptr || GetWorld()->GetNetMode() != NM_Client)
	{
		return;  // the server's own player already looks at the real console
	}
	if (S->ApplyConsoleSnapshot(Bytes, AppliedSeq))
	{
		++SnapshotsApplied;
	}
}

void USimCrewSeatComponent::ClientActionResult_Implementation(const FString& ControlId, bool bMoved, const FString& Why)
{
	OnActionResult.Broadcast(ControlId, bMoved, Why);
}

void USimCrewSeatComponent::OnRep_SeatRole()
{
	OnSeatChanged.Broadcast(SeatRole);
}
