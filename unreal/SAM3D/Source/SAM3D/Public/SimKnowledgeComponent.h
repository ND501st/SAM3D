#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCoreIncludes.h"
#include "SimKnowledgeComponent.generated.h"

/** One item of the player's fused picture, in Unreal coordinates. */
USTRUCT(BlueprintType)
struct FSimKnowledgeTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	int32 Id = 0;

	/** 0 unknown, 1 assumed friend, 2 friend, 3 suspect, 4 hostile (simcore::em::Identity). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	uint8 IdentityCode = 0;

	/** 0 unknown, 1 air, 2 ground, 3 emitter (simcore::em::Category). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	uint8 CategoryCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bPositionValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FVector Location = FVector::ZeroVector;

	/** cm/s, Unreal axes. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FVector Velocity = FVector::ZeroVector;

	/** Horizontal one-sigma position error (m). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	float SigmaMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bFromFix = false;

	/** Direction known, range unknown: BearingOrigin and BearingDegrees give the newest line. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bBearingOnly = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FVector BearingOrigin = FVector::ZeroVector;

	/** True azimuth, degrees clockwise from north. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	float BearingDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FString TypeGuess;
};

/** A contact of an ESM receiver cabled to the player's node. */
USTRUCT(BlueprintType)
struct FSimKnowledgeContact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	int32 Key = 0;

	/** True azimuth, degrees clockwise from north. */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	float BearingDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	float SigmaDegrees = 0.f;

	/** What the receiver's own threat library calls the signal ("" unknown). */
	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FString LibraryName;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bHostile = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bSteady = false;
};

/** A track of a radar cabled to the player's node. */
USTRUCT(BlueprintType)
struct FSimKnowledgeSensorTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	int32 Key = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	bool bConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SAM3D|Knowledge")
	float SigmaMeters = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimKnowledgeUpdated, int32, Seq);

/**
 * Multiplayer knowledge (roadmap E10): what one player may see, carried from the server's simulation to that player.
 *
 * Put it on a PlayerController (so it has an owning connection). On the server, USimWorldSubsystem sends this player
 * the view of `NodeId` (0: the Osa crew's own node in a scenario with the EM layer) every `SendPeriodSeconds` of sim
 * time, as a delta against the newest view the player acknowledged (simcore/em/replication.hpp); the bytes travel by an
 * unreliable client RPC and the acknowledgement comes back by an unreliable server RPC. A lost packet is never
 * acknowledged, so the next delta is taken from an older base. The player sees only knowledge, never the truth.
 * Works the same in standalone and on a listen server's own player (the RPCs run locally).
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimKnowledgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimKnowledgeComponent();

	/** The EM node whose knowledge this player gets (server only). 0: the scenario unit's node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Knowledge")
	int32 NodeId = 0;

	/** Sim seconds between packets (server only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Knowledge", meta = (ClampMin = "0.05"))
	float SendPeriodSeconds = 0.2f;

	/** Fired on the owning client each time a newer view is applied. */
	UPROPERTY(BlueprintAssignable, Category = "SAM3D|Knowledge")
	FOnSimKnowledgeUpdated OnKnowledgeUpdated;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	TArray<FSimKnowledgeTrack> GetPictureTracks() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	TArray<FSimKnowledgeContact> GetEsmContacts() const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	TArray<FSimKnowledgeSensorTrack> GetSensorTracks() const;

	/** Number of the newest view held (0: none yet). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	int32 GetKnowledgeSeq() const;

	/** Sim time of the newest view (s). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	double GetKnowledgeTimeSeconds() const;

	/** One line: view number, items, contacts, tracks, bytes received. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Knowledge")
	FString GetKnowledgeSummary() const;

	/** The view as simcore holds it (owning client). */
	const simcore::em::KnowledgeView& GetView() const;

	// Server side, used by USimWorldSubsystem.
	int32 GetReplicatorHandle() const { return ReplicatorHandle; }
	void SetReplicatorHandle(int32 Handle) { ReplicatorHandle = Handle; }
	double& SendAccumulator() { return Accumulator; }
	void SendKnowledge(const TArray<uint8>& Bytes) { ClientReceiveKnowledge(Bytes); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Client, Unreliable)
	void ClientReceiveKnowledge(const TArray<uint8>& Bytes);

	UFUNCTION(Server, Unreliable)
	void ServerAckKnowledge(int32 Seq);

private:
	TUniquePtr<simcore::em::KnowledgeMirror> Mirror;
	int32 ReplicatorHandle = INDEX_NONE;
	double Accumulator = 0.0;
	int64 BytesReceived = 0;
};
