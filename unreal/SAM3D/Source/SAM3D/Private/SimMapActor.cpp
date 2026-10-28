#include "SimMapActor.h"

#include "Camera/CameraActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Engine/GameInstance.h"
#include "SAM3D.h"
#include "SimAirCampaignSubsystem.h"
#include "SimBasicModels.h"
#include "SimCoreIncludes.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"

namespace
{
	FVector ToUnreal(const simcore::Vec3& Enu) { return USimWorldSubsystem::EnuToUnreal(Enu); }

	// An ENU direction as an Unreal direction (X north, Y east, Z up; no scale).
	FVector DirToUnreal(const simcore::Vec3& D) { return FVector(D.y, D.x, D.z); }

	// The colour each kind of box is drawn in: flat, a little muted, so the shapes read and nothing else.
	FLinearColor PartColour(simcore::MapPart P)
	{
		using simcore::MapPart;
		switch (P)
		{
		case MapPart::House: return FLinearColor(0.62f, 0.52f, 0.38f);
		case MapPart::Flats: return FLinearColor(0.52f, 0.52f, 0.50f);
		case MapPart::Industrial: return FLinearColor(0.42f, 0.40f, 0.37f);
		case MapPart::Farm: return FLinearColor(0.55f, 0.45f, 0.32f);
		case MapPart::Military: return FLinearColor(0.27f, 0.31f, 0.21f);
		case MapPart::Hangar: return FLinearColor(0.36f, 0.38f, 0.35f);
		case MapPart::Shelter: return FLinearColor(0.35f, 0.37f, 0.31f);
		case MapPart::Tower: return FLinearColor(0.75f, 0.72f, 0.64f);
		case MapPart::Station: return FLinearColor(0.55f, 0.36f, 0.27f);
		case MapPart::Tank: return FLinearColor(0.62f, 0.62f, 0.58f);
		case MapPart::Chimney: return FLinearColor(0.45f, 0.25f, 0.20f);
		case MapPart::CoolingTower: return FLinearColor(0.60f, 0.60f, 0.57f);
		case MapPart::Bridge: return FLinearColor(0.42f, 0.42f, 0.42f);
		case MapPart::Canopy: return FLinearColor(0.09f, 0.17f, 0.06f);
		case MapPart::Marsh: return FLinearColor(0.20f, 0.26f, 0.15f);
		case MapPart::Revetment: return FLinearColor(0.33f, 0.27f, 0.18f);
		case MapPart::Pad: return FLinearColor(0.36f, 0.33f, 0.27f);
		case MapPart::Runway: return FLinearColor(0.38f, 0.38f, 0.37f);
		case MapPart::Mast: return FLinearColor(0.20f, 0.20f, 0.20f);
		case MapPart::Tent: return FLinearColor(0.27f, 0.31f, 0.20f);
		case MapPart::Bunker: return FLinearColor(0.28f, 0.30f, 0.22f);
		case MapPart::Launcher: return FLinearColor(0.24f, 0.29f, 0.18f);
		case MapPart::Count: break;
		}
		return FLinearColor::Gray;
	}

	FLinearColor SurfaceColour(simcore::MapSurface S)
	{
		using simcore::MapSurface;
		switch (S)
		{
		case MapSurface::Water: return FLinearColor(0.05f, 0.11f, 0.15f);
		case MapSurface::MainRoad: return FLinearColor(0.16f, 0.16f, 0.16f);
		case MapSurface::Road: return FLinearColor(0.22f, 0.22f, 0.21f);
		case MapSurface::Track: return FLinearColor(0.33f, 0.27f, 0.18f);
		case MapSurface::Rail: return FLinearColor(0.20f, 0.17f, 0.14f);
		}
		return FLinearColor::Gray;
	}

	// Ground colours: farmland in big patches (fields, a few tints), greener low by the river, browner up the hills.
	FLinearColor GroundColour(const simcore::MapDef& Def, double E, double N, double H)
	{
		static const FLinearColor Fields[] = {
			FLinearColor(0.20f, 0.26f, 0.10f), FLinearColor(0.26f, 0.28f, 0.12f), FLinearColor(0.30f, 0.27f, 0.15f),
			FLinearColor(0.17f, 0.23f, 0.09f), FLinearColor(0.33f, 0.30f, 0.19f)};
		const uint64 Seed = Def.seed ^ 0xF1E1Dull;
		const double U = simcore::noise::Value(Seed, E, N, 450.0);
		FLinearColor C = Fields[FMath::Clamp(static_cast<int32>(U * 5.0), 0, 4)];
		const double Rise = FMath::Clamp((H - Def.base_m) / 60.0, -0.3, 1.0);
		C = FMath::Lerp(C, FLinearColor(0.30f, 0.27f, 0.18f), static_cast<float>(FMath::Max(0.0, Rise) * 0.4));
		if (Rise < 0.0)
		{
			C = FMath::Lerp(C, FLinearColor(0.13f, 0.20f, 0.08f), static_cast<float>(-Rise * 2.0));
		}
		return C;
	}

	// The country's land cover over the fields' colour (docs/COUNTRY_MAP.md): reed beds and wet meadow, woods, water.
	FLinearColor CoverTint(const FLinearColor& Fields, const char* Cover)
	{
		if (FCStringAnsi::Strcmp(Cover, "marsh") == 0)
		{
			return FMath::Lerp(Fields, FLinearColor(0.16f, 0.20f, 0.12f), 0.7f);
		}
		if (FCStringAnsi::Strcmp(Cover, "forest") == 0)
		{
			return FLinearColor(0.07f, 0.13f, 0.05f);
		}
		if (FCStringAnsi::Strcmp(Cover, "lake") == 0 || FCStringAnsi::Strcmp(Cover, "river") == 0)
		{
			return SurfaceColour(simcore::MapSurface::Water);
		}
		return Fields;
	}
}

ASimMapActor::ASimMapActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

FString ASimMapActor::FindDataDirectory()
{
	// The repo's data folder: <project>/../../data (as USimWorldSubsystem finds it).
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("data")));
}

void ASimMapActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTransform(FTransform::Identity);  // the meshes are in the sim's frame: the actor stays at the origin
	CheckTimer = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimMapShots="), ShotFolder) && !ShotFolder.IsEmpty())
	{
		ShotIndex = 0;
		ShotTimer = 12.0f;  // the scenario loads, the map is drawn, the ground's collision cooks, the sky captures
	}
}

