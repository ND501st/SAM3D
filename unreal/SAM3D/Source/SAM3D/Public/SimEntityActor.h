#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimEntityActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USimPu12MastComponent;

class USimWorldSubsystem;

/**
 * Visual stand-in for one simcore entity (aircraft, missile, ground vehicle).
 * Each tick it reads its state from USimWorldSubsystem's one snapshot a frame and draws it at render time: carried on by
 * its velocity between sim steps (docs/fx/README.md 4.3). A finished missile hides where it was; a destroyed aircraft
 * follows its wreck's fall (USimFxSubsystem::GetWreckPose), darkened, and hides when it reaches the ground; a mover or a
 * visitor that comes back into sight is shown again. Subclass it in Blueprint to swap the placeholder cone for a real
 * model, add trails, explosions, sounds, etc.
 */
UCLASS(Blueprintable)
class SAM3D_API ASimEntityActor : public AActor
{
	GENERATED_BODY()

public:
	ASimEntityActor();

	virtual void Tick(float DeltaSeconds) override;

	/** simcore entity id this actor mirrors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D")
	int32 EntityId = -1;

	/** Hide the actor when the entity is destroyed / the missile's flight ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D")
	bool bHideWhenInactive = true;

	/** Swap the placeholder cone for a box (a battery vehicle or a dummy position). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D")
	void UseGroundStandIn();

	/** An Osa-shaped stand-in (hull, turret, the folded search antenna as boxes, the own vehicle's proportions): a
	 *  battery launcher, or the mock-up at a dummy position. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D")
	void UseOsaStandIn();

	/** A battery vehicle's model by its kind (reload: the TZM 9T217, fuel: the ATZ-3.8-131, command_post: the PU-12M;
	 *  /Game/SAM3D/Vehicles, built by tools/blender/build_support_vehicles.py). False if there is none: keep the box. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D")
	bool UseVehicleModel(const FString& Kind);

	/** Draw it with a basic model (data/scenery/basic_models.json: docs/scenery/), built to size, in place of the
	 *  placeholder. False if the model isn't in the library: keep what it has. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D")
	bool UseBasicModel(const FString& ModelId);

	/** The basic model it is drawn with ("" none). */
	UFUNCTION(BlueprintPure, Category = "SAM3D")
	FString GetBasicModel() const { return BasicModel; }

	/** The PU-12M's mast, guys and AB-1 (a command_post's model only; null otherwise). It follows the vehicle's deployment. */
	UFUNCTION(BlueprintPure, Category = "SAM3D")
	USimPu12MastComponent* GetMast() const { return Mast; }

	/** Called once when the entity stops being alive (shot down, missile detonated...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "SAM3D")
	void OnEntityInactive();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<USceneComponent> Root;

	/** Placeholder mesh (engine cone, rotated so its tip points along +X / the velocity). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SAM3D")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	/** Where a ground vehicle is drawn: on the level's ground at its sim position (roadmap R5). On a map's level the map's
	 *  drawn ground (USimWorldSubsystem::GroundAt, every frame, no trace); on a Cesium level a ground-only trace every
	 *  0.5 s, or when it has moved more than a metre. */
	FVector GroundBelow(const FVector& At, const USimWorldSubsystem& Sim);
	/** A destroyed aircraft: its wreck's pose while it falls (shown, darkened), hidden once it is down or with no wreck. */
	void FollowWreck(const USimWorldSubsystem& Sim);
	/** Every part in one dark colour, once (a burnt wreck). */
	void DarkenAsWreck();

	bool bReportedInactive = false;
	/** Sim time it stopped being alive (for the wreck's wait). */
	double InactiveSinceSimSeconds = 0.0;
	/** The frame it stopped being alive (the wreck's first frames). */
	uint64 InactiveSinceFrame = 0;
	bool bWreckSeen = false;
	bool bDarkened = false;

	/** The ground trace: when and from where it was last made, and the ground's offset from there. */
	double LastGroundTraceSeconds = -1.0;
	FVector LastGroundTraceFrom = FVector::ZeroVector;
	FVector GroundOffset = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoxMesh;

	UPROPERTY()
	TObjectPtr<USimPu12MastComponent> Mast;

	/** The basic model's parts (UseBasicModel). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ModelParts;

	FString BasicModel;
};
