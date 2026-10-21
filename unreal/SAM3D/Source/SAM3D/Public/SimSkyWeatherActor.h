#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimSkyWeatherActor.generated.h"

class UDirectionalLightComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;

/**
 * The sky over a map level, from the sim's weather (simcore/weather.hpp: the scenario's "weather" preset) and the sun's
 * place (simcore/solar.hpp) for the scenario's latitude, the day and the hour:
 *
 * - the sun: a directional light at the sun's elevation and azimuth, lighting a physically based sky atmosphere
 *   (Rayleigh and Mie scattering), whose aerosol (Mie) scattering is set from the day's visibility by Koschmieder
 *   (extinction 3.912 / visibility, less the air's own Rayleigh part), so the haze over 40 km reads as it would;
 * - the cloud: the engine's volumetric cloud on the weather's main layer (its base, depth and cover), and, if asked,
 *   the sim's own cloud field as low-poly cells: exactly the cloud the TV sight cannot see through, drifting with the
 *   wind at its height. The volumetric cloud is the look; the cells are the truth.
 *
 * The sky reaches past any map: the atmosphere is the planet's, and the cloud covers the whole airspace.
 */
UCLASS()
class SAM3D_API ASimSkyWeatherActor : public AActor
{
	GENERATED_BODY()

public:
	ASimSkyWeatherActor();

	/** Day of the year (1-366; 288: 15 October). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "1", ClampMax = "366"))
	int32 DayOfYear = 288;

	/** Local time at the scenario's start, hours. The sky moves on with sim time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "0", ClampMax = "24"))
	float LocalTimeHours = 10.5f;

	/** Local time minus UTC (Central European Time: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky")
	float UtcOffsetHours = 1.0f;

	/** Draw the sim's cloud cells (low-poly): what blocks the TV sight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky")
	bool bDrawCloudCells = false;

	/** Draw the engine's volumetric cloud on the main layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky")
	bool bVolumetricClouds = true;

	/** How far out the cloud cells are drawn, km. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "5", ClampMax = "200"))
	float CloudCellRadiusKm = 60.0f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ApplyWeather();
	void UpdateSun();
	void BuildCloudCells();

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> CloudCells;

	FString AppliedPreset;      // the weather the sky was last set for
	FVector AppliedOrigin = FVector(1e9);
	float Timer = 0.0f;
	float DefaultCoverage = 0.0f;  // the cloud material's own coverage, read once
	bool bHaveDefaultCoverage = false;
};
