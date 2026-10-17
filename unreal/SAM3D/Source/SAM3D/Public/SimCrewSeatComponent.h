#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCrewSeatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSimSeatActionResult, const FString&, ControlId, bool, bMoved, const FString&, Why);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimSeatChanged, const FString&, SeatRole);

/**
 * Multiplayer crew: one player in one of the vehicle's crew seats (a console station: commander, deputy commander,
 * senior operator, operator, driver).
 *
 * Put it on a PlayerController. The server runs the one simulation (ScenarioRunner); the player in a seat replaces the
 * NPC there, gets that seat's orders as procedures and may work only that station's controls. Every machine keeps the
 * same scenario loaded; a network client never steps it, and its console (switches, wheels, subsystems, inputs) is
 * overwritten from the server's snapshots several times a second, so the cabin it draws is the server's. Control moves
 * go to the server (reliable for switches, unreliable for wheel drags) and come back in the next snapshot; a refused
 * move comes back with the reason. Free seats stay NPC-crewed.
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimCrewSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimCrewSeatComponent();

	/** The seat this player holds ("" none). Set by the server. */
	UPROPERTY(ReplicatedUsing = OnRep_SeatRole, BlueprintReadOnly, Category = "SAM3D|Crew")
	FString SeatRole;

	/** The procedure the player in the seat is being judged against, and its next step (1-based; 0 none). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "SAM3D|Crew")
	FString SeatProcedureId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "SAM3D|Crew")
	int32 SeatProcedureStep = 0;

	/** The name the debrief uses for this player (empty: the PlayerState's name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew")
	FString PlayerName;

	/** Sim seconds between console snapshots to this player (server only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Crew", meta = (ClampMin = "0.02"))
	float ConsoleSendPeriodSeconds = 0.1f;

	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Crew")
	FOnSimSeatActionResult OnActionResult;

	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Crew")
	FOnSimSeatChanged OnSeatChanged;

	/** Ask for a seat (a console station id); "" leaves the seat. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	void RequestSeat(const FString& Role);

	/** Move a switch, selector or key, or press a button, at this player's station. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	void SetControl(const FString& ControlId, int32 Position);

	/** Turn a hand wheel at this player's station. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	void TurnWheel(const FString& ControlId, double Value);

	/** The command post (P7): give launcher Callsign the board's target Number (the commanding seat only), or write the
	 *  IT-45's target Number on the board (the plotter's seat only). The answer comes back by OnActionResult, its
	 *  ControlId "designate" or "plot". */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void Designate(const FString& Callsign, int32 Number);

	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void PlotTrack(int32 Number);

	/** Console snapshots applied on this machine (network clients). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	int32 GetConsoleSnapshotsApplied() const { return SnapshotsApplied; }

	// Server side, used by USimWorldSubsystem.
	double& SendAccumulator() { return Accumulator; }
	uint32 NextSnapshotSeq() { return ++SnapshotSeq; }
	void SendConsole(const TArray<uint8>& Bytes) { ClientReceiveConsole(Bytes); }
	FString DebriefName() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_SeatRole();

	UFUNCTION(Server, Reliable)
	void ServerRequestSeat(const FString& Role);

	UFUNCTION(Server, Reliable)
	void ServerSetControl(const FString& ControlId, int32 Position);

	UFUNCTION(Server, Unreliable)
	void ServerTurnWheel(const FString& ControlId, double Value);

	UFUNCTION(Server, Reliable)
	void ServerDesignate(const FString& Callsign, int32 Number);

	UFUNCTION(Server, Reliable)
	void ServerPlotTrack(int32 Number);

	UFUNCTION(Client, Unreliable)
	void ClientReceiveConsole(const TArray<uint8>& Bytes);

	UFUNCTION(Client, Reliable)
	void ClientActionResult(const FString& ControlId, bool bMoved, const FString& Why);

private:
	class USimWorldSubsystem* Sim() const;

	double Accumulator = 0.0;
	uint32 SnapshotSeq = 0;      // server: the last snapshot number sent
	uint32 AppliedSeq = 0;       // client: the newest snapshot applied
	int32 SnapshotsApplied = 0;
};
