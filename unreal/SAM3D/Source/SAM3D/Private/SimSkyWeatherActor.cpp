#include "SimSkyWeatherActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SAM3D.h"
#include "SimCoreIncludes.h"
#include "SimWorldSubsystem.h"

namespace
{
	// The clear air's own (Rayleigh) extinction at 550 nm, per km: about 0.0116 (the engine's default Rayleigh green
	// channel is 0.0135). What the visibility leaves beyond it is the haze's.
	constexpr double RayleighPerKm = 0.0135;
	// Of the haze's extinction, the share that scatters (the rest is absorbed): the engine's default single-scattering
	// albedo for Mie, 0.003996 / (0.003996 + 0.000444).
	constexpr double MieAlbedo = 0.9;
}

ASimSkyWeatherActor::ASimSkyWeatherActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(10.0f);  // lux in the engine's default exposure; the atmosphere dims and reddens it low in the sky
	Sun->SetDynamicShadowDistanceMovableLight(20000.0f);  // shadows out to 200 m, then distance field / far shadows

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetRealTimeCapture(true);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
}

void ASimSkyWeatherActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTransform(FTransform::Identity);
	if (FParse::Param(FCommandLine::Get(), TEXT("SimCloudCells")))
	{
		bDrawCloudCells = true;  // the sim's own cloud, as boxes: what the TV cannot see through
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
	{
		CloudMaterial = UMaterialInstanceDynamic::Create(M, this);
		Clouds->SetMaterial(CloudMaterial);
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		CloudMaterial->GetAllScalarParameterInfo(Infos, Ids);
		FString Names;
		for (const FMaterialParameterInfo& I : Infos)
		{
			float V = 0.0f;
			CloudMaterial->GetScalarParameterValue(I, V);
			Names += FString::Printf(TEXT(" %s=%g"), *I.Name.ToString(), V);
		}
		UE_LOG(LogSAM3D, Log, TEXT("Sky: the cloud material's scalars:%s"), *Names);
	}
	Timer = 0.0f;
}

void ASimSkyWeatherActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	if (Timer > 0.0f)
	{
		return;
	}
	Timer = 1.0f;
	ApplyWeather();
	UpdateSun();
	// The cells drift with the wind at each layer's height.
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	if (W != nullptr)
	{
		const simcore::Weather& Wx = W->GetWeather();
		for (int32 i = 0; i < CloudCells.Num() && i < static_cast<int32>(Wx.LayerCount()); ++i)
		{
			if (CloudCells[i] != nullptr)
			{
				CloudCells[i]->SetRelativeLocation(USimWorldSubsystem::EnuToUnreal(Wx.LayerDrift(static_cast<std::size_t>(i))));
			}
		}
	}
}

void ASimSkyWeatherActor::ApplyWeather()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	if (W == nullptr)
	{
		return;
	}
	const simcore::Weather& Wx = W->GetWeather();
	const simcore::GeoPoint& O = W->Frame().Origin();
	const FVector Origin(O.LonDeg(), O.LatDeg(), O.alt_m);
	const FString PresetId = UTF8_TO_TCHAR(Wx.Preset().id.c_str());
	if (PresetId == AppliedPreset && Origin.Equals(AppliedOrigin, 1e-9))
	{
		return;
	}
	AppliedPreset = PresetId;
	AppliedOrigin = Origin;

	// Haze: Koschmieder's extinction for the day's visibility, the air's own Rayleigh part taken out.
	const double VisKm = FMath::Max(0.5, Wx.Preset().visibility_km);
	const double MieExt = FMath::Max(0.004 / MieAlbedo, 3.912 / VisKm - RayleighPerKm);
	Atmosphere->SetMieScatteringScale(static_cast<float>(MieExt * MieAlbedo));
	Atmosphere->SetMieAbsorptionScale(static_cast<float>(MieExt * (1.0 - MieAlbedo)));
	Atmosphere->SetMieExponentialDistribution(1.2f);  // km: the haze lives in the boundary layer
	Atmosphere->SetGroundAlbedo(FColor(60, 64, 40));  // the plain's farmland beyond the drawn ground

	// The main cloud layer (the lowest that covers a quarter of the sky or more, else the most covering one).
	int32 Main = -1;
	for (int32 i = 0; i < static_cast<int32>(Wx.LayerCount()); ++i)
	{
		const double Cover = Wx.LayerCover(static_cast<std::size_t>(i));
		if (Main < 0 || (Cover >= 0.25 && Wx.LayerCover(static_cast<std::size_t>(Main)) < 0.25) ||
			(Cover > Wx.LayerCover(static_cast<std::size_t>(Main)) && Wx.LayerCover(static_cast<std::size_t>(Main)) < 0.25))
		{
			Main = i;
		}
	}
	const bool bClouds = bVolumetricClouds && Main >= 0;
	Clouds->SetVisibility(bClouds);
	if (bClouds)
	{
		const simcore::CloudLayerDef& L = Wx.LayerDef(static_cast<std::size_t>(Main));
		Clouds->SetLayerBottomAltitude(static_cast<float>(FMath::Max(0.05, (L.base_m - O.alt_m) / 1000.0)));
		Clouds->SetLayerHeight(static_cast<float>(FMath::Max(0.1, L.thickness_m / 1000.0)));
		if (CloudMaterial != nullptr)
		{
			// The sample material's coverage is a bias on its noise (default -0.2, which reads as broken cloud, about
			// 5 oktas): shift it by the layer's cover against that; a full overcast well past it.
			float Default = 0.0f;
			if (!bHaveDefaultCoverage && CloudMaterial->GetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), Default))
			{
				DefaultCoverage = Default;
				bHaveDefaultCoverage = true;
			}
			const double Cover = Wx.LayerCover(static_cast<std::size_t>(Main));
			if (bHaveDefaultCoverage)
			{
				const float Value = static_cast<float>(Cover >= 0.999 ? DefaultCoverage + 1.0 : DefaultCoverage + 1.2 * (Cover - 0.625));
				CloudMaterial->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), Value);
				UE_LOG(LogSAM3D, Log, TEXT("Sky: cloud coverage %.3f (the material's default %.3f, the layer's cover %.0f oktas)"), Value,
					DefaultCoverage, Cover * 8.0);
			}
		}
	}
	UE_LOG(LogSAM3D, Log, TEXT("Sky: weather '%s', visibility %.0f km (Mie %.4f/km), %d cloud layer(s)%s"), *PresetId, VisKm,
		MieExt * MieAlbedo, static_cast<int32>(Wx.LayerCount()),
		bClouds ? *FString::Printf(TEXT(", volumetric on '%s'"), UTF8_TO_TCHAR(Wx.LayerDef(static_cast<std::size_t>(Main)).id.c_str())) : TEXT(""));
	BuildCloudCells();
}