void ASimMapActor::RunShots(float DeltaSeconds)
{
	// Views: a camera over the map's (east, north) km, metres above the ground there, looking at another point.
	struct FView
	{
		const TCHAR* Name;
		double E, N, Up, Te, Tn, TUp;
	};
	static const FView Views[] = {
		{TEXT("battery"), 5.75, -9.05, 4.0, 12.0, -6.0, 30.0},
		{TEXT("kesmark"), 5.0, -4.5, 300.0, 9.0, -1.0, 0.0},
		{TEXT("bridges"), 10.5, -8.5, 120.0, 13.0, -5.0, 0.0},
		{TEXT("woods"), -16.0, -6.0, 60.0, 9.0, -1.0, 20.0},
		{TEXT("aerial"), -6.0, -16.0, 3000.0, 6.0, -2.0, 0.0},
		{TEXT("high"), -20.0, -30.0, 9000.0, 25.0, 20.0, 0.0},
		{TEXT("country"), -20.0, -260.0, 90000.0, -10.0, 10.0, 0.0},  // the whole country from very high in the south
		{TEXT("refinery"), 33.3, 39.2, 250.0, 34.0, 40.0, 0.0},
		{TEXT("capital"), 26.0, 44.0, 3000.0, 36.0, 58.0, 0.0},
		{TEXT("reservoir"), 47.5, 104.0, 400.0, 52.5, 120.0, 100.0},      // up the Rava's gorge to the reservoir above the dam
		{TEXT("border_range"), 30.0, 112.0, 900.0, 45.0, 126.0, 300.0},   // the border range against the Pact
		{TEXT("lake_siro"), -78.0, -8.0, 600.0, -78.0, 2.0, 0.0},         // Lake Siro in its reeds
	};
	constexpr int32 Count = UE_ARRAY_COUNT(Views);
	if (ShotIndex < 0 || !Map || BuiltFor.X > 1e8)
	{
		return;
	}
	ShotTimer -= DeltaSeconds;
	if (ShotTimer > 0.0f)
	{
		return;
	}
	if (ShotIndex >= 2 * Count)
	{
		UE_LOG(LogSAM3D, Log, TEXT("Map shots: done (%s)"), *ShotFolder);
		FGenericPlatformMisc::RequestExit(false);
		ShotIndex = -1;
		return;
	}
	const FView& V = Views[ShotIndex / 2];
	if (ShotIndex % 2 == 0)  // place the camera, then give the picture two seconds to settle
	{
		const simcore::MapFrameLink Link(Map->Def().origin, simcore::GeoPoint::FromDegrees(BuiltFor.Y, BuiltFor.X, BuiltFor.Z));
		const auto At = [&](double Ek, double Nk, double Up)
		{
			const double E = Ek * 1000.0, N = Nk * 1000.0;
			return ToUnreal(Link.MapToFrame(E, N, Map->GroundHeight(E, N) + Up));
		};
		const FVector From = At(V.E, V.N, V.Up), To = At(V.Te, V.Tn, V.TUp);
		if (ShotCamera == nullptr)
		{
			ShotCamera = GetWorld()->SpawnActor<ACameraActor>(From, (To - From).Rotation());
		}
		ShotCamera->SetActorLocationAndRotation(From, (To - From).Rotation());
		if (APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
		{
			Pc->SetViewTarget(ShotCamera);
			if (Pc->MyHUD != nullptr)
			{
				Pc->MyHUD->bShowHUD = false;  // clean pictures
			}
		}
		ShotTimer = 2.5f;
	}
	else
	{
		const FString Path = FPaths::Combine(ShotFolder, FString::Printf(TEXT("map_%d_%s.png"), ShotIndex / 2, V.Name));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Map shots: %s"), *Path);
		ShotTimer = 1.0f;
	}
	++ShotIndex;
}

void ASimMapActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RunShots(DeltaSeconds);
	CheckTimer -= DeltaSeconds;
	if (CheckTimer > 0.0f)
	{
		return;
	}
	CheckTimer = 0.5f;
	// Draw it in the running scenario's frame; rebuild when a scenario with another origin starts.
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	if (Sim == nullptr || W == nullptr || !Sim->IsScenarioRunning())
	{
		return;
	}
	const simcore::GeoPoint& O = W->Frame().Origin();
	const FVector Origin(O.LonDeg(), O.LatDeg(), O.alt_m);
	if (!Origin.Equals(BuiltFor, 1e-9))
	{
		FString Error;
		if (!Build(Origin, Sim->GetDataDirectory(), Error))
		{
			UE_LOG(LogSAM3D, Error, TEXT("Map %s: %s"), *MapFile, *Error);
			BuiltFor = Origin;  // don't retry every half second
			return;
		}
		bWarnedFlat = false;
	}
	// The sim's ground should be the map's: say so once if the scenario plays on its own flat ground here.
	if (!bWarnedFlat && Map)
	{
		bWarnedFlat = true;
		const simcore::MapTerrain Mt(Map, O);
		const double Diff = FMath::Abs(W->GetTerrain().HeightAt(3000.0, 2000.0) - Mt.HeightAt(3000.0, 2000.0)) +
							FMath::Abs(W->GetTerrain().HeightAt(-4000.0, -3000.0) - Mt.HeightAt(-4000.0, -3000.0));
		if (Diff > 0.5)
		{
			UE_LOG(LogSAM3D, Warning,
				TEXT("Map %s: the scenario plays on its own ground, not the map's (set its \"terrain\" to {\"type\": \"map\", \"map\": \"%s\"}): line of sight and heights won't match what you see"),
				*MapFile, *MapFile);
		}
	}
}

void ASimMapActor::BuildPreview()
{
	if (!Map)
	{
		std::string Error;
		Map = simcore::LoadMapCached(TCHAR_TO_UTF8(*FindDataDirectory()), TCHAR_TO_UTF8(*MapFile), &Error);
		if (!Map)
		{
			UE_LOG(LogSAM3D, Error, TEXT("Map %s: %s"), *MapFile, UTF8_TO_TCHAR(Error.c_str()));
			return;
		}
	}
	const simcore::GeoPoint& O = Map->Def().origin;
	FString Error;
	Build(FVector(O.LonDeg(), O.LatDeg(), O.alt_m), FindDataDirectory(), Error);
}

