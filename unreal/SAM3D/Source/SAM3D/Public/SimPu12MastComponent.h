#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SimPu12MastComponent.generated.h"

class UStaticMeshComponent;

/**
 * The PU-12M's telescopic mast, its guys and the AB-1 generator set on the ground (docs/PU12_DEVELOPMENT.md P5).
 *
 * Where they stand comes from art/pu12m/pu12m_rig.json (written by tools/blender/pu12m_model.py from the traced model,
 * tools/blender/pu12m_trace.py). The mast's five moving sections stand for an extension from 0 (stowed, about 1.5 m above
 * the roof) to 1 (raised, 16 m above the ground); the guys show once it is up; the AB-1 once it is set out. The owner sets
 * the extension: the player's own post from the console's mast_up subsystem (USimWorldSubsystem::GetPostMastExtension),
 * a battery's post from its deployment (it raises and lowers at the mast's own pace).
 */
UCLASS(ClassGroup = (SAM3D), meta = (BlueprintSpawnableComponent))
class SAM3D_API USimPu12MastComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USimPu12MastComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Stand the mast at this extension at once (0 stowed, 1 raised): the simulation's own progress. */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void SetExtension(float NewExtension);

	/** Move the mast toward this extension at its own pace (RaiseSeconds for the whole way up, LowerSeconds down). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void SetTargetExtension(float NewTarget);

	/** The AB-1 set on the ground behind the vehicle (true) or stowed (false). */
	UFUNCTION(BlueprintCallable, Category = "SAM3D|PU12M")
	void SetGeneratorOut(bool bOut);

	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	float GetExtension() const { return Extension; }

	/** The Z (cm, the vehicle's frame, from the ground) of a section's top: 0 is the fixed base, 1.. the moving ones. */
	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	float SectionTopZ(int32 Section) const;

	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	int32 NumSections() const { return Sections.Num(); }

	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	bool AreGuysShown() const;

	/** The rig file was read (else the built-in copy of the trace's numbers stands). */
	UFUNCTION(BlueprintPure, Category = "SAM3D|PU12M")
	bool IsRigLoaded() const { return bRigLoaded; }

	/** Seconds to raise the mast all the way (data/procedures/pu12m.json mast_up start_time_s: an estimate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|PU12M")
	float RaiseSeconds = 420.f;

	/** Seconds to lower it (mast_up stop_time_s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|PU12M")
	float LowerSeconds = 300.f;

protected:
	virtual void OnRegister() override;

private:
	struct FSection
	{
		float Radius = 0.f;
		float Length = 0.f;
		FString Mesh;
	};

	void LoadRig();
	void Build();
	void Place();

	float MastX = 156.4f;
	float MastY = 69.f;
	float BaseTopZ = 325.5f;
	float StowedTopZ = 355.5f;
	float RaisedTopZ = 1600.f;
	float NestStep = 5.f;
	TArray<FSection> Sections;
	TArray<float> GuyLevels;
	float GuyRadius = 600.f;
	FString GuyMesh = TEXT("SM_PU12M_Guy");
	float Ab1X = -499.f;
	float Ab1Y = 90.f;
	FString Ab1MeshName = TEXT("SM_PU12M_AB1");

	float Extension = 0.f;
	float Target = 0.f;
	bool bMoving = false;
	bool bGeneratorOut = false;
	bool bRigLoaded = false;
	bool bBuilt = false;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SectionMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> GuyMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ab1Mesh;
};
