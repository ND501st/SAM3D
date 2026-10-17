#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimPositionWork.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A prepared position's works at one vehicle's place (roadmap 7.6, simcore ScenarioRunner::Works): the emplacement, an
 * earth parapet round the vehicle's footprint with the back left open to drive out by (the vehicles stood in emplacements
 * about 2 m deep: korovin_tiv_2010), and a camouflage net sagging over it on four poles, taken down when its vehicle
 * leaves. Built from the engine's cube and a procedural net; the sizes are SAM3D's.
 */
UCLASS()
class SAM3D_API ASimPositionWork : public AActor
{
	GENERATED_BODY()

public:
	ASimPositionWork();

	/** Build the works for a vehicle of `LengthM` x `WidthM` (the Osa's hull, 9.1 x 2.8 m). */
	void Build(bool bEmplacement, bool bNets, double LengthM = 9.1, double WidthM = 2.8);

	/** Show or hide the net (it comes down when the vehicle leaves). */
	void SetNets(bool bOn);
	bool HasNets() const { return bNetsShown; }

	/** Index of the work in the simulation's list. */
	int32 WorkIndex = INDEX_NONE;

protected:
	UPROPERTY(VisibleAnywhere, Category = "SAM3D")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Earth;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Poles;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Net;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Surface;

private:
	UStaticMeshComponent* AddBox(const FVector& SizeM, const FVector& CentreM, const FRotator& Rotation, const FLinearColor& Color,
		float Roughness, TArray<TObjectPtr<UStaticMeshComponent>>& Into);
	bool bNetsShown = false;
};