bool ASimMapActor::BuildMap(FString& OutError)
{
	USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const simcore::World* W = Sim != nullptr ? Sim->GetSimWorld() : nullptr;
	const FString Data = Sim != nullptr ? Sim->GetDataDirectory() : FindDataDirectory();
	if (W != nullptr && Sim->IsScenarioRunning())
	{
		const simcore::GeoPoint& O = W->Frame().Origin();
		return Build(FVector(O.LonDeg(), O.LatDeg(), O.alt_m), Data, OutError);
	}
	std::string Error;
	Map = simcore::LoadMapCached(TCHAR_TO_UTF8(*Data), TCHAR_TO_UTF8(*MapFile), &Error);
	if (!Map)
	{
		OutError = UTF8_TO_TCHAR(Error.c_str());
		return false;
	}
	const simcore::GeoPoint& O = Map->Def().origin;
	return Build(FVector(O.LonDeg(), O.LatDeg(), O.alt_m), Data, OutError);
}

bool ASimMapActor::Build(const FVector& FrameOriginLonLatAlt, const FString& DataDirectory, FString& OutError)
{
	const double T0 = FPlatformTime::Seconds();
	std::string Error;
	Map = simcore::LoadMapCached(TCHAR_TO_UTF8(*DataDirectory), TCHAR_TO_UTF8(*MapFile), &Error);
	if (!Map)
	{
		OutError = UTF8_TO_TCHAR(Error.c_str());
		return false;
	}
	Country.reset();
	if (!CountryFile.IsEmpty())
	{
		// Built once a session (a fraction of a second) and kept: every raid's level draws the same country.
		static TMap<FString, std::shared_ptr<const simcore::CountryMap>> Cache;
		const FString Key = FPaths::Combine(DataDirectory, CountryFile);
		if (const std::shared_ptr<const simcore::CountryMap>* Found = Cache.Find(Key))
		{
			Country = *Found;
		}
		else
		{
			Country = simcore::LoadCountryMap(TCHAR_TO_UTF8(*DataDirectory), TCHAR_TO_UTF8(*CountryFile), &Error);
			if (!Country)
			{
				UE_LOG(LogSAM3D, Warning, TEXT("Map %s: the country %s does not load (%s): the map's far ground instead"), *MapFile, *CountryFile,
					UTF8_TO_TCHAR(Error.c_str()));
			}
			else
			{
				Cache.Add(Key, Country);
			}
		}
	}
	const double T1 = FPlatformTime::Seconds();
	Clear();
	const simcore::MapFrameLink Link(Map->Def().origin,
		simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z));
	BuildGround(Link);
	BuildStrips(Link);
	BuildBoxes(Link);
	if (Country)
	{
		BuildCountry(FrameOriginLonLatAlt);
	}
	if (bDrawAssets)
	{
		BuildAssets(FrameOriginLonLatAlt);
	}
	BuiltFor = FrameOriginLonLatAlt;
	UE_LOG(LogSAM3D, Log, TEXT("Map %s drawn for the frame at %.5f N %.5f E: %d boxes%s, %d assets' models; laid out in %.2f s, meshes %.2f s"),
		UTF8_TO_TCHAR(Map->Def().map_id.c_str()), FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, BoxCount,
		Country ? TEXT(" in the country") : TEXT(""), AssetCount, T1 - T0, FPlatformTime::Seconds() - T1);
	return true;
}

void ASimMapActor::Clear()
{
	for (UHierarchicalInstancedStaticMeshComponent* P : Parts)
	{
		if (P != nullptr)
		{
			P->DestroyComponent();
		}
	}
	Parts.Reset();
	for (UProceduralMeshComponent* T : GroundTiles)
	{
		if (T != nullptr)
		{
			T->DestroyComponent();
		}
	}
	GroundTiles.Reset();
	if (Strips != nullptr)
	{
		Strips->DestroyComponent();
		Strips = nullptr;
	}
	if (CountryStrips != nullptr)
	{
		CountryStrips->DestroyComponent();
		CountryStrips = nullptr;
	}
	for (USceneComponent* P : AssetParts)
	{
		if (P != nullptr)
		{
			P->DestroyComponent();
		}
	}
	AssetParts.Reset();
	AssetCount = 0;
	BoxCount = 0;
}

UMaterialInterface* ASimMapActor::ColourMaterial(const FLinearColor& Colour)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
	if (Mid != nullptr)
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
	}
	return Mid;
}

