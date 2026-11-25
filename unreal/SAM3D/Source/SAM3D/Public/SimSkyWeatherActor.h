#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimSkyWeatherActor.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
namespace simcore
{
	struct SunPosition;
}

/**
 * The sky over a map level, from the sim's weather (simcore/weather.hpp: the scenario's "weather" preset) and the sun's
 * place (simcore/solar.hpp) for the scenario's latitude, the day and the hour:
 *
 * - the sun: a directional light at the sun's elevation and azimuth, lighting a physically based sky atmosphere
 *   (Rayleigh and Mie scattering), whose aerosol (Mie) scattering is set from the day's visibility by Koschmieder
 *   (extinction 3.912 / visibility, less the air's own Rayleigh part), so the haze over 40 km reads as it would;
 * - the haze near the ground (roadmap R6): an exponential height fog drawing HeightFogShare of that extinction (the
 *   atmosphere keeps the rest), thinning upward over the boundary layer; no volumetric fog;
 * - the cloud: the engine's volumetric cloud on the weather's main layer (its base, depth and cover), and, if asked,
 *   the sim's own cloud field as low-poly cells: exactly the cloud the TV sight cannot see through, drifting with the
 *   wind at its height. The volumetric cloud is the look; the cells are the truth.
 *
 * The sky reaches past any map: the atmosphere is the planet's, and the cloud covers the whole airspace.
 *
 * One sun (roadmap R7, simcore/light.hpp): with a scenario loaded, the sun is the scenario's own sky clock (its origin,
 * "date", "utc_offset_h" and start hour, which the missions menu's time of day sets) at the sim's time, the same sun the
 * TV, the battery's night and the visual range read. DayOfYear, LocalTimeHours and UtcOffsetHours are only the editor's
 * fallback, for a level played without a scenario.
 */
UCLASS()
class SAM3D_API ASimSkyWeatherActor : public AActor
{
	GENERATED_BODY()

public:
	ASimSkyWeatherActor();

	/** Day of the year (1-366; 288: 15 October). Editor fallback: a loaded scenario's sky clock wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "1", ClampMax = "366"))
	int32 DayOfYear = 288;

	/** Local time at the start, hours; the sky moves on with sim time. Editor fallback: a loaded scenario's sky clock wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "0", ClampMax = "24"))
	float LocalTimeHours = 10.5f;

	/** Local time minus UTC (Central European Time: 1). Editor fallback: a loaded scenario's sky clock wins. */
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

	/** Draw the haze near the ground as an exponential height fog (roadmap R6), its density from the day's visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky")
	bool bHeightFog = true;

	/** Of the haze's extinction at the ground (Koschmieder, from the visibility), the share the height fog draws; the sky
	 *  atmosphere's aerosol keeps the rest, so the two together still give the day's visibility. A game value, to be tuned
	 *  against -SimMapShots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightFogShare = 0.3f;

	/** The height over which the fog's density falls by e, km: the haze's boundary layer, as the atmosphere's aerosol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "0.1", ClampMax = "10"))
	float HeightFogScaleHeightKm = 1.2f;

	/** The outdoor views' exposure compensation, EV (the engine's own is +1, which lifts the plain's dark ground to a
	 *  light sand). Set by an unbound post process, so every view outdoors takes it; the cabin and its displays set
	 *  their own. A game value, tuned against -SimMapShots (track T); -SimExposureBias=<ev> overrides it to tune. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SAM3D|Sky", meta = (ClampMin = "-4", ClampMax = "4"))
	float OutdoorExposureBias = 0.0f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ApplyWeather();
	void UpdateSun();
	void SetSunDirection(const simcore::SunPosition& S);
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

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(VisibleAnywhere, Category = "SAM3D|Sky")
	TObjectPtr<UPostProcessComponent> Exposure;

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
