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
 * (with its grid glass), the plotter's map, the ДХ keypad's window and the IT-1M / IT-2M boards. Draws into its own
 * render target every frame from the simulation's state and shows it on a quad on the panel. The TV films the 3D world
 * with a scene capture looking along the tracking radar's wheels.
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
	void DrawStatusBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn, float DeltaTime);
	void DrawPlotBoard(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn);
	void DrawKeypadWindow(UCanvas* Canvas, const FVector2D& Size, const simcore::PostFeed& Post, bool bOn);
	void UpdateTv(const USimWorldSubsystem* Sim, simcore::ScenarioRunner& Runner, bool bOn);

	int32 LayoutIndex = INDEX_NONE;
	simcore::CabinDisplayKind Kind = simcore::CabinDisplayKind::Dial;
	// What the scopes draw from this frame: the engagement model where the sim runs, the knowledge stream on a client.
	simcore::ScopeFeed Feed;
	TUniquePtr<simcore::SearchScope> Scope;
	TUniquePtr<simcore::ElevationScope> ElevScope;
	int32 ElevRaster = INDEX_NONE;
	double LastSimTimeSeconds = -1.0;
	/** Redrawing costs the render thread hundreds of canvas lines: only while the camera is in the cabin, at most
	 *  sam3d.CabinDisplayHz times a second (the TV's picture sam3d.TvHz); the frame time since the last drawing. */
	float RedrawSecondsLeft = 0.0f;
	float DeltaSinceDraw = 0.0f;
	/** The owner's camera is in its cabin (or the owner isn't a vehicle): the instruments are seen. */
	bool IsSeen() const;
	/** For a picture that only changes with a few values (a dial, a meter, a tube switched off): those values hashed into
	 *  Key. False for a moving picture (a lit scope, the counters, the post's boards), which is redrawn at the set rate. */
	bool ContentKey(const simcore::CabinDisplay& Display, const simcore::ConsoleData& Console, bool bOn, uint32& Key) const;
	uint32 DrawnKey = 0;
	bool bDrawnKeyValid = false;
	/** The search scope's fixed face (the azimuth scale and its numbers), drawn once and stamped under each picture. */
	void DrawPpiFace(const FVector2D& Size);
	bool bTvWasOn = false;
	FRandomStream Noise;
	// The IT-45's sweep (rad clockwise from north) and the turns it has made (its speckle is redrawn each turn).
	double AspdSweepRad = 0.0;
	int32 AspdTurns = 0;
	// The status board's lamps, each easing on and off (a row of nine per launcher).
	TArray<float> BoardGlow;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Screen;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> FaceTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	/** Bezel behind the screen, and the TV's crosshair. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