void ASimMapActor::BuildGround(const simcore::MapFrameLink& Link)
{
	UMaterialInterface* GroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Map/M_SimMapVertexColor.M_SimMapVertexColor"));
	if (GroundMaterial == nullptr)
	{
		GroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}
	const simcore::MapData& M = *Map;
	const simcore::MapDef& D = M.Def();
	// The far ground is drawn where the map has it, the country's 1 km ground only beyond (BuildCountry): a district's
	// far ground is its country's ground ("far_ground"), at the map's own spacing and with its far sites levelled, so the
	// far boxes and lines stand on what is drawn and the sim sees it. With the country drawn it takes its land cover.
	const simcore::MapFrameLink ToCountry(Country ? Country->Def().ground.origin : D.origin, D.origin);
	const auto FarColour = [&](double E, double N, double H)
	{
		const FLinearColor Fields = GroundColour(M.Def(), E, N, H);
		if (!Country)
		{
			return Fields;
		}
		const simcore::MapPoint P = ToCountry.FrameToMap(E, N);
		return CoverTint(Fields, Country->LandCoverAt(P.east_m, P.north_m));
	};
	if (Country && !D.far_ground)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Map %s: no \"far_ground\" (the country's file): its far ground is its own, not the country's drawn around it"),
			*MapFile);
	}
	// One square of the grid as a mesh of its own, cells [C0, C1) x [R0, R1) every Step samples. Far squares skip the
	// cells under the map and sink their edge against it, so no crack shows between the two.
	auto AddSection = [&](const simcore::MapGrid& G, int32 C0, int32 C1, int32 R0, int32 R1, int32 Step, bool bFar, bool bCollide)
	{
		const int32 Nc = (C1 - C0) / Step + 1, Nr = (R1 - R0) / Step + 1;
		TArray<FVector> Verts;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colours;
		TArray<FVector2D> Uv;
		TArray<int32> Tris;
		TArray<FProcMeshTangent> Tangents;
		Verts.Reserve(Nc * Nr);
		Normals.Reserve(Nc * Nr);
		Colours.Reserve(Nc * Nr);
		Uv.Reserve(Nc * Nr);
		const double Mid = G.EastOf((C0 + C1) / 2), MidN = G.NorthOf((R0 + R1) / 2);
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(Mid, MidN, Ae, An, Au);
		const auto InCore = [&](double E, double N)
		{ return E > D.west_m + 1.0 && E < D.east_m - 1.0 && N > D.south_m + 1.0 && N < D.north_m - 1.0; };
		for (int32 r = 0; r < Nr; ++r)
		{
			for (int32 c = 0; c < Nc; ++c)
			{
				const int32 Gc = FMath::Min(C0 + c * Step, G.cols - 1), Gr = FMath::Min(R0 + r * Step, G.rows - 1);
				const double E = G.EastOf(Gc), N = G.NorthOf(Gr);
				double H = G.At(Gc, Gr);
				if (bFar && D.InCore(E, N))
				{
					H -= 3.0;  // tucked under the map's edge
				}
				Verts.Add(ToUnreal(Link.MapToFrame(E, N, H)));
				const double He = G.At(FMath::Min(Gc + 1, G.cols - 1), Gr) - G.At(FMath::Max(Gc - 1, 0), Gr);
				const double Hn = G.At(Gc, FMath::Min(Gr + 1, G.rows - 1)) - G.At(Gc, FMath::Max(Gr - 1, 0));
				const simcore::Vec3 Local(-He / (2.0 * G.spacing_m), -Hn / (2.0 * G.spacing_m), 1.0);
				const simcore::Vec3 Nrm = (Ae * Local.x + An * Local.y + Au * Local.z).Normalized();
				Normals.Add(DirToUnreal(Nrm));
				Colours.Add(bFar ? FarColour(E, N, H) : GroundColour(M.Def(), E, N, H));
				Uv.Add(FVector2D(E / 1000.0, N / 1000.0));
			}
		}
		for (int32 r = 0; r + 1 < Nr; ++r)
		{
			for (int32 c = 0; c + 1 < Nc; ++c)
			{
				if (bFar)
				{
					const double Ce = G.EastOf(C0 + c * Step) + 0.5 * Step * G.spacing_m, Cn = G.NorthOf(R0 + r * Step) + 0.5 * Step * G.spacing_m;
					if (InCore(Ce, Cn))
					{
						continue;
					}
				}
				const int32 A = r * Nc + c, B = A + 1, Cc = A + Nc, Dd = Cc + 1;
				// The face's normal is (C - A) x (B - A) for (A, B, C): A, B (east), C (north) faces up.
				Tris.Append({A, B, Cc, B, Dd, Cc});
			}
		}
		if (Tris.Num() == 0)
		{
			return;
		}
		// A component per square: each cooks its own collision once (sections of one component would re-cook all of it).
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
		Tile->SetupAttachment(Root);
		Tile->bUseAsyncCooking = true;
		Tile->SetCastShadow(!bFar);
		Tile->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uv, Colours, Tangents, bCollide);
		Tile->SetMaterial(0, GroundMaterial);
		Tile->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Tile->RegisterComponent();
		GroundTiles.Add(Tile);
	};

	const simcore::MapGrid& Core = M.CoreGrid();
	const int32 Step = FMath::Max(1, GroundStep);
	const int32 Tile = 50 * Step;
	for (int32 R0 = 0; R0 < Core.rows - 1; R0 += Tile)
	{
		for (int32 C0 = 0; C0 < Core.cols - 1; C0 += Tile)
		{
			AddSection(Core, C0, FMath::Min(C0 + Tile, Core.cols - 1), R0, FMath::Min(R0 + Tile, Core.rows - 1), Step, false, bCollision);
		}
	}
	if (bDrawFarGround)
	{
		const simcore::MapGrid& Far = M.FarGrid();
		for (int32 R0 = 0; R0 < Far.rows - 1; R0 += 50)
		{
			for (int32 C0 = 0; C0 < Far.cols - 1; C0 += 50)
			{
				AddSection(Far, C0, FMath::Min(C0 + 50, Far.cols - 1), R0, FMath::Min(R0 + 50, Far.rows - 1), 1, true, false);
			}
		}
	}
}

void ASimMapActor::BuildStrips(const simcore::MapFrameLink& Link)
{
	Strips = NewObject<UProceduralMeshComponent>(this, TEXT("MapStrips"));
	Strips->SetupAttachment(Root);
	Strips->SetCastShadow(false);
	Strips->RegisterComponent();
	constexpr int32 Surfaces = 5;
	TArray<FVector> Verts[Surfaces];
	TArray<int32> Tris[Surfaces];
	TArray<FVector> Normals[Surfaces];
	for (const simcore::MapStrip& S : Map->Layout().strips)
	{
		const int32 K = static_cast<int32>(S.surface);
		const int32 N = static_cast<int32>(S.points.size());
		if (N < 2)
		{
			continue;
		}
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(S.points[N / 2].x, S.points[N / 2].y, Ae, An, Au);
		const FVector Up = DirToUnreal(Au);
		const int32 Base = Verts[K].Num();
		for (int32 i = 0; i < N; ++i)
		{
			const simcore::Vec3& P = S.points[i];
			const simcore::Vec3& A = S.points[FMath::Max(i - 1, 0)];
			const simcore::Vec3& B = S.points[FMath::Min(i + 1, N - 1)];
			const double Dx = B.x - A.x, Dy = B.y - A.y, L = FMath::Max(FMath::Sqrt(Dx * Dx + Dy * Dy), 1e-6);
			const double Re = Dy / L, Rn = -Dx / L;  // to the right of the line
			const double Half = 0.5 * S.width_m;
			Verts[K].Add(ToUnreal(Link.MapToFrame(P.x - Re * Half, P.y - Rn * Half, P.z)));
			Verts[K].Add(ToUnreal(Link.MapToFrame(P.x + Re * Half, P.y + Rn * Half, P.z)));
			Normals[K].Add(Up);
			Normals[K].Add(Up);
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 L0 = Base + 2 * i, R0 = L0 + 1, L1 = L0 + 2, R1 = L0 + 3;
			Tris[K].Append({L0, R0, L1, R0, R1, L1});  // facing up, as the ground's
		}
	}
	const TArray<FVector2D> NoUv;
	const TArray<FLinearColor> NoColours;
	const TArray<FProcMeshTangent> NoTangents;
	int32 Section = 0;
	for (int32 K = 0; K < Surfaces; ++K)
	{
		if (Tris[K].Num() == 0)
		{
			continue;
		}
		Strips->CreateMeshSection_LinearColor(Section, Verts[K], Tris[K], Normals[K], NoUv, NoColours, NoTangents, false);
		Strips->SetMaterial(Section, ColourMaterial(SurfaceColour(static_cast<simcore::MapSurface>(K))));
		++Section;
	}
}

