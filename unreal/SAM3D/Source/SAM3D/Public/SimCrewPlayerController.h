#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "SimCrewPlayerController.generated.h"

class USimCrewSeatComponent;
class USimKnowledgeComponent;

/**
 * A player of the multiplayer crew: carries a crew seat (USimCrewSeatComponent) and the player's knowledge stream
 * (USimKnowledgeComponent). The scenario itself is loaded on every machine by the map's ASimScenarioDirector; the
 * server simulates, clients mirror.
 *
 * Command-line helpers (for scripted checks and dedicated-server sessions):
 *   -SimSeat=<station>            ask for that seat once connected (e.g. -SimSeat=driver)
 *   -SimCrewTry=<control>:<pos>   move a control of the seat 5 s after joining (e.g. -SimCrewTry=pwr.gta:1)
 *   -SimCrewSmoke=<seconds>       after that long, log one "SIMCREW SMOKE" line with what this machine holds, and quit
 */
UCLASS()
class SAM3D_API ASimCrewPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASimCrewPlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D|Crew")
	TObjectPtr<USimCrewSeatComponent> CrewSeat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D|Crew")
	TObjectPtr<USimKnowledgeComponent> Knowledge;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void LogActionResult(const FString& ControlId, bool bMoved, const FString& Why);

	void TryControl();
	void LogSmoke();

	FTimerHandle SeatTimer;
	FTimerHandle TryTimer;
	FTimerHandle SmokeTimer;
};

/** Game mode for the multiplayer crew: every player gets an ASimCrewPlayerController. Use ?game=SimCrew on the map URL or
 *  set it as the map's game mode. On a dedicated server with -SimCrewSmoke=<s>, logs the seats and quits after that long. */
UCLASS()
class SAM3D_API ASimCrewGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASimCrewGameMode();

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle SmokeTimer;
};
