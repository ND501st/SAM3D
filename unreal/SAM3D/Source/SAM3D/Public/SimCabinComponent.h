#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimWorldSubsystem.h"
#include "SimCabinComponent.generated.h"

class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USimCabinDisplayComponent;
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

	/** Red dot beside the control marking its working position (simcore::CabinLayout::RedDot). May be null. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mark;

	FLinearColor BaseColor = FLinearColor::Black;
	FLinearColor MovingColor = FLinearColor::Black;

	/** simcore::CabinLookKind from the layout's "look" (Default = by style), and its size (m, 0 = the style's). */
	uint8 LookKind = 0;
	double LookWidthM = 0.0;
	double LookHeightM = 0.0;

	/** Shown with the cabin parts library's meshes (art/cabin_parts) rather than engine basic shapes. */
	bool bParts = false;
};

/**
 * The 3D cabin of the player's vehicle, built from the cabin layout the world subsystem loaded
 * (data/cabins/<system>.json, simcore/cabin.hpp): panels, controls and lamps from engine basic shapes, and the
 * instruments (scopes, TV, dials) as USimCabinDisplayComponent screens.
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

	UFUNCTION(BlueprintPure, Category = "SAM3D|Cabin")
	int32 GetNumDisplays() const;

	/** Where a display is (centre, relative to this component), which station reads it, and its label. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	bool GetDisplayView(int32 Display, int32& OutStation, FVector& OutCentre, FString& OutLabel) const;

	/** The control or lamp a world-space segment hits first; INDEX_NONE if none. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|Cabin")
	int32 TraceItem(const FVector& WorldStart, const FVector& WorldEnd) const;

	/** Puts a cabin part on the cabin's own lighting channel (1), lit by the cabin lamps and never by the sun: the thin
	 * shell doesn't shadow a low sun reliably, and a sunlit patch burns white at the cabin's fixed indoor exposure.
	 * Call before registering the component. */
	static void UseCabinLighting(UPrimitiveComponent* Part);

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
	void BuildBlock(const simcore::CabinLayout& Layout, const simcore::CabinBlock& Block);
	void BuildShell(const simcore::CabinShellPart& Part);
	void BuildFixture(const simcore::CabinLayout& Layout, const simcore::CabinFixture& Fixture);
	/** A static part of the panel decoration (no collision); kept for DestroyParts. */
	UStaticMeshComponent* AddDecor(UStaticMesh* Mesh, const FLinearColor& Color);
	/** A mesh of the cabin parts library (/Game/SAM3D/Cabin/Parts/<Name>), or null if it isn't imported. */
	UStaticMesh* PartMesh(const TCHAR* Name);
	/** A flat quad on the face showing Text (drawn once into a small texture with a font that has Cyrillic). */
	void AddTextPlate(const FVector& Centre, const FVector& Normal, const FVector& Right, const FVector& Up, double WidthM,
		double HeightM, const FString& Text, const FLinearColor& Paper, bool bEngraved = false);
	/** An item's engraved label just above it (an item ItemHeightM tall at Centre); nothing for an empty text. */
	void AddLabel(const FVector& Centre, const FVector& Normal, const FVector& Right, const FVector& Up, double ItemHeightM,
		const FString& Text);
	void DestroyParts();
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FLinearColor& Color);
	void Animate(const USimWorldSubsystem* Sim, float DeltaTime);
	void AnimateControl(const USimWorldSubsystem* Sim, FSimCabinItem& Item);
	/** AnimateControl for an item drawn with parts library meshes; false if its style isn't handled that way. */
	bool AnimatePart(const simcore::ControlPanel& Panel, const simcore::CabinControl& Control, const simcore::ControlDef& Def,
		int32 Position, FSimCabinItem& Item);
	void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color) const;
	USimWorldSubsystem* GetSim() const;

	UPROPERTY(Transient)
	TArray<FSimCabinItem> Items;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PanelMeshes;

	/** Parts library meshes loaded so far, by name (null entries: not imported). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMesh>> PartMeshes;

	/** Blocks, fixtures and nameplate texts: drawn, not operated. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> DecorParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USimCabinDisplayComponent>> Displays;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	/** Engine basic-shape material; its "Color" parameter tints each part. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	/** The cabin's master material (/Game/SAM3D/Cabin/Materials/M_CabinSurface: Color, Roughness, Metallic); null =
	 * engine basic-shape material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Engraved labels: /Game/SAM3D/Cabin/Materials/M_CabinLabel (a texture's red channel masks dark, lit letters). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LabelMaterial;

	/** Engine material that shows a texture ("SlateUI") unlit: the displays' screens. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ScreenMaterial;

	/** The layout the parts were built from; rebuilt when the subsystem loads another. */
	const simcore::CabinLayout* BuiltLayout = nullptr;
	int32 HighlightedItem = INDEX_NONE;
};