void ASimMapActor::BuildBoxes(const simcore::MapFrameLink& Link)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	constexpr int32 Kinds = static_cast<int32>(simcore::MapPart::Count);
	TArray<FTransform> Instances[Kinds];
	for (const simcore::MapBox& B : Map->Layout().boxes)
	{
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(B.east_m, B.north_m, Ae, An, Au);
		const double S = FMath::Sin(B.heading_rad), C = FMath::Cos(B.heading_rad);
		const FVector Forward = DirToUnreal(Ae * S + An * C);
		const FVector Up = DirToUnreal(Au);
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
		const FVector At = ToUnreal(Link.MapToFrame(B.east_m, B.north_m, B.base_m + 0.5 * B.height_m));
		const bool bRound = simcore::MapPartIsRound(B.part);
		const FVector Scale(B.length_m, bRound ? B.length_m : B.width_m, B.height_m);  // the engine's cube and cylinder are 1 m
		Instances[static_cast<int32>(B.part)].Add(FTransform(Rot, At, Scale));
	}
	for (int32 K = 0; K < Kinds; ++K)
	{
		if (Instances[K].Num() == 0)
		{
			continue;
		}
		const simcore::MapPart Part = static_cast<simcore::MapPart>(K);
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this, FName(*FString::Printf(TEXT("Map_%s"), UTF8_TO_TCHAR(simcore::MapPartName(Part)))));
		H->SetupAttachment(Root);
		H->SetStaticMesh(simcore::MapPartIsRound(Part) ? Cylinder : Cube);
		H->SetMaterial(0, ColourMaterial(PartColour(Part)));
		// The vehicle finds the ground by tracing: only what it can drive on takes the trace (not tree tops or roofs).
		const bool bSolid = bCollision && (Part == simcore::MapPart::Bridge || Part == simcore::MapPart::Runway || Part == simcore::MapPart::Pad);
		H->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		H->SetCastShadow(Part != simcore::MapPart::Marsh && Part != simcore::MapPart::Pad && Part != simcore::MapPart::Runway);
		H->RegisterComponent();
		H->AddInstances(Instances[K], false, true);
		Parts.Add(H);
		BoxCount += Instances[K].Num();
	}
}

double ASimMapActor::CountryGround(double East, double North) const
{
	// The map's own ground where the map draws it (its rivers' banks and hills at 50 m, its far ground at 500 m), the
	// country's 1 km ground beyond.
	if (Map)
	{
		const simcore::MapFrameLink ToMap(Map->Def().origin, Country ? Country->Def().ground.origin : Map->Def().origin);
		const simcore::MapPoint P = ToMap.FrameToMap(East, North);
		if (Map->Def().InCore(P.east_m, P.north_m) || (bDrawFarGround && Map->FarGrid().Covers(P.east_m, P.north_m)) || !Country)
		{
			return Map->GroundHeight(P.east_m, P.north_m);
		}
	}
	return Country ? Country->GroundHeight(East, North) : 0.0;
}

