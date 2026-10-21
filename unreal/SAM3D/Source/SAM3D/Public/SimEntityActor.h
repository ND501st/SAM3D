#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimEntityActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USimPu12MastComponent;

/**
 * Visual stand-in for one simcore entity (aircraft or missile).
 * Each tick it copies its transform from USimWorldSubsystem. Subclass it in Blueprint
 * to swap the placeholder cone for a real model, add trails, explosions, sounds, etc.
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
	bool bReportedInactive = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoxMesh;

	UPROPERTY()
	TObjectPtr<USimPu12MastComponent> Mast;

	/** The basic model's parts (UseBasicModel). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ModelParts;

	FString BasicModel;
};
