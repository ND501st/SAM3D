#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimCoreIncludes.h"
#include "SimCabinDisplayComponent.generated.h"

class UCanvas;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class USimWorldSubsystem;

/**
 * One instrument of the cabin (simcore::CabinDisplay): the search radar's scope, the tracking radar's range
 * indicator and elevation-search indicator, the TV sight's monitor, a needle dial or meter, or the command post's IT-45
 * (with the plotting glass over it) and status boards. Draws into its own render target every frame
 * from the simulation's state and shows it on a quad on the panel. The TV films the 3D world with a scene
 * capture looking along the tracking radar's wheels.
 *
 * Created and placed by USimCabinComponent; presentation only.
 */
UCLASS()
class SAM3D_API USimCabinDisplayComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USimCabinDisplayComponent();

	/**
	 * Build the screen for layout display LayoutIndex. Centre and the panel axes (out of the face, right, up the
	 * face) are in the parent's space (cm).
	 */
	void Setup(const USimWorldSubsystem* Sim, int32 LayoutIndex, const FVector& Centre, const FVector& Normal,
		const FVector& Right, const FVector& Up, UMaterialInterface* ScreenMaterial, UStaticMesh* Cube,
		UMaterialInterface* ShapeMaterial);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Render-target pixels along the display's longer side. */
	static constexpr int32 PixelsPerLongSide = 512;

private:
	USimWorldSubsystem* GetSim() const;
	void DrawPpi(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display, simcore::ScenarioRunner& Runner, bool bOn);
	void DrawRangeScope(UCanvas* Canvas, const FVector2D& Size, simcore::ScenarioRunner& Runner, bool bOn);
	void DrawElevationScope(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display,
		simcore::ScenarioRunner& Runner, bool bOn);
	void DrawDial(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display, const simcore::ConsoleData& Console, bool bOn, bool bLit);
	void DrawMeter(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display, const simcore::ConsoleData& Console, bool bOn);
	void DrawCounters(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display, simcore::ScenarioRunner& Runner, bool bOn);
	void DrawAspdScope(UCanvas* Canvas, const FVector2D& Size, const simcore::CabinDisplay& Display, const simcore::PostFeed& Post, bool bOn);
	void DrawStatusBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn);
	void UpdateTv(const USimWorldSubsystem* Sim, simcore::ScenarioRunner& Runner, bool bOn);

	int32 LayoutIndex = INDEX_NONE;
	simcore::CabinDisplayKind Kind = simcore::CabinDisplayKind::Dial;
	// What the scopes draw from this frame: the engagement model where the sim runs, the knowledge stream on a client.
	simcore::ScopeFeed Feed;
	TUniquePtr<simcore::SearchScope> Scope;
	TUniquePtr<simcore::ElevationScope> ElevScope;
	int32 ElevRaster = INDEX_NONE;
	double LastSimTimeSeconds = -1.0;
	bool bTvWasOn = false;
	FRandomStream Noise;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Screen;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	/** Bezel behind the screen, and the TV's crosshair. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