void ASimMapActor::BuildCountry(const FVector& FrameOriginLonLatAlt)
{
	const simcore::CountryMap& Cm = *Country;
	const simcore::CountryDef& Cd = Cm.Def();
	const simcore::MapFrameLink Link(Cd.ground.origin,
		simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z));
	const simcore::MapFrameLink ToMap(Map->Def().origin, Cd.ground.origin);
	const simcore::MapDef& D = Map->Def();
	// Whether a point of the country lies under the map (which draws its own ground, lines and buildings there).
	const auto UnderMap = [&](double E, double N, double Margin)
	{
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		return P.east_m > D.west_m - Margin && P.east_m < D.east_m + Margin && P.north_m > D.south_m - Margin && P.north_m < D.north_m + Margin;
	};
	// Whether the map draws the ground there: its far ground (out to far_km, the country's ground at the map's spacing)
	// when drawn, else only the map proper.
	const auto UnderMapGround = [&](double E, double N, double Margin)
	{
		if (!bDrawFarGround)
		{
			return UnderMap(E, N, Margin);
		}
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		return FMath::Abs(P.east_m) < D.far_m + Margin && FMath::Abs(P.north_m) < D.far_m + Margin;
	};
	const simcore::MapGrid& G = Cm.Grid();
	const double W0 = G.west_m, W1 = G.EastOf(G.cols - 1), S0 = G.south_m, S1 = G.NorthOf(G.rows - 1);
	const auto InGrid = [&](double E, double N) { return E > W0 + 1.0 && E < W1 - 1.0 && N > S0 + 1.0 && N < S1 - 1.0; };
	UMaterialInterface* GroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SAM3D/Map/M_SimMapVertexColor.M_SimMapVertexColor"));
	if (GroundMaterial == nullptr)
	{
		GroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}

	// The ground: one mesh per square of cells, its points every Spacing from (E0, N0); cells where the map draws its
	// ground (or, beyond the country's grid, inside it) skipped and their edge sunk 3 m against the finer ground.
	const auto AddSquare = [&](double E0, double N0, double Spacing, int32 Nc, int32 Nr, bool bBeyond)
	{
		TArray<FVector> Verts;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colours;
		TArray<FVector2D> Uv;
		TArray<int32> Tris;
		const TArray<FProcMeshTangent> Tangents;
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(E0 + 0.5 * Spacing * (Nc - 1), N0 + 0.5 * Spacing * (Nr - 1), Ae, An, Au);
		for (int32 R = 0; R < Nr; ++R)
		{
			for (int32 C = 0; C < Nc; ++C)
			{
				const double E = E0 + C * Spacing, N = N0 + R * Spacing;
				double H = Cm.GroundHeight(E, N);
				if (UnderMapGround(E, N, 1.0) || (bBeyond && InGrid(E, N)))
				{
					H -= 3.0;
				}
				Verts.Add(ToUnreal(Link.MapToFrame(E, N, H)));
				const double He = Cm.GroundHeight(E + Spacing, N) - Cm.GroundHeight(E - Spacing, N);
				const double Hn = Cm.GroundHeight(E, N + Spacing) - Cm.GroundHeight(E, N - Spacing);
				const simcore::Vec3 Local(-He / (2.0 * Spacing), -Hn / (2.0 * Spacing), 1.0);
				Normals.Add(DirToUnreal((Ae * Local.x + An * Local.y + Au * Local.z).Normalized()));
				Colours.Add(CoverTint(GroundColour(Cd.ground, E, N, H), Cm.LandCoverAt(E, N)));
				Uv.Add(FVector2D(E / 1000.0, N / 1000.0));
			}
		}
		for (int32 R = 0; R + 1 < Nr; ++R)
		{
			for (int32 C = 0; C + 1 < Nc; ++C)
			{
				const double Ce = E0 + (C + 0.5) * Spacing, Cn = N0 + (R + 0.5) * Spacing;
				if (UnderMapGround(Ce, Cn, -1.0) || (bBeyond && InGrid(Ce, Cn)))
				{
					continue;
				}
				const int32 A = R * Nc + C, B = A + 1, Cc = A + Nc, Dd = Cc + 1;
				Tris.Append({A, B, Cc, B, Dd, Cc});
			}
		}
		if (Tris.Num() == 0)
		{
			return;
		}
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
		Tile->SetupAttachment(Root);
		Tile->SetCastShadow(false);
		Tile->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uv, Colours, Tangents, false);
		Tile->SetMaterial(0, GroundMaterial);
		Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tile->RegisterComponent();
		GroundTiles.Add(Tile);
	};
	// The country's grid at its own spacing (1 km), in squares of 50 cells.
	for (int32 R0 = 0; R0 < G.rows - 1; R0 += 50)
	{
		for (int32 C0 = 0; C0 < G.cols - 1; C0 += 50)
		{
			AddSquare(G.EastOf(C0), G.NorthOf(R0), G.spacing_m, FMath::Min(50, G.cols - 1 - C0) + 1, FMath::Min(50, G.rows - 1 - R0) + 1, false);
		}
	}
	// The land beyond its borders, 100 km out, at 4 km: what aircraft from the neighbours' bases fly over.
	const double Beyond = 100000.0, Coarse = 4000.0;
	const int32 Bc = static_cast<int32>((W1 - W0 + 2.0 * Beyond) / Coarse) + 1, Br = static_cast<int32>((S1 - S0 + 2.0 * Beyond) / Coarse) + 1;
	for (int32 R0 = 0; R0 < Br - 1; R0 += 40)
	{
		for (int32 C0 = 0; C0 < Bc - 1; C0 += 40)
		{
			AddSquare(W0 - Beyond + C0 * Coarse, S0 - Beyond + R0 * Coarse, Coarse, FMath::Min(40, Bc - 1 - C0) + 1, FMath::Min(40, Br - 1 - R0) + 1, true);
		}
	}

	// The lines as ribbons at strategic widths, on the ground's highest corner of the mesh cell under each point (so the
	// mesh never covers them: the map's far ground where it draws it, else the country's 1 km), every 400 m; not where
	// the map draws its own.
	const auto CellTop = [](const simcore::MapGrid& Gr, double E, double N)
	{
		const int32 C = FMath::Clamp(static_cast<int32>((E - Gr.west_m) / Gr.spacing_m), 0, Gr.cols - 2);
		const int32 R = FMath::Clamp(static_cast<int32>((N - Gr.south_m) / Gr.spacing_m), 0, Gr.rows - 2);
		return FMath::Max(FMath::Max(Gr.At(C, R), Gr.At(C + 1, R)), FMath::Max(Gr.At(C, R + 1), Gr.At(C + 1, R + 1)));
	};
	const auto MeshTop = [&](double E, double N)
	{
		if (UnderMapGround(E, N, 0.0))
		{
			const simcore::MapPoint P = ToMap.FrameToMap(E, N);
			const simcore::MapGrid& Mg = D.InCore(P.east_m, P.north_m) ? Map->CoreGrid() : Map->FarGrid();
			return CellTop(Mg, P.east_m, P.north_m);
		}
		if (!InGrid(E, N))
		{
			return Cm.GroundHeight(E, N);
		}
		return CellTop(G, E, N);
	};
	CountryStrips = NewObject<UProceduralMeshComponent>(this, TEXT("CountryStrips"));
	CountryStrips->SetupAttachment(Root);
	CountryStrips->SetCastShadow(false);
	CountryStrips->RegisterComponent();
	struct FKind
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		FLinearColor Colour;
	};
	FKind Kinds[5];  // water, main roads, other roads, railways, pipelines and the border
	Kinds[0].Colour = SurfaceColour(simcore::MapSurface::Water);
	Kinds[1].Colour = SurfaceColour(simcore::MapSurface::MainRoad);
	Kinds[2].Colour = SurfaceColour(simcore::MapSurface::Road);
	Kinds[3].Colour = SurfaceColour(simcore::MapSurface::Rail);
	Kinds[4].Colour = FLinearColor(0.42f, 0.12f, 0.08f);
	const auto Ribbon = [&](const std::vector<simcore::MapPoint>& Points, double Width, double Lift, FKind& K, bool bClosed)
	{
		// Resampled every 400 m; a run is broken where it passes under the map.
		if (Points.size() < 2)
		{
			return;
		}
		std::vector<simcore::MapPoint> P;
		const std::size_t N = Points.size() + (bClosed ? 1 : 0);
		for (std::size_t i = 0; i + 1 < N; ++i)
		{
			const simcore::MapPoint& A = Points[i];
			const simcore::MapPoint& B = Points[(i + 1) % Points.size()];
			const double L = FMath::Sqrt(FMath::Square(B.east_m - A.east_m) + FMath::Square(B.north_m - A.north_m));
			const int32 Steps = FMath::Max(1, static_cast<int32>(L / 400.0));
			for (int32 s = 0; s < Steps; ++s)
			{
				const double T = static_cast<double>(s) / Steps;
				P.push_back({A.east_m + (B.east_m - A.east_m) * T, A.north_m + (B.north_m - A.north_m) * T});
			}
		}
		P.push_back(bClosed ? Points.front() : Points.back());
		int32 RunStart = -1;
		const auto Flush = [&](int32 From, int32 To)  // points [From, To]
		{
			if (To - From < 1)
			{
				return;
			}
			const int32 Base = K.Verts.Num();
			for (int32 i = From; i <= To; ++i)
			{
				const simcore::MapPoint& Q = P[static_cast<std::size_t>(i)];
				const simcore::MapPoint& Qa = P[static_cast<std::size_t>(FMath::Max(i - 1, From))];
				const simcore::MapPoint& Qb = P[static_cast<std::size_t>(FMath::Min(i + 1, To))];
				const double Dx = Qb.east_m - Qa.east_m, Dy = Qb.north_m - Qa.north_m, L = FMath::Max(FMath::Sqrt(Dx * Dx + Dy * Dy), 1e-6);
				const double Re = Dy / L, Rn = -Dx / L, Half = 0.5 * Width;
				const double H = MeshTop(Q.east_m, Q.north_m) + Lift;
				simcore::Vec3 Ae, An, Au;
				Link.AxesAt(Q.east_m, Q.north_m, Ae, An, Au);
				K.Verts.Add(ToUnreal(Link.MapToFrame(Q.east_m - Re * Half, Q.north_m - Rn * Half, H)));
				K.Verts.Add(ToUnreal(Link.MapToFrame(Q.east_m + Re * Half, Q.north_m + Rn * Half, H)));
				K.Normals.Add(DirToUnreal(Au));
				K.Normals.Add(DirToUnreal(Au));
			}
			for (int32 i = 0; i < To - From; ++i)
			{
				const int32 L0 = Base + 2 * i, R0 = L0 + 1, L1 = L0 + 2, R1 = L0 + 3;
				K.Tris.Append({L0, R0, L1, R0, R1, L1});
			}
		};
		for (int32 i = 0; i < static_cast<int32>(P.size()); ++i)
		{
			const bool bOut = !UnderMap(P[static_cast<std::size_t>(i)].east_m, P[static_cast<std::size_t>(i)].north_m, 0.0);
			if (bOut && RunStart < 0)
			{
				RunStart = i;
			}
			if (!bOut && RunStart >= 0)
			{
				Flush(RunStart, i - 1);
				RunStart = -1;
			}
		}
		if (RunStart >= 0)
		{
			Flush(RunStart, static_cast<int32>(P.size()) - 1);
		}
	};
	for (const simcore::MapLineDef& L : Cd.ground.rivers)
	{
		Ribbon(L.points, FMath::Max(L.width_m, 60.0), 1.5, Kinds[0], false);
	}
	for (const simcore::MapLineDef& L : Cd.ground.roads)
	{
		const bool bMain = L.line_class == "main";
		Ribbon(L.points, bMain ? 30.0 : 18.0, 2.5, Kinds[bMain ? 1 : 2], false);
	}
	for (const simcore::MapLineDef& L : Cd.ground.railways)
	{
		Ribbon(L.points, 14.0, 2.5, Kinds[3], false);
	}
	for (const simcore::MapLineDef& L : Cd.pipelines)
	{
		Ribbon(L.points, 8.0, 2.0, Kinds[4], false);  // the pipeline's cleared strip
	}
	Ribbon(Cd.outline, 25.0, 2.0, Kinds[4], true);  // the border
	// The lakes: their surface as a sheet of 250 m cells at their level (on the mesh where its coarse ground stands
	// higher, as the rivers' ribbons are), not where the map draws its own.
	for (const simcore::MapLakeDef& Lake : Cd.ground.lakes)
	{
		double Lo0 = 1e18, Lo1 = 1e18, Hi0 = -1e18, Hi1 = -1e18;
		for (const simcore::MapPoint& Q : Lake.points)
		{
			Lo0 = FMath::Min(Lo0, Q.east_m);
			Hi0 = FMath::Max(Hi0, Q.east_m);
			Lo1 = FMath::Min(Lo1, Q.north_m);
			Hi1 = FMath::Max(Hi1, Q.north_m);
		}
		const double Cell = 250.0;
		for (double N = Lo1; N < Hi1; N += Cell)
		{
			for (double E = Lo0; E < Hi0; E += Cell)
			{
				const double Ce = E + 0.5 * Cell, Cn = N + 0.5 * Cell;
				if (!simcore::mapgeo::InPolygon(Lake.points, Ce, Cn) || UnderMap(Ce, Cn, 0.0))
				{
					continue;
				}
				const double H = FMath::Max(Lake.level_m, MeshTop(Ce, Cn)) + 1.0;
				simcore::Vec3 Ae, An, Au;
				Link.AxesAt(Ce, Cn, Ae, An, Au);
				FKind& K = Kinds[0];
				const int32 A = K.Verts.Num(), B = A + 1, Cc = A + 2, Dd = A + 3;
				K.Verts.Add(ToUnreal(Link.MapToFrame(E, N, H)));
				K.Verts.Add(ToUnreal(Link.MapToFrame(E + Cell, N, H)));
				K.Verts.Add(ToUnreal(Link.MapToFrame(E, N + Cell, H)));
				K.Verts.Add(ToUnreal(Link.MapToFrame(E + Cell, N + Cell, H)));
				for (int32 i = 0; i < 4; ++i)
				{
					K.Normals.Add(DirToUnreal(Au));
				}
				K.Tris.Append({A, B, Cc, B, Dd, Cc});
			}
		}
	}
	const TArray<FVector2D> NoUv;
	const TArray<FLinearColor> NoColours;
	const TArray<FProcMeshTangent> NoTangents;
	int32 Section = 0;
	for (FKind& K : Kinds)
	{
		if (K.Tris.Num() == 0)
		{
			continue;
		}
		CountryStrips->CreateMeshSection_LinearColor(Section, K.Verts, K.Tris, K.Normals, NoUv, NoColours, NoTangents, false);
		CountryStrips->SetMaterial(Section, ColourMaterial(K.Colour));
		++Section;
	}

	// The towns beyond the map: a patch of town ground and blocks on it (the capital's and the cities' taller), placed by
	// a hash of the town's id so they stand where they stood.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TArray<FTransform> Patches, Houses, Blocks;
	for (const simcore::CountrySettlementDef& S : Cd.settlements)
	{
		if (UnderMap(S.centre.east_m, S.centre.north_m, S.radius_m))
		{
			continue;
		}
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(S.centre.east_m, S.centre.north_m, Ae, An, Au);
		const FVector Up = DirToUnreal(Au);
		const FRotator Flat = FRotationMatrix::MakeFromXZ(DirToUnreal(An), Up).Rotator();
		const double Hc = MeshTop(S.centre.east_m, S.centre.north_m);
		Patches.Add(FTransform(Flat, ToUnreal(Link.MapToFrame(S.centre.east_m, S.centre.north_m, Hc + 0.5)), FVector(2.0 * S.radius_m, 2.0 * S.radius_m, 1.0)));
		const bool bBig = S.kind == "capital" || S.kind == "city";
		const int32 Count = S.kind == "capital" ? 420 : S.kind == "city" ? 200 : S.kind == "town" ? 90 : 20;
		const uint64 Seed = static_cast<uint64>(GetTypeHash(FString(UTF8_TO_TCHAR(S.id.c_str()))));
		for (int32 i = 0; i < Count; ++i)
		{
			const double U1 = simcore::noise::Value(Seed, i * 1.37, 0.5, 1.0), U2 = simcore::noise::Value(Seed ^ 0x51ull, i * 2.11, 1.5, 1.0);
			const double U3 = simcore::noise::Value(Seed ^ 0xA7ull, i * 0.73, 2.5, 1.0);
			const double Rr = S.radius_m * 0.9 * FMath::Sqrt(U1), A = 2.0 * UE_DOUBLE_PI * U2;
			const double E = S.centre.east_m + Rr * FMath::Sin(A), N = S.centre.north_m + Rr * FMath::Cos(A);
			const double Hb = bBig ? 12.0 + 30.0 * U3 * (1.0 - Rr / S.radius_m) : 6.0 + 4.0 * U3;
			const FVector Size = bBig ? FVector(40.0, 18.0, Hb) : FVector(12.0, 9.0, Hb);
			const FRotator Rot = FRotationMatrix::MakeFromXZ(DirToUnreal(Ae * FMath::Sin(U3 * 3.0) + An * FMath::Cos(U3 * 3.0)), Up).Rotator();
			(bBig ? Blocks : Houses).Add(FTransform(Rot, ToUnreal(Link.MapToFrame(E, N, MeshTop(E, N) + 0.5 * Hb)), Size));
		}
	}
	const auto AddInstances = [&](const TCHAR* Name, UStaticMesh* Mesh, const TArray<FTransform>& Xf, const FLinearColor& Colour, bool bShadow)
	{
		if (Xf.Num() == 0)
		{
			return;
		}
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		H->SetupAttachment(Root);
		H->SetStaticMesh(Mesh);
		H->SetMaterial(0, ColourMaterial(Colour));
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCastShadow(bShadow);
		H->RegisterComponent();
		H->AddInstances(Xf, false, true);
		Parts.Add(H);
		BoxCount += Xf.Num();
	};
	AddInstances(TEXT("Country_TownGround"), Cylinder, Patches, FLinearColor(0.21f, 0.2f, 0.18f), false);
	AddInstances(TEXT("Country_Houses"), Cube, Houses, PartColour(simcore::MapPart::House), true);
	AddInstances(TEXT("Country_Blocks"), Cube, Blocks, PartColour(simcore::MapPart::Flats), true);
}