void ASimSkyWeatherActor::UpdateSun()
{
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	const double LatDeg = W != nullptr ? W->Frame().Origin().LatDeg() : 46.9;
	const double LonDeg = W != nullptr ? W->Frame().Origin().LonDeg() : 19.7;
	const double SimHours = Sim != nullptr ? Sim->GetSimTimeSeconds() / 3600.0 : 0.0;
	double Utc = LocalTimeHours - UtcOffsetHours + SimHours;
	int32 Day = DayOfYear;
	while (Utc >= 24.0)
	{
		Utc -= 24.0;
		Day = Day % 365 + 1;
	}
	while (Utc < 0.0)
	{
		Utc += 24.0;
		Day = Day > 1 ? Day - 1 : 365;
	}
	const simcore::SunPosition S = simcore::SunAt(LatDeg, LonDeg, Day, Utc);
	// The direction to the sun in Unreal (X north, Y east, Z up); the light shines the other way.
	const double Ce = FMath::Cos(S.elevation_rad);
	const FVector ToSun(Ce * FMath::Cos(S.azimuth_rad), Ce * FMath::Sin(S.azimuth_rad), FMath::Sin(S.elevation_rad));
	Sun->SetWorldRotation((-ToSun).Rotation());
}

void ASimSkyWeatherActor::BuildCloudCells()
{
	for (UHierarchicalInstancedStaticMeshComponent* C : CloudCells)
	{
		if (C != nullptr)
		{
			C->DestroyComponent();
		}
	}
	CloudCells.Reset();
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	if (!bDrawCloudCells || W == nullptr)
	{
		return;
	}
	const simcore::Weather& Wx = W->GetWeather();
	const simcore::LocalFrame& Frame = W->Frame();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	const double R = CloudCellRadiusKm * 1000.0;
	for (std::size_t i = 0; i < Wx.LayerCount(); ++i)
	{
		const simcore::CloudLayerDef& L = Wx.LayerDef(i);
		const double Step = FMath::Max(400.0, 0.5 * L.cell_m);
		TArray<FTransform> Cells;
		for (const simcore::CloudCell& C : Wx.Cells(i, -R, -R, R, R, Step))
		{
			if (C.east_m * C.east_m + C.north_m * C.north_m > R * R)
			{
				continue;
			}
			// The cell's middle at its height above sea level, in the scenario's frame (below the plane far out).
			simcore::GeoPoint G = Frame.EnuToGeodetic({C.east_m, C.north_m, 0.0});
			G.alt_m = 0.5 * (C.base_m + C.top_m);
			const FVector At = USimWorldSubsystem::EnuToUnreal(Frame.GeodeticToEnu(G));
			Cells.Add(FTransform(FRotator::ZeroRotator, At, FVector(Step, Step, C.top_m - C.base_m)));  // the cube is 1 m
		}
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetupAttachment(Root);
		H->SetStaticMesh(Cube);
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.86f, 0.88f));
		H->SetMaterial(0, Mid);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->RegisterComponent();
		H->AddInstances(Cells, false, true);
		H->SetRelativeLocation(USimWorldSubsystem::EnuToUnreal(Wx.LayerDrift(i)));
		CloudCells.Add(H);
		UE_LOG(LogSAM3D, Log, TEXT("Sky: %d cloud cells of '%s' drawn"), Cells.Num(), UTF8_TO_TCHAR(L.id.c_str()));
	}
}
