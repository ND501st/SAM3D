#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimCoreIncludes.h"
#include "SimCrewBodiesComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USimWorldSubsystem;

/**
 * The crewmates' bodies in the cabin (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): one body of rigid parts per
 * station, posed every frame by simcore's BodyAnimator from what that crewmate is doing (ScenarioRunner::CrewActivity).
 * His hand goes to the control he is about to move and stays a moment, grips the hand wheel he turns; he bends or turns
 * on his seat to reach, stands up for a panel out of seated reach; he looks at what he works or at his instruments and
 * nods while he speaks. Presentation only: the simulation decides what each one does and when.
 *
 * Attach it to the cabin component (the same body frame: x right, y forward, z up in m -> X forward, Y right, Z up in
 * cm). Meshes: /Game/SAM3D/Crew/SM_Crew_* (tools/blender/build_crew_body.py, tools/ue_import_crew_body.py); engine
 * basic shapes stand in for any that isn't imported. Also draws the training overlay's ghost hand on the advised
 * control (docs/crew/08_TRAINING_AND_ASSISTANCE.md).
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimCrewBodiesComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USimCrewBodiesComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The body of this station isn't drawn: the camera is there (the player's own seat, first person). "" for none. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	void SetHiddenStation(const FString& Station) { HiddenStation = Station; }

	/** Bodies within RadiusCm of a point are hidden too (0: none): the player standing at a far panel has a crewmate's
	 *  head where his eyes are. */
	void SetClearAround(const FVector& WorldLocation, float RadiusCm)
	{
		ClearAt = WorldLocation;
		ClearRadiusCm = RadiusCm;
	}

	/** The ghost hand of the training overlay: a hand from Station's seat on Control, pulsing ("" hides it). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	void SetGhostHand(const FString& Station, const FString& Control);

	/** Bodies built (one per station of the cabin with a crew member). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Crew")
	int32 GetNumBodies() const { return Bodies.Num(); }

	/** Where a body's head is (component space, cm) and whether it is drawn; false if there is no such station. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Crew")
	bool GetBodyHead(const FString& Station, FVector& OutLocation, bool& bOutVisible) const;

private:
	struct FPart
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FName MeshName;      // the library mesh it shows now (hands change with their shape)
		bool bFallback = false;
		double RadiusM = 0.05;  // the basic shape's radius when it stands in
	};
	struct FBody
	{
		FString Station;
		TUniquePtr<simcore::BodyAnimator> Animator;
		TArray<FPart> Parts;
		bool bVisible = true;
		/** Up and away from his seat (out of the vehicle, or on his way): drawn on his feet where his body is
		 *  (simcore::WalkingPose), stepping as he walks. */
		bool bAway = false;
		double StridePhase = 0.0;
		simcore::Vec3 LastFeet;
	};
	void PoseAway(FBody& Body, const simcore::MoverView& View, const simcore::BodyDims& Dims);

	bool Build(USimWorldSubsystem* Sim);
	void Clear();
	void PoseBody(FBody& Body);
	void ShowPart(FPart& Part, const simcore::BodyPart& P);
	UStaticMesh* LibraryMesh(const FName& Name);
	FName MeshNameFor(const simcore::BodyPart& P) const;
	void PaintPart(UStaticMeshComponent* Mesh, bool bFallback, const FLinearColor& FallbackColor);
	void UpdateGhost(float DeltaTime);

	TArray<FBody> Bodies;

	/** All part components, kept alive for the garbage collector. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> AllParts;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ghost;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	const simcore::CabinLayout* BuiltLayout = nullptr;
	const simcore::ScenarioRunner* BuiltRunner = nullptr;
	FString HiddenStation;
	FVector ClearAt = FVector::ZeroVector;
	float ClearRadiusCm = 0.0f;
	FString GhostStation;
	FString GhostControl;
	float GhostClock = 0.0f;
	// The ghost hand's reach plan, kept while the advice stays on the same control (planning searches every posture).
	FString GhostPlanKey;
	const simcore::CabinLayout* GhostPlanLayout = nullptr;
	simcore::ReachPlan GhostPlan;
};