void ASimMapActor::BuildAssets(const FVector& FrameOriginLonLatAlt)
{
	using namespace simcore::airwar;
	const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
	if (Lib == nullptr)
	{
		return;
	}
	// The running campaign's theatre and where it has its objects now, else the theatre file's.
	const USimAirCampaignSubsystem* AirWar = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USimAirCampaignSubsystem>() : nullptr;
	const AirCampaign* C = AirWar != nullptr ? AirWar->GetCampaign() : nullptr;
	static TMap<FString, TSharedPtr<TheatreDef>> Theatres;
	const TheatreDef* Th = C != nullptr ? &C->Data().theatre : nullptr;
	if (Th == nullptr)
	{
		const FString Key = FPaths::Combine(FindDataDirectory(), TheatreFile);
		TSharedPtr<TheatreDef>* Found = Theatres.Find(Key);
		if (Found == nullptr)
		{
			TSharedPtr<TheatreDef> Loaded = MakeShared<TheatreDef>();
			std::string Error;
			if (!LoadTheatreFile(TCHAR_TO_UTF8(*Key), *Loaded, &Error))
			{
				UE_LOG(LogSAM3D, Warning, TEXT("Map: the theatre %s does not load (%s): no assets drawn"), *TheatreFile, UTF8_TO_TCHAR(Error.c_str()));
				Loaded.Reset();
			}
			Found = &Theatres.Add(Key, Loaded);
		}
		Th = Found->Get();
	}
	if (Th == nullptr)
	{
		return;
	}
	const simcore::GeoPoint FrameOrigin = simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z);
	const simcore::MapFrameLink Link(Th->origin, FrameOrigin);
	const simcore::MapFrameLink ToCountry(Country ? Country->Def().ground.origin : Th->origin, Th->origin);
	// What the scenario draws itself: its neighbours (ASimScenarioDirector), its own battery and its dummies (entities).
	TSet<FString> Skip;
	const USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr;
	const simcore::ScenarioRunner* Runner = Sim != nullptr && Sim->IsScenarioRunning() ? Sim->GetScenarioRunner() : nullptr;
	if (Runner != nullptr)
	{
		for (const simcore::ScenarioNeighbour& Nb : Runner->Neighbours())
		{
			Skip.Add(UTF8_TO_TCHAR(Nb.id.c_str()));
		}
	}
	const FLinearColor Wreck(0.07f, 0.07f, 0.06f);
	for (int32 I = 0; I < static_cast<int32>(Th->objects.size()); ++I)
	{
		const TheatreObject& O = Th->objects[static_cast<std::size_t>(I)];
		const simcore::Vec3 Pos = C != nullptr ? C->Truth()[static_cast<std::size_t>(I)].pos : O.pos;
		const double Function = C != nullptr ? C->Truth()[static_cast<std::size_t>(I)].function : 1.0;
		if (O.decoy || Skip.Contains(UTF8_TO_TCHAR(O.id.c_str())))
		{
			continue;
		}
		if (Runner != nullptr && O.player)
		{
			continue;  // the player's own battery: the scenario's vehicles
		}
		const std::vector<std::pair<std::string, simcore::Vec3>> Site = Lib->ObjectLayout(ObjectKindName(O.kind), O.defence.system);
		for (const std::pair<std::string, simcore::Vec3>& V : Site)
		{
			const double E = Pos.x + V.second.x, N = Pos.y + V.second.y;
			const simcore::MapPoint Pc = ToCountry.FrameToMap(E, N);
			const double H = CountryGround(Pc.east_m, Pc.north_m);
			simcore::Vec3 Ae, An, Au;
			Link.AxesAt(E, N, Ae, An, Au);
			// Each faces out from its site's centre (launchers round their radar), the centre's north.
			const double Len = V.second.HorizontalLength();
			const simcore::Vec3 Fwd = Len > 1.0 ? Ae * (V.second.x / Len) + An * (V.second.y / Len) : An;
			USceneComponent* Holder = NewObject<USceneComponent>(this);
			Holder->SetupAttachment(Root);
			Holder->SetRelativeLocationAndRotation(ToUnreal(Link.MapToFrame(E, N, H)), FRotationMatrix::MakeFromXZ(DirToUnreal(Fwd), DirToUnreal(Au)).Rotator());
			Holder->RegisterComponent();
			AssetParts.Add(Holder);
			TArray<UStaticMeshComponent*> Built;
			FSimBasicModels::Build(UTF8_TO_TCHAR(V.first.c_str()), Holder, &Built, Function <= 0.05 ? &Wreck : nullptr);
			for (UStaticMeshComponent* Part : Built)
			{
				AssetParts.Add(Part);
			}
			++AssetCount;
		}
	}
}
