#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimWorldSubsystem.h"
#include "SimCabinComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** One control or lamp built in the cabin, with the meshes that show it. */
USTRUCT()
struct SAM3D_API FSimCabinItem
{
	GENERATED_BODY()

	/** Console control id; empty for a lamp. */
	FString ControlId;

	/** Lamp id; empty for a control. */
	FString LampId;

	/** Index into the layout's controls or lamps. */
	int32 LayoutIndex = INDEX_NONE;

	/** simcore::CabinStyle of a control. */
	uint8 Style = 0;

	/** Seconds left to show a push-button pressed in (the sim releases it after one step). */
	float PressedFor = 0.0f;

	/** Placement in this component's space (cm), and the panel's axes: out of the face, right, up the face. */
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;

	/** Body, moving part (lever, knob, cap, cover, key, rim) and pointer/handle. Any may be null. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Moving;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Pointer;

	FLinearColor BaseColor = FLinearColor::Black;
	FLinearColor MovingColor = FLinearColor::Black;
};

/**
 * The 3D cabin of the player's vehicle, built from the cabin layout the world subsystem loaded
 * (data/cabins/<system>.json, simcore/cabin.hpp): panels, controls and lamps from engine basic shapes.
 *
 * Presentation only. Every control shows the console's state each frame (switch throw, knob angle, cover open,
 * key turned, wheel angle, lamp lit), and player actions go to USimWorldSubsystem::PlayerSetControl /
 * PlayerTurnWheel, which judge them against the player's procedure. Attach it to the vehicle's root: the
 * layout's body frame (x right, y forward, z up, m) maps to this component's X forward, Y right, Z up (cm).
 * It builds itself as soon as a cabin layout is loaded.
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimCabinComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USimCabinComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsBuilt() const { return BuiltLayout != nullptr; }

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	int32 GetNumStations() const;

	/** A seat's eye point and view, relative to this component. False if there is no such station. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool GetStationView(int32 Station, FVector& OutLocation, FRotator& OutRotation, FString& OutLabel) const;

	/** The control or lamp a world-space segment hits first; INDEX_NONE if none. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	int32 TraceItem(const FVector& WorldStart, const FVector& WorldEnd) const;

	/** Label and current state, e.g. "ПОИСК: ВКЛ". */
	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	FString DescribeItem(int32 Item) const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	bool IsWheel(int32 Item) const;

	/** Click a switch, selector, key, cover or button. Direction +1 (left click) or -1 (right click). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	ESimPlayerActionResult ClickItem(int32 Item, int32 Direction);

	/** Turn a hand wheel by a number of turns (negative = anticlockwise). False if it is locked. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool TurnWheel(int32 Item, double Turns);

	/** Tint one item to show what the player is pointing at (INDEX_NONE for none). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	void SetHighlightedItem(int32 Item) { HighlightedItem = Item; }

private:
	bool Build(const USimWorldSubsystem* Sim);
	void BuildControl(const USimWorldSubsystem* Sim, int32 LayoutIndex);
	void BuildLamp(const USimWorldSubsystem* Sim, int32 LayoutIndex);
	void DestroyParts();
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FLinearColor& Color);
	void Animate(const USimWorldSubsystem* Sim, float DeltaTime);
	void AnimateControl(const USimWorldSubsystem* Sim, FSimCabinItem& Item);
	void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color) const;
	USimWorldSubsystem* GetSim() const;

	UPROPERTY(Transient)
	TArray<FSimCabinItem> Items;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PanelMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	/** Engine basic-shape material; its "Color" parameter tints each part. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	/** The layout the parts were built from; rebuilt when the subsystem loads another. */
	const simcore::CabinLayout* BuiltLayout = nullptr;
	int32 HighlightedItem = INDEX_NONE;
};
