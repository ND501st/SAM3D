#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "SimFxSubsystem.generated.h"

class AActor;
class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class FSimFxWorld;
class FSimFxShots;
class FSimAirShots;

/**
 * The weapons' effects, drawn (docs/fx/README.md 4.1, 5): what the simulation's fx::Feed says happened (our missiles'
 * launches and ends, the neighbours' missiles and guns, bombs, rockets and gun runs, aircraft going down, ARMs, cruise
 * missiles) and every aircraft's contrails and afterburner, as CPU particles drawn by a few instanced quads, ribbons in one
 * procedural mesh, a pool of shadowless lights, scorch decals and the munitions' meshes. All made once, on a transient
 * host actor; every budget a sam3d.fx.* cvar; "stat SAM3DFx" shows the cost.
 *
 * Presentation only: it reads the feed with its own cursor and the cached entity snapshot, runs on the simulation's
 * clock (frozen while paused) and feeds nothing back. Detail follows each effect's projected size on the largest view
 * (the camera or the TV sight's capture, SetAuxView); outside every view nothing is spawned.
 */
UCLASS()
class SAM3D_API USimFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem / FTickableGameObject
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** True while this aircraft entity is a falling wreck (and after: bOutVisible false once it has reached the ground,
	 *  where the impact explosion and its smoke take its place). Location in Unreal cm, at the render time. */
	bool GetWreckPose(int32 EntityId, FVector& OutLocation, FRotator& OutRotation, bool& bOutVisible) const;

	/** A second view the effects must look right in (the TV sight's scene capture), set each frame it is seen. Field of
	 *  view horizontal in degrees, the picture's width in pixels. A view not set for a moment counts no more. */
	void SetAuxView(FName Key, const FVector& Location, const FRotator& Rotation, float FovDeg, float WidthPx, bool bActive);

	/** For tests and the photographer: what was drawn last frame. */
	int32 GetSpriteCount() const;
	int32 GetRibbonCount() const;
	/** The effect lights lit last frame (the pool's, the brightest few). */
	int32 GetLightCount() const;
	bool IsNight() const;
	/** The game thread's time in the effects' last frame (the feed, the particles and the drawing), ms. */
	float GetLastFrameMs() const { return LastFrameMs; }

	/** Every effect in the air or on the ground gone at once, the scorches with them (a new scenario, a restart, the
	 *  photographer between its subjects). */
	void ClearEffects();

	/** The effects' state (Private/SimFxImpl.h), for the photographer and the automation tests; null before Initialize. */
	FSimFxWorld* GetFxWorld() const { return Fx.Get(); }

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void MakeBackEnd();
	void DrawSprites();
	void DrawRibbons();
	void DrawLights();
	void DrawDecals();
	/** Grow the scorch decals' pool to Count (made once each, hidden until used). */
	void AddDecals(int32 Count);
	void DrawMunitions();
	void UpdateViews();
	void UpdateSun();
	void HideAll();
	void UploadLayer(int32 Layer);
	/** A trail's points near the camera seen along it, as smoke puffs (from RibbonAtScratch and RibbonPuffScratch). */
	void AddTrailPuffs(const struct FSimFxRibbon& R);

	TPimplPtr<FSimFxWorld> Fx;
	TPimplPtr<FSimFxShots> Shots;
	TPimplPtr<FSimAirShots> AirShots;  // -SimAirShots (roadmap B13)

	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Layers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LayerMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> RibbonMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RibbonMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> LightPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> DecalPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MunitionPool;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ScorchMaterial;

	/** Each pooled decal's own material (its opacity fades it), and when it was laid (-1: unused). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DecalMaterials;
	TArray<double> DecalBornT;
	TArray<float> DecalOpacity;

	/** The instances each layer holds (its high-water mark), how many were shown last frame, and how many frames it has
	 * been empty (hidden only after a while: each show or hide rebuilds its render state, a spike frame). */
	TArray<int32> LayerCapacity;
	TArray<int32> LayerShown;
	TArray<int32> LayerEmptyFrames;
	TArray<FTransform> XfScratch;
	TArray<const struct FSimFxSprite*> ItemScratch;
	TArray<float> DataScratch;
	TArray<int32> OrderScratch;
	TArray<float> DistScratch;
	/** The batch custom-data call (its end inclusive): 0 untried, 2 works, 3 failed (one instance at a time). */
	int32 CustomDataMode = 0;
	/** The engine plane's texture axes in its own frame (found from its vertices when they can be read). */
	FVector PlaneU = FVector(0.0, 1.0, 0.0);
	FVector PlaneV = FVector(-1.0, 0.0, 0.0);
	TArray<FVector> RibbonVerts;
	TArray<int32> RibbonTris;
	TArray<FVector2D> RibbonUvs;
	TArray<FVector2D> RibbonUvs1;   // (erosion, hairline share): M_FX_Ribbon's contract (tools/ue_make_fx_assets.py)
	TArray<FVector2D> RibbonUvs2;   // the frame's lights on it R, G (T9)
	TArray<FVector2D> RibbonUvs3;   // and B
	TArray<FVector> RibbonAtScratch;  // a ribbon's points where they are now
	TArray<FVector4f> RibbonPuffScratch;  // and per point: true width cm, the opacity its puffs take, the view's sine, age
	TArray<FLinearColor> RibbonColours;
	TArray<FVector> RibbonNormals;
	/** The ribbon material's brightness: the vertex colours (8 bits, 0..1) are the lit colour over it. */
	float RibbonScale = 1.0f;
	float LastFrameMs = 0.0f;
	int32 RibbonOrderCount = 0;
	int32 RibbonLowFrames = 0;  // frames running the trails used under half their section (SimFxLogic::RibbonSectionQuads)
	int32 NextDecal = 0;
	double SunTimer = 0.0;
	bool bMissingAssetsLogged = false;
	bool bBackEnd = false;
	bool bHidden = false;
	bool bSunLogged = false;
	/** The colour explosion flipbook was imported (else its sprites draw from the generated fireball). */
	bool bColourFlipbook = false;
};
