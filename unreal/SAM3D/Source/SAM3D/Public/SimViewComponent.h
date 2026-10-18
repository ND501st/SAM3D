#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SimViewComponent.generated.h"

class APlayerController;
class ASimOsaVehicle;
class UCameraComponent;
class USimWorldSubsystem;

/** The camera views, on the F keys as in DCS (docs/PLAYING.md). */
UENUM(BlueprintType)
enum class ESimView : uint8
{
	/** F1: the cabin, at a crew seat (again: the next seat). */
	Cabin,
	/** F2: round your own vehicle (again: closer or further). */
	Vehicle,
	/** F3: a fly-by of what the camera follows: it waits ahead and watches it go past. */
	FlyBy,
	/** F4: the chase, behind what the camera follows (again: from ahead). */
	Chase,
	/** F5: the nearest aircraft, seen from your vehicle, the lens closed on it (again: the next nearest). */
	NearestAir,
	/** F6: your missile in flight (again: the next). */
	Weapon,
	/** F7: the battery's vehicles and the other ground units (again: the next). */
	Ground,
	/** F8: the aircraft (again: the next). */
	Air,
	/** F10: the map, from above (wheel: height, right drag: pan). */
	Map,
	/** F11: a free camera (W A S D Q E, right mouse looks, Shift faster). */
	Free
};

/** The camera the external views look through (ASimOsaVehicle's cabin camera is F1's). */
UCLASS(NotPlaceable, Transient)
class SAM3D_API ASimViewCamera : public AActor
{
	GENERATED_BODY()

public:
	ASimViewCamera();

	UCameraComponent* GetCamera() const { return Camera; }

private:
	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Views")
	TObjectPtr<UCameraComponent> Camera;
};

/**
 * The player's camera views (F1-F11, docs/PLAYING.md) and the time compression keys (Ctrl+Z faster, Alt+Z slower, Shift+Z
 * normal). Lives on the player's ASimOsaVehicle, which calls UpdateViews each frame.
 *
 * Views are presentation only: they move a camera, never tell the crew anything (the first-person rule, docs/command/).
 * The labels name the view and what it follows, and carry no numbers the crew would not have. The console variable
 * sam3d.Views limits them: 2 every view (default), 1 our own side (the cabin, our vehicles, our missiles, the map),
 * 0 the cabin and our own vehicle only.
 *
 * Scripted check: -SimViewShots=<folder> steps through the views (after -SimShotDelay= seconds, default 15) and
 * photographs each with the HUD (<n>_<key>.png), then quits.
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimViewComponent();

	/** Each frame, from the vehicle's Tick: the F keys, the mouse, the camera's place, the time keys. */
	void UpdateViews(APlayerController* Pc, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "SAM3D|Views")
	void SetView(ESimView NewView);

	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	ESimView GetView() const { return View; }

	/** "F1" .. "F11". */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	FString GetViewKey() const { return KeyOf(View); }

	/** The view and what it follows, e.g. "F4  Chase: Practice drone 1". */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	FString GetViewLabel() const;

	/** A short note from the last key (e.g. "No missile in flight"), "" once it has been shown long enough. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	FString GetViewMessage() const { return MessageSeconds > 0.0f ? Message : FString(); }

	/** The free camera has W A S D: the vehicle doesn't drive while it is on. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Views")
	bool IsFreeCamera() const { return View == ESimView::Free; }

	static FString KeyOf(ESimView InView);
	static FString NameOf(ESimView InView);

	/** Degrees of camera turn per unit of mouse movement (right mouse held). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Views")
	double OrbitDegPerMouseUnit = 2.5;

	/** The free camera's speed (m/s); Shift five times faster, the wheel changes it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Views")
	double FreeSpeedMps = 60.0;

private:
	/** What a view follows: our own vehicle, or a sim entity. */
	struct FSubject
	{
		bool bOwn = true;
		int32 EntityId = INDEX_NONE;
		bool operator==(const FSubject& O) const { return bOwn == O.bOwn && (bOwn || EntityId == O.EntityId); }
	};
	struct FSubjectState
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FString Name;
		bool bAlive = false;
		/** Roughly how big it is (m), for how far the camera stands off. */
		double SizeM = 10.0;
	};

	ASimOsaVehicle* Vehicle() const;
	USimWorldSubsystem* Sim() const;
	bool StateOf(const FSubject& S, FSubjectState& Out) const;
	/** The subjects a view cycles through, in order (the nearest first for F5). */
	TArray<FSubject> CandidatesFor(ESimView InView) const;
	bool IsAllowed(ESimView InView) const;
	/** A view key pressed: switch to the view, or on to the next of its kind if already in it. */
	void Press(ESimView InView);
	void Note(const FString& Text);
	void EnsureCamera();
	void PlaceCamera(APlayerController* Pc, float DeltaSeconds);
	void HandleTimeKeys(APlayerController* Pc);
	void ReportToTutorial();
	/** -SimViewShots: the next view of the scripted check, its picture, the end. */
	void RunViewShots(float DeltaSeconds);
	/** Camera at Location looking at Target, with this lens. */
	void Aim(const FVector& Location, const FVector& Target, float FovDeg);

	UPROPERTY(Transient)
	TObjectPtr<ASimViewCamera> ViewCamera;

	ESimView View = ESimView::Vehicle;
	FSubject Subject;
	/** The subject was lost (shot down, detonated): the camera stays where it last saw it. */
	bool bSubjectGone = false;
	FVector LastSubjectLocation = FVector::ZeroVector;

	double OrbitYawDeg = 150.0;
	double OrbitPitchDeg = -22.0;
	/** Distance as a multiple of the subject's size (the wheel). */
	double OrbitScale = 3.0;
	bool bChaseAhead = false;
	bool bFlyByPlaced = false;
	FVector FlyByPoint = FVector::ZeroVector;
	/** F5's lens: how much of the view the aircraft fills (the wheel). */
	double PadlockFill = 1.0;
	double MapHeightM = 1500.0;
	FVector MapPan = FVector::ZeroVector;
	FVector FreeLocation = FVector::ZeroVector;
	FRotator FreeRotation = FRotator::ZeroRotator;
	bool bFreePlaced = false;
	/** The chase views' camera offset from what they follow, smoothed. */
	FVector Smoothed = FVector::ZeroVector;
	bool bSnap = true;

	FString ShotFolder;
	int32 ShotStep = INDEX_NONE;
	float ShotClock = 0.0f;
	bool bShotsAsked = false;

	FString Message;
	float MessageSeconds = 0.0f;
	FString LastReportedView;
};
