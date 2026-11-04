#include "SimMapActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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
#include "SimMapAssets.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

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
		case MapPart::TreeRow: return FLinearColor(0.09f, 0.17f, 0.06f);
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

	FLinearColor Linear(const std::array<float, 3>& C) { return FLinearColor(C[0], C[1], C[2]); }

	// The dressing is built once per map (a second or two) and kept while the map is: every raid's level on it shares it.
	struct FSimDressingCache
	{
		std::weak_ptr<const simcore::MapData> Map;
		FString RulesPath;
		std::shared_ptr<const simcore::scenery::DressingRules> Rules;
		std::shared_ptr<const simcore::scenery::CoverRaster> Cover;
		std::shared_ptr<const simcore::scenery::MapDressing> Dressing;
	};
	FSimDressingCache GSimDressingCache;

	// The day of the year the ground's crops are read on: the scenario's date (15 October, day 288, the sky clock's default,
	// without a scenario). The crop calendar (simcore/scenery/ground_layers.hpp) turns it into each field's layer.
	int GroundDay(const UWorld* World)
	{
		simcore::SkyClock Clock;
		if (USimWorldSubsystem* Sim = World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr)
		{
			Sim->GetSkyClock(Clock);
		}
		return Clock.day_of_year;
	}

	// The ground's layer mask as a texture (roadmap T1): a texel per land cover cell, unfiltered (the material reads the
	// four cells round a point itself), row 0 the southernmost, as simcore bakes it.
	UTexture2D* MaskTexture(const simcore::scenery::GroundMask& Mask)
	{
		if (Mask.cols <= 0 || Mask.rows <= 0 || Mask.cols > 16384 || Mask.rows > 16384)
		{
			return nullptr;
		}
		UTexture2D* T = UTexture2D::CreateTransient(Mask.cols, Mask.rows, PF_B8G8R8A8,
			MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(*FString::Printf(TEXT("GroundMask_%hs"), Mask.id.c_str()))));
		if (T == nullptr || T->GetPlatformData() == nullptr || T->GetPlatformData()->Mips.Num() == 0)
		{
			return nullptr;
		}
		T->Filter = TF_Nearest;
		T->SRGB = false;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->NeverStream = true;
		T->CompressionSettings = TC_VectorDisplacementmap;
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		uint8* Out = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		const int64 Count = static_cast<int64>(Mask.cols) * Mask.rows;
		for (int64 I = 0; I < Count; ++I)
		{
			// simcore's RGBA to the texture's BGRA.
			Out[I * 4 + 0] = Mask.texels[I * 4 + 2];
			Out[I * 4 + 1] = Mask.texels[I * 4 + 1];
			Out[I * 4 + 2] = Mask.texels[I * 4 + 0];
			Out[I * 4 + 3] = Mask.texels[I * 4 + 3];
		}
		Mip.BulkData.Unlock();
		T->UpdateResource();
		return T;
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
	// The level's drawn ground is this map's: entities are put on it by GroundAt, not by traces (roadmap R5).
	if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Sim->SetGroundMap(this);
	}
	CheckTimer = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimMapShots="), ShotFolder) && !ShotFolder.IsEmpty())
	{
		ShotIndex = 0;
		ShotTimer = 12.0f;  // the scenario loads, the map is drawn, the ground's collision cooks, the sky captures
	}
}

void ASimMapActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		if (Sim->GetGroundMap() == this)
		{
			Sim->SetGroundMap(nullptr);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASimMapActor::RunShots(float DeltaSeconds)
{
	// Views: a camera over the map's (east, north) km, metres above the ground there, looking at another point.
	struct FView
	{
		FString Name;
		double E, N, Up, Te, Tn, TUp;
		double FovDeg = 0.0;  // 0: the camera's own (90)
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
	if (ShotIndex < 0 || !Map || BuiltFor.X > 1e8)
	{
		return;
	}
	// And R6's close views, found on the map as it is laid out: a tree at a wood's edge in the Lazne woods, a street of
	// Kesmark, a bridge's pier from the bank, the capital from 150 m.
	TArray<FView> All(Views, UE_ARRAY_COUNT(Views));
	if (Dressing && !Dressing->trees.empty())
	{
		const simcore::scenery::TreeInstance* Best = nullptr;
		double BestD = 1e18;
		for (const simcore::scenery::TreeInstance& T : Dressing->trees)
		{
			const double Dd = FMath::Square(T.east_m + 11000.0) + FMath::Square(T.north_m + 3000.0);
			if (Dd < BestD)
			{
				BestD = Dd;
				Best = &T;
			}
		}
		const double E = Best->east_m / 1000.0, N = Best->north_m / 1000.0;
		All.Add(FView{TEXT("tree"), E, N - 0.03, 1.7, E, N, 0.6 * Best->height_m});
	}
	for (const simcore::MapSettlementDef& S : Map->Def().settlements)
	{
		if (S.id == "kesmark")
		{
			const double E = S.centre.east_m / 1000.0, N = S.centre.north_m / 1000.0;
			All.Add(FView{TEXT("street"), E - 0.05, N, 1.7, E + 0.25, N, 2.0});
		}
	}
	if (!Map->Layout().crossings.empty())
	{
		const simcore::MapCrossing& C = Map->Layout().crossings.front();
		const double E = C.at.east_m / 1000.0, N = C.at.north_m / 1000.0;
		const double Se = FMath::Sin(C.heading_rad), Sn = FMath::Cos(C.heading_rad);
		// From 60 m off the bridge's side, a little above the water, at its middle.
		All.Add(FView{TEXT("bridge_pier"), E + 0.06 * Sn, N - 0.06 * Se, 2.5, E, N, 2.0});
	}
	if (!Map->Def().detail_settlements.empty())
	{
		const simcore::MapSettlementDef& S = Map->Def().detail_settlements.front();
		const double E = S.centre.east_m / 1000.0, N = S.centre.north_m / 1000.0;
		All.Add(FView{TEXT("capital_150m"), E, N - 1.5, 150.0, E, N, 0.0});
	}
	// Track T's furrows (docs/scenery/TERRAIN_ROADMAP.md, view V1): 1.7 m over the turned field nearest the battery, well
	// inside it, looking along its furrows. The ground material turns a field's texture with them, so they run straight
	// ahead.
	if (bTexturedGround && GroundLayers && Cover && DressRules)
	{
		const int Day = GroundDay(GetWorld());
		const double Be = 5750.0, Bn = -9050.0, Reach = 1500.0;
		double BestD = 1e18, Fe = 0.0, Fn = 0.0, Fh = 0.0;
		for (const simcore::scenery::CoverGrid& G : Cover->grids)
		{
			for (int32 Row = 0; Row < G.rows; Row += 2)
			{
				const double Cn = G.CentreNorth(Row);
				if (FMath::Abs(Cn - Bn) > Reach)
				{
					continue;
				}
				for (int32 Col = 0; Col < G.cols; Col += 2)
				{
					const double Ce = G.CentreEast(Col);
					const double D = FMath::Square(Ce - Be) + FMath::Square(Cn - Bn);
					const std::size_t I = G.Index(Col, Row);
					if (D >= BestD || D > Reach * Reach || G.cover[I] != static_cast<std::uint8_t>(simcore::scenery::Cover::Field))
					{
						continue;
					}
					const int Layer = GroundLayers->LayerOnDay(simcore::scenery::Cover::Field, G.crop[I],
							simcore::FieldParcel(G.fields, Ce, Cn), Day);
					if (!GroundLayers->layers[static_cast<std::size_t>(Layer)].rows)
					{
						continue;
					}
					const double H = simcore::FieldHeading(G.fields, Ce, Cn);
					const double Ae = FMath::Sin(H), An = FMath::Cos(H);
					const std::uint64_t P = simcore::FieldParcel(G.fields, Ce, Cn);
					const auto Same = [&](double Along, double Across)
					{
						return simcore::FieldParcel(G.fields, Ce + Along * Ae + Across * An, Cn + Along * An - Across * Ae) == P;
					};
					if (Same(60.0, 0.0) && Same(-20.0, 0.0) && Same(0.0, 30.0) && Same(0.0, -30.0))
					{
						BestD = D;
						Fe = Ce;
						Fn = Cn;
						Fh = H;
					}
				}
			}
		}
		if (BestD < 1e18)
		{
			All.Add(FView{TEXT("furrows"), Fe / 1000.0, Fn / 1000.0, 1.7, (Fe + 12.0 * FMath::Sin(Fh)) / 1000.0, (Fn + 12.0 * FMath::Cos(Fh)) / 1000.0, 0.0});
		}
	}
	// -SimShotSet=views (roadmap T2, data/scenery/views.json): the terrain's check views, resolved on the map by simcore, in
	// place of the list above. A view the map cannot give leaves <name>.missing.txt with its reason.
	FString ShotSet;
	FParse::Value(FCommandLine::Get(), TEXT("SimShotSet="), ShotSet);
	const bool bViewShots = ShotSet == TEXT("views");
	if (bViewShots)
	{
		static TArray<FView> ViewList;
		static bool bViewListBuilt = false;
		if (!bViewListBuilt)
		{
			bViewListBuilt = true;
			IFileManager::Get().MakeDirectory(*ShotFolder, true);
			simcore::scenery::ViewSet Set;
			std::string Error;
			const FString ViewsPath = FPaths::Combine(FindDataDirectory(), TEXT("scenery/views.json"));
			if (!simcore::scenery::LoadViews(TCHAR_TO_UTF8(*ViewsPath), Set, &Error))
			{
				UE_LOG(LogSAM3D, Error, TEXT("Map shots: %s: %s"), *ViewsPath, UTF8_TO_TCHAR(Error.c_str()));
			}
			else if (DressRules && Cover)
			{
				for (const simcore::scenery::ViewDef& Def : Set.views)
				{
					for (const simcore::scenery::ResolvedView& C : simcore::scenery::ResolveView(*Map, *DressRules, *Cover, Def))
					{
						const FString Name = UTF8_TO_TCHAR(C.id.c_str());
						if (!C.ok)
						{
							UE_LOG(LogSAM3D, Log, TEXT("Map shots: %s is missing: %s"), *Name, UTF8_TO_TCHAR(C.missing_reason.c_str()));
							FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(C.missing_reason.c_str())), *FPaths::Combine(ShotFolder, Name + TEXT(".missing.txt")));
							continue;
						}
						ViewList.Add(FView{Name, C.eye_e / 1000.0, C.eye_n / 1000.0, C.eye_up_m, C.target_e / 1000.0, C.target_n / 1000.0, C.target_up_m,
							FMath::RadiansToDegrees(C.fov_rad)});
					}
				}
			}
			UE_LOG(LogSAM3D, Log, TEXT("Map shots: %d views from %s"), ViewList.Num(), *ViewsPath);
		}
		All = ViewList;
	}
	const int32 Count = All.Num();
	ShotTimer -= DeltaSeconds;
	// The cost: the frames' times over the last second and a half of the wait before the picture (the sky has settled).
	if (bViewShots && ShotIndex >= 0 && ShotIndex % 2 == 1 && ShotTimer > 0.0f && ShotTimer < 1.5f)
	{
		ShotGpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		ShotGameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		ShotRenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		++ShotSamples;
	}
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
	const FView& V = All[ShotIndex / 2];
	if (ShotIndex % 2 == 0)  // place the camera, then give the picture two seconds to settle
	{
		const simcore::MapFrameLink Link(Map->Def().origin, simcore::GeoPoint::FromDegrees(BuiltFor.Y, BuiltFor.X, BuiltFor.Z));
		const auto At = [&](double Ek, double Nk, double Up)
		{
			const double E = Ek * 1000.0, N = Nk * 1000.0;
			return ToUnreal(Link.GroundPoint(E, N, Map->GroundHeight(E, N) + Up));
		};
		const FVector From = At(V.E, V.N, V.Up), To = At(V.Te, V.Tn, V.TUp);
		if (ShotCamera == nullptr)
		{
			ShotCamera = GetWorld()->SpawnActor<ACameraActor>(From, (To - From).Rotation());
		}
		ShotCamera->SetActorLocationAndRotation(From, (To - From).Rotation());
		ShotCamera->GetCameraComponent()->SetFieldOfView(V.FovDeg > 0.0 ? static_cast<float>(V.FovDeg) : 90.0f);
		// This view's cost starts afresh.
		ShotGpuMs = ShotGameMs = ShotRenderMs = 0.0;
		ShotSamples = 0;
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
		// -SimShotHide=a,b: hide every actor whose name contains one of these (a probe: what hides the ground at a view).
		FString HideList;
		if (FParse::Value(FCommandLine::Get(), TEXT("SimShotHide="), HideList) && !HideList.IsEmpty())
		{
			TArray<FString> Names;
			HideList.ParseIntoArray(Names, TEXT(","));
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				for (const FString& N : Names)
				{
					if (It->GetName().Contains(N))
					{
						It->SetActorHiddenInGame(true);
					}
				}
			}
		}
		// -SimShotProbe: what the picture's eye is inside or beside (a probe): every visible primitive within 300 m of the eye, its
		// class, owner, size and first material.
		if (FParse::Param(FCommandLine::Get(), TEXT("SimShotProbe")) && ShotCamera != nullptr)
		{
			const FVector Eye = ShotCamera->GetActorLocation();
			for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
			{
				UPrimitiveComponent* P = *It;
				if (P->GetWorld() != GetWorld() || !P->IsVisible() || !P->IsRegistered())
				{
					continue;
				}
				const FBoxSphereBounds& B = P->Bounds;
				const double Away = FMath::Max(0.0, FVector::Dist(B.Origin, Eye) - B.SphereRadius);
				if (Away > 30000.0)
				{
					continue;
				}
				UMaterialInterface* Mat = P->GetNumMaterials() > 0 ? P->GetMaterial(0) : nullptr;
				UE_LOG(LogSAM3D, Log, TEXT("Probe %s: %s (%s) owner %s, extent %.0f x %.0f x %.0f m, %.0f m away, material %s"), *V.Name,
					*P->GetName(), *P->GetClass()->GetName(), P->GetOwner() != nullptr ? *P->GetOwner()->GetName() : TEXT("-"), B.BoxExtent.X / 50.0,
					B.BoxExtent.Y / 50.0, B.BoxExtent.Z / 50.0, Away / 100.0, Mat != nullptr ? *Mat->GetName() : TEXT("-"));
			}
		}
		const FString Path = FPaths::Combine(ShotFolder, bViewShots ? V.Name + TEXT(".png") : FString::Printf(TEXT("map_%d_%s.png"), ShotIndex / 2, *V.Name));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogSAM3D, Log, TEXT("Map shots: %s"), *Path);
		if (bViewShots)
		{
			// The mean of each time over the wait's frames, ms; 0 where the platform does not give one.
			const double N = FMath::Max(1, ShotSamples);
			FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"view\": \"%s\", \"samples\": %d, \"gpu_ms\": %.3f, \"game_ms\": %.3f, \"render_ms\": %.3f}\n"),
				*V.Name, ShotSamples, ShotGpuMs / N, ShotGameMs / N, ShotRenderMs / N), *FPaths::Combine(ShotFolder, V.Name + TEXT(".cost.json")));
			// The GPU's passes of one frame, to the log (read by tools/terrain_views.py).
			GEngine->Exec(GetWorld(), TEXT("r.ProfileGPU.ShowUI 0"));
			GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
		}
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
	GroundLink = std::make_shared<const simcore::MapFrameLink>(Map->Def().origin,
		simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z));
	const simcore::MapFrameLink& Link = *GroundLink;
	LoadDressing(DataDirectory);
	BuildGround(Link);
	BuildStrips(Link);
	BuildBoxes(Link);
	BuildTrees(Link);
	BuildBridges(Link);
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
	GroundMaterials.Reset();
	GroundMasks.Reset();
	FarCover.reset();
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
	if (Bridges != nullptr)
	{
		Bridges->DestroyComponent();
		Bridges = nullptr;
	}
	for (USceneComponent* P : AssetParts)
	{
		if (P != nullptr)
		{
			P->DestroyComponent();
		}
	}
	AssetParts.Reset();
	FSimBasicModels::RemoveTurningAntennas(this);
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

FVector ASimMapActor::GroundVertex(const simcore::MapFrameLink& Link, const simcore::MapGrid& Grid, int32 C, int32 R)
{
	return ToUnreal(Link.GroundPoint(Grid.EastOf(C), Grid.NorthOf(R), Grid.At(C, R)));
}

bool ASimMapActor::BuildForFrame(const FVector& FrameOriginLonLatAlt, FString& OutError)
{
	return Build(FrameOriginLonLatAlt, FindDataDirectory(), OutError);
}

bool ASimMapActor::DrawnGroundAt(const FVector& UnrealPos, FVector& OutGround) const
{
	// Straight down the frame's up (Unreal Z: a ground point keeps its east and north, MapFrameLink::GroundPoint), so a
	// face is found by its corners' X and Y alone; the skirts, edge-on from above, have none.
	bool bFound = false;
	double Highest = -UE_BIG_NUMBER;
	for (const TObjectPtr<UProceduralMeshComponent>& TilePtr : GroundTiles)
	{
		UProceduralMeshComponent* Tile = TilePtr.Get();
		if (Tile == nullptr)
		{
			continue;
		}
		const FBox Box = Tile->Bounds.GetBox();
		if (UnrealPos.X < Box.Min.X || UnrealPos.X > Box.Max.X || UnrealPos.Y < Box.Min.Y || UnrealPos.Y > Box.Max.Y)
		{
			continue;
		}
		const FProcMeshSection* Section = Tile->GetProcMeshSection(0);
		if (Section == nullptr)
		{
			continue;
		}
		const FTransform& Xf = Tile->GetComponentTransform();
		const TArray<FProcMeshVertex>& Verts = Section->ProcVertexBuffer;
		const TArray<uint32>& Index = Section->ProcIndexBuffer;
		for (int32 I = 0; I + 2 < Index.Num(); I += 3)
		{
			const FVector A = Xf.TransformPosition(Verts[static_cast<int32>(Index[I])].Position);
			const FVector B = Xf.TransformPosition(Verts[static_cast<int32>(Index[I + 1])].Position);
			const FVector C = Xf.TransformPosition(Verts[static_cast<int32>(Index[I + 2])].Position);
			const double D = (B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y);
			if (FMath::Abs(D) < 1.0)  // under a square centimetre from above: a skirt
			{
				continue;
			}
			const double U = ((UnrealPos.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (UnrealPos.Y - A.Y)) / D;
			const double V = ((B.X - A.X) * (UnrealPos.Y - A.Y) - (UnrealPos.X - A.X) * (B.Y - A.Y)) / D;
			if (U < -1e-9 || V < -1e-9 || U + V > 1.0 + 1e-9)
			{
				continue;
			}
			const double Z = A.Z + U * (B.Z - A.Z) + V * (C.Z - A.Z);
			if (Z > Highest)
			{
				Highest = Z;
				bFound = true;
			}
		}
	}
	if (bFound)
	{
		OutGround = FVector(UnrealPos.X, UnrealPos.Y, Highest);
	}
	return bFound;
}

bool ASimMapActor::GroundAt(const FVector& UnrealPos, FVector& OutGround, FVector* OutNormal) const
{
	if (!Map || !GroundLink)
	{
		return false;
	}
	const simcore::Vec3 P = USimWorldSubsystem::UnrealToEnu(UnrealPos);
	const simcore::MapPoint Mp = GroundLink->FrameToMap(P.x, P.y);
	const int32 Level = Map->LevelAt(Mp.east_m, Mp.north_m);
	if (Level < 0)
	{
		return false;
	}
	const double H = Map->GroundHeight(Mp.east_m, Mp.north_m);
	OutGround = ToUnreal(simcore::GroundPointInFrame(GroundLink->Frame(), P.x, P.y, H));
	if (OutNormal != nullptr)
	{
		// The face of the triangle the point is on: {A, B, C} or {B, D, C} of its cell (MapGrid::SampleTri).
		const std::vector<const simcore::MapGrid*> Levels = Map->DrawnLevels();
		const simcore::MapGrid& G = *Levels[static_cast<std::size_t>(Level)];
		int C = 0, R = 0;
		G.CellOf(Mp.east_m, Mp.north_m, C, R);
		const double Tx = (Mp.east_m - G.EastOf(C)) / G.spacing_m, Ty = (Mp.north_m - G.NorthOf(R)) / G.spacing_m;
		const bool bLower = Tx + Ty <= 1.0;
		const FVector A = bLower ? GroundVertex(*GroundLink, G, C, R) : GroundVertex(*GroundLink, G, C + 1, R + 1);
		const FVector B = GroundVertex(*GroundLink, G, C + 1, R);
		const FVector Cc = GroundVertex(*GroundLink, G, C, R + 1);
		FVector Nrm = FVector::CrossProduct(B - A, Cc - A).GetSafeNormal();
		if (Nrm.Z < 0.0)
		{
			Nrm = -Nrm;
		}
		*OutNormal = Nrm.IsNearlyZero() ? FVector::UpVector : Nrm;
	}
	return true;
}

void ASimMapActor::LoadDressing(const FString& DataDirectory)
{
	// The map's dressing (roadmap R6): the rules, the land cover and the trees, simcore's (scenery/map_dressing.hpp), built
	// once per map. Without the rules' file the defaults (the file's values) serve.
	const FString RulesPath = FPaths::Combine(DataDirectory, TEXT("scenery/map_dressing.json"));
	FSimDressingCache& C = GSimDressingCache;
	if (C.Map.lock() != Map || C.RulesPath != RulesPath || !C.Rules || !C.Cover || !C.Dressing)
	{
		const double T0 = FPlatformTime::Seconds();
		std::shared_ptr<simcore::scenery::DressingRules> Rules = std::make_shared<simcore::scenery::DressingRules>();
		std::string Error;
		if (!simcore::scenery::LoadDressingRules(TCHAR_TO_UTF8(*RulesPath), *Rules, &Error))
		{
			UE_LOG(LogSAM3D, Warning, TEXT("Map %s: %s: the dressing's defaults instead"), *MapFile, UTF8_TO_TCHAR(Error.c_str()));
		}
		C.Map = Map;
		C.RulesPath = RulesPath;
		C.Rules = Rules;
		C.Cover = std::make_shared<const simcore::scenery::CoverRaster>(simcore::scenery::BakeCover(*Map, *Rules));
		C.Dressing = std::make_shared<const simcore::scenery::MapDressing>(simcore::scenery::PlaceTrees(*Map, *Rules));
		UE_LOG(LogSAM3D, Log, TEXT("Map %s: dressing %d trees, %d wood masses, the land cover in %d squares (%.2f s)"), *MapFile,
			static_cast<int32>(C.Dressing->trees.size()), static_cast<int32>(C.Dressing->masses.size()), static_cast<int32>(C.Cover->grids.size()),
			FPlatformTime::Seconds() - T0);
	}
	DressRules = C.Rules;
	Cover = C.Cover;
	Dressing = C.Dressing;
	// The ground's layers (roadmap T1): a small file, read each build. Without it the ground keeps the cover's colours.
	GroundLayers.reset();
	const FString LayersPath = FPaths::Combine(DataDirectory, TEXT("scenery/ground_layers.json"));
	std::shared_ptr<simcore::scenery::GroundLayerRules> LayerRules = std::make_shared<simcore::scenery::GroundLayerRules>();
	std::string Error;
	if (simcore::scenery::LoadGroundLayers(TCHAR_TO_UTF8(*LayersPath), *DressRules, *LayerRules, &Error))
	{
		GroundLayers = LayerRules;
	}
	else
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Map %s: %s: the ground keeps the land cover's colours"), *MapFile, UTF8_TO_TCHAR(Error.c_str()));
	}
}

TArray<UMaterialInterface*> ASimMapActor::TexturedGround(const simcore::MapFrameLink& Link, const std::vector<const simcore::MapGrid*>& Levels)
{
	TArray<UMaterialInterface*> Out;
	Out.Init(nullptr, static_cast<int32>(Levels.size()));
	if (!bTexturedGround || !GroundLayers || !Cover || !DressRules)
	{
		return Out;
	}
	// The ground material with this machine's texture arrays (tools/ue_build_ground_material.py makes it once the
	// textures are fetched, docs/scenery/TERRAIN_ROADMAP.md). Not there: the plain ground, said once.
	UMaterialInterface* Parent =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ThirdParty/Ground/MI_SimGround.MI_SimGround"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Parent == nullptr)
	{
		static bool bSaid = false;
		if (!bSaid)
		{
			bSaid = true;
			UE_LOG(LogSAM3D, Log, TEXT("Map %s: no /Game/ThirdParty/Ground/MI_SimGround on this machine: the ground keeps the land cover's colours "
				"(python tools/fetch_textures.py, then tools/ue_build_ground_material.py)"), *MapFile);
		}
		return Out;
	}
	const int Day = GroundDay(GetWorld());
	// The world's (east, north) in m to the map's: the scenario's frame is the world's less this actor's place, and the
	// frame's (east, north) goes to the map's by an affine map (MapFrameLink::FrameToMap, exactly). The material finds a
	// pixel's land cover cell by it (the tiles' UVs would do, but a procedural mesh may store them as 16-bit floats: steps
	// of 30 m this far out).
	const FVector Here = GetActorLocation();
	const double ActE = Here.Y / 100.0, ActN = Here.X / 100.0;
	const simcore::MapPoint O = Link.FrameToMap(0.0, 0.0), Ex = Link.FrameToMap(1000.0, 0.0), Ny = Link.FrameToMap(0.0, 1000.0);
	const double Ma = (Ex.east_m - O.east_m) / 1000.0, Mb = (Ny.east_m - O.east_m) / 1000.0;
	const double Mc = (Ex.north_m - O.north_m) / 1000.0, Md = (Ny.north_m - O.north_m) / 1000.0;
	const FLinearColor MapOff(static_cast<float>(O.east_m - Ma * ActE - Mb * ActN), static_cast<float>(O.north_m - Mc * ActE - Md * ActN), 0.0f, 0.0f);
	const FLinearColor MapAxes(static_cast<float>(Ma), static_cast<float>(Mb), static_cast<float>(Mc), static_cast<float>(Md));
	FLinearColor Tiles[4];
	for (int32 I = 0; I < simcore::scenery::kMaxGroundLayers; ++I)
	{
		const float Tile = I < static_cast<int32>(GroundLayers->layers.size()) ? static_cast<float>(GroundLayers->layers[I].tile_m) : 3.0f;
		Tiles[I / 4].Component(I % 4) = Tile;
	}
	// The far ground wears the country's land cover (roadmap T3.2): baked once a session (a second) and kept.
	if (Country && !FarCover)
	{
		static TMap<FString, std::shared_ptr<const simcore::scenery::CoverGrid>> FarCache;
		const FString Key = MapFile + TEXT("|") + CountryFile;
		if (const std::shared_ptr<const simcore::scenery::CoverGrid>* Found = FarCache.Find(Key))
		{
			FarCover = *Found;
		}
		else
		{
			FarCover = std::make_shared<const simcore::scenery::CoverGrid>(simcore::scenery::BakeFarCover(*Map, *Country, *DressRules));
			FarCache.Add(Key, FarCover);
		}
	}
	// A level is drawn with the land cover square that matches it: a detail area's own, the map proper's ("map"), the far
	// ground's.
	const simcore::MapGrid& Proper = Map->CoreGrid();
	for (std::size_t L = 0; L < Levels.size(); ++L)
	{
		const simcore::MapGrid& G = *Levels[L];
		const simcore::scenery::CoverGrid* Square = nullptr;
		const bool bFarLevel = &G == &Map->FarGrid() && FarCover != nullptr;
		if (bFarLevel)
		{
			Square = FarCover.get();
		}
		for (const simcore::scenery::CoverGrid& C : Cover->grids)
		{
			if (bFarLevel)
			{
				break;
			}
			const bool bProper = C.id == "map";
			if ((&G == &Proper) == bProper && (bProper || (FMath::Abs(C.west_m - G.west_m) < 1.0 && FMath::Abs(C.south_m - G.south_m) < 1.0)))
			{
				Square = &C;
				break;
			}
		}
		if (Square == nullptr)
		{
			continue;
		}
		UTexture2D* Mask = MaskTexture(simcore::scenery::BakeGroundMask(*Square, *DressRules, *GroundLayers, Day));
		if (Mask == nullptr)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
		Mid->SetTextureParameterValue(TEXT("Mask"), Mask);
		// -SimGroundDebug=1 or 2: the material draws what it found (the layers, the depth) instead of the ground (a probe).
		float GroundDebug = 0.0f;
		FParse::Value(FCommandLine::Get(), TEXT("SimGroundDebug="), GroundDebug);
		Mid->SetScalarParameterValue(TEXT("Debug"), GroundDebug);
		// The layers' average colours take over from the vertex colours late: the map proper keeps its layers to its own edge
		// (30 km), the far ground's from the first metre (no detail textures that far: the albedo's last mip) to 60 km, so the
		// two meet at the same colour (roadmap T3.2).
		Mid->SetScalarParameterValue(TEXT("FarStartM"), bFarLevel ? 0.0f : 400.0f);
		Mid->SetScalarParameterValue(TEXT("FarLengthM"), bFarLevel ? 1.0f : 1600.0f);
		Mid->SetScalarParameterValue(TEXT("CoverStartM"), bFarLevel ? 60000.0f : 30000.0f);
		Mid->SetScalarParameterValue(TEXT("CoverLengthM"), bFarLevel ? 60000.0f : 30000.0f);
		Mid->SetVectorParameterValue(TEXT("MapOff"), MapOff);
		Mid->SetVectorParameterValue(TEXT("MapAxes"), MapAxes);
		Mid->SetScalarParameterValue(TEXT("PeriodM"), static_cast<float>(simcore::scenery::kGroundTilePeriodM));
		// The square in the map's frame, km.
		Mid->SetVectorParameterValue(TEXT("MaskRect"), FLinearColor(static_cast<float>(Square->west_m / 1000.0), static_cast<float>(Square->south_m / 1000.0),
			static_cast<float>(Square->cell_m * Square->cols / 1000.0), static_cast<float>(Square->cell_m * Square->rows / 1000.0)));
		Mid->SetScalarParameterValue(TEXT("MaskCols"), static_cast<float>(Square->cols));
		Mid->SetScalarParameterValue(TEXT("MaskRows"), static_cast<float>(Square->rows));
		// The plain's fields (simcore/farm_fields.hpp), found exactly by the material's port of FieldAt. The seed is a 32-bit
		// number: two halves, each exact in a float. Without a "farmland" block the estate size 0 turns the port off.
		if (Map->Def().farmland.laid)
		{
			const simcore::FieldGrid FG = Map->Fields();
			Mid->SetVectorParameterValue(TEXT("FieldGridA"), FLinearColor(static_cast<float>(FG.estate_m), static_cast<float>(FG.length_min_m),
				static_cast<float>(FG.length_max_m), static_cast<float>(FG.width_min_m)));
			Mid->SetVectorParameterValue(TEXT("FieldGridB"), FLinearColor(static_cast<float>(FG.width_max_m), static_cast<float>(FG.seed >> 16),
				static_cast<float>(FG.seed & 0xFFFFu), 0.0f));
		}
		Mid->SetVectorParameterValue(TEXT("TileA"), Tiles[0]);
		Mid->SetVectorParameterValue(TEXT("TileB"), Tiles[1]);
		Mid->SetVectorParameterValue(TEXT("TileC"), Tiles[2]);
		Mid->SetVectorParameterValue(TEXT("TileD"), Tiles[3]);
		GroundMasks.Add(Mask);
		GroundMaterials.Add(Mid);
		Out[static_cast<int32>(L)] = Mid;
	}
	UE_LOG(LogSAM3D, Log, TEXT("Map %s: the textured ground on %d of %d levels (crops of day %d)"), *MapFile, GroundMaterials.Num(), static_cast<int32>(Levels.size()),
		Day);
	return Out;
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
	// One ground (roadmap R5): every level simcore samples (MapData::DrawnLevels: the map proper at 50 m, its far
	// ground at 500 m, the country's 1 km grid and the 4 km land beyond it), each vertex simcore's node placed straight
	// down the scenario frame's up (MapFrameLink::GroundPoint), each cell the two triangles MapGrid::SampleTri samples,
	// a coarser level leaving out only the cells wholly under a finer one (MapData::DrawsCell). No sinks, no patches
	// laid over it: what stands on the sim's ground stands on this. A skirt 5 m deep hangs from every edge where a level
	// stops, to hide the cracks where a coarse edge meets the finer level's nodes.
	if (Country && !D.far_ground)
	{
		UE_LOG(LogSAM3D, Warning, TEXT("Map %s: no \"far_ground\" (the country's file): the country's ground is not drawn round it"), *MapFile);
	}
	const simcore::MapFrameLink ToCountry(Country ? Country->Def().ground.origin : D.origin, D.origin);
	const simcore::MapFrameLink CountryToMap(D.origin, Country ? Country->Def().ground.origin : D.origin);
	// The towns the map does not draw itself: their ground in the town's colour (the patch laid over it before R5).
	struct FTown
	{
		double E, N, R2;
	};
	TArray<FTown> Towns;
	if (Country)
	{
		for (const simcore::CountrySettlementDef& S : Country->Def().settlements)
		{
			const simcore::MapPoint P = CountryToMap.FrameToMap(S.centre.east_m, S.centre.north_m);
			const bool bUnderMap = P.east_m > D.west_m - S.radius_m && P.east_m < D.east_m + S.radius_m && P.north_m > D.south_m - S.radius_m &&
								   P.north_m < D.north_m + S.radius_m;
			if (!bUnderMap)
			{
				Towns.Add({P.east_m, P.north_m, S.radius_m * S.radius_m});
			}
		}
	}
	const FLinearColor TownColour(0.21f, 0.2f, 0.18f);
	const std::vector<const simcore::MapGrid*> Levels = M.DrawnLevels();
	// The map proper and the detail areas with their layers' textures where this machine has them (roadmap T1).
	const TArray<UMaterialInterface*> Textured = TexturedGround(Link, Levels);
	// The detail areas (roadmap R6) come first in the levels: one inside the map proper is drawn as the map proper is
	// (its colours, collision, shadows), one beyond it as the far ground near the scenario is.
	const auto IsCoreLike = [&](std::size_t Level)
	{
		const simcore::MapGrid* G = Levels[Level];
		return G == &M.CoreGrid() || (Level < M.DetailGrids().size() && D.InCore(G->west_m + 1.0, G->south_m + 1.0));
	};
	const auto IsFarLike = [&](std::size_t Level)
	{
		return Levels[Level] == &M.FarGrid() || (Level < M.DetailGrids().size() && !IsCoreLike(Level));
	};
	const auto Colour = [&](std::size_t Level, double E, double N, double H)
	{
		// The map proper and the detail areas: the land cover's colour (roadmap R6: a field's crop, a wood, a town, military
		// ground), a little browner up the hills and greener low by the river, as GroundColour tints its fields.
		if ((IsCoreLike(Level) || Level < M.DetailGrids().size()) && Cover && DressRules && Cover->Has(E, N))
		{
			FLinearColor C = Linear(Cover->ColourAt(*DressRules, E, N));
			const double Rise = FMath::Clamp((H - D.base_m) / 60.0, -0.3, 1.0);
			C = FMath::Lerp(C, FLinearColor(0.30f, 0.27f, 0.18f), static_cast<float>(FMath::Max(0.0, Rise) * 0.25));
			return C;
		}
		if (IsCoreLike(Level))
		{
			return GroundColour(D, E, N, H);
		}
		for (const FTown& T : Towns)
		{
			if (FMath::Square(E - T.E) + FMath::Square(N - T.N) < T.R2)
			{
				return TownColour;
			}
		}
		if (!Country)
		{
			return GroundColour(D, E, N, H);
		}
		const simcore::MapPoint P = ToCountry.FrameToMap(E, N);
		return CoverTint(GroundColour(IsFarLike(Level) ? D : Country->Def().ground, E, N, H), Country->LandCoverAt(P.east_m, P.north_m));
	};
	// The scenario's origin in the map: the far ground's tiles within 15 km of it take queries (the camera's, physics').
	const simcore::MapPoint Here = Link.FrameToMap(0.0, 0.0);
	constexpr double QueryReachM = 15000.0;
	constexpr double SkirtM = 5.0;
	// One square of a level as a mesh of its own: cells [C0, C1) x [R0, R1).
	auto AddSection = [&](std::size_t Level, int32 C0, int32 C1, int32 R0, int32 R1)
	{
		const simcore::MapGrid& G = *Levels[Level];
		const int32 Nc = C1 - C0 + 1, Nr = R1 - R0 + 1;
		TArray<int32> Tris;
		TArray<uint8> Drawn;
		Drawn.SetNumZeroed((C1 - C0) * (R1 - R0));
		bool bAny = false;
		for (int32 r = R0; r < R1; ++r)
		{
			for (int32 c = C0; c < C1; ++c)
			{
				const bool bDraws = M.DrawsCell(Level, c, r);
				Drawn[(r - R0) * (C1 - C0) + (c - C0)] = bDraws ? 1 : 0;
				bAny = bAny || bDraws;
			}
		}
		if (!bAny)
		{
			return;
		}
		TArray<FVector> Verts;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colours;
		TArray<FVector2D> Uv;
		TArray<FProcMeshTangent> Tangents;
		Verts.Reserve(Nc * Nr);
		Normals.Reserve(Nc * Nr);
		Colours.Reserve(Nc * Nr);
		Uv.Reserve(Nc * Nr);
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(G.EastOf((C0 + C1) / 2), G.NorthOf((R0 + R1) / 2), Ae, An, Au);
		const auto NormalAt = [&](int32 Gc, int32 Gr)
		{
			const double He = G.At(FMath::Min(Gc + 1, G.cols - 1), Gr) - G.At(FMath::Max(Gc - 1, 0), Gr);
			const double Hn = G.At(Gc, FMath::Min(Gr + 1, G.rows - 1)) - G.At(Gc, FMath::Max(Gr - 1, 0));
			const simcore::Vec3 Local(-He / (2.0 * G.spacing_m), -Hn / (2.0 * G.spacing_m), 1.0);
			return DirToUnreal((Ae * Local.x + An * Local.y + Au * Local.z).Normalized());
		};
		for (int32 r = 0; r < Nr; ++r)
		{
			for (int32 c = 0; c < Nc; ++c)
			{
				const int32 Gc = C0 + c, Gr = R0 + r;
				const double E = G.EastOf(Gc), N = G.NorthOf(Gr), H = G.At(Gc, Gr);
				Verts.Add(GroundVertex(Link, G, Gc, Gr));
				Normals.Add(NormalAt(Gc, Gr));
				Colours.Add(Colour(Level, E, N, H));
				Uv.Add(FVector2D(E / 1000.0, N / 1000.0));
			}
		}
		// A skirt down from the edge between two nodes of the grid: both faces, so it hides the crack from either side.
		const auto Skirt = [&](int32 Ca, int32 Ra, int32 Cb, int32 Rb)
		{
			const int32 Base = Verts.Num();
			for (const FIntPoint& Node : {FIntPoint(Ca, Ra), FIntPoint(Cb, Rb)})
			{
				const double E = G.EastOf(Node.X), N = G.NorthOf(Node.Y), H = G.At(Node.X, Node.Y);
				Verts.Add(GroundVertex(Link, G, Node.X, Node.Y));
				Verts.Add(ToUnreal(Link.GroundPoint(E, N, H - SkirtM)));
				const FVector Nrm = NormalAt(Node.X, Node.Y);
				Normals.Add(Nrm);
				Normals.Add(Nrm);
				const FLinearColor Col = Colour(Level, E, N, H);
				Colours.Add(Col);
				Colours.Add(Col);
				Uv.Add(FVector2D(E / 1000.0, N / 1000.0));
				Uv.Add(FVector2D(E / 1000.0, N / 1000.0));
			}
			const int32 Ta = Base, Ba = Base + 1, Tb = Base + 2, Bb = Base + 3;
			Tris.Append({Ta, Tb, Ba, Tb, Bb, Ba, Ta, Ba, Tb, Tb, Ba, Bb});
		};
		const auto DrawnAt = [&](int32 c, int32 r)
		{
			if (c < 0 || r < 0 || c >= G.cols - 1 || r >= G.rows - 1)
			{
				return false;
			}
			if (c >= C0 && c < C1 && r >= R0 && r < R1)
			{
				return Drawn[(r - R0) * (C1 - C0) + (c - C0)] != 0;
			}
			return M.DrawsCell(Level, c, r);
		};
		for (int32 r = 0; r + 1 < Nr; ++r)
		{
			for (int32 c = 0; c + 1 < Nc; ++c)
			{
				const int32 Gc = C0 + c, Gr = R0 + r;
				if (Drawn[r * (C1 - C0) + c] == 0)
				{
					continue;
				}
				const int32 A = r * Nc + c, B = A + 1, Cc = A + Nc, Dd = Cc + 1;
				// The face's normal is (C - A) x (B - A) for (A, B, C): A, B (east), C (north) faces up. The split along
				// B-C is simcore's (MapGrid::SampleTri): the contract.
				Tris.Append({A, B, Cc, B, Dd, Cc});
				if (!DrawnAt(Gc, Gr - 1))
				{
					Skirt(Gc, Gr, Gc + 1, Gr);
				}
				if (!DrawnAt(Gc, Gr + 1))
				{
					Skirt(Gc, Gr + 1, Gc + 1, Gr + 1);
				}
				if (!DrawnAt(Gc - 1, Gr))
				{
					Skirt(Gc, Gr, Gc, Gr + 1);
				}
				if (!DrawnAt(Gc + 1, Gr))
				{
					Skirt(Gc + 1, Gr, Gc + 1, Gr + 1);
				}
			}
		}
		// Collision: the map proper's (bCollision), the far ground's near the scenario for queries only.
		const double E0 = G.EastOf(C0), E1 = G.EastOf(C1), N0 = G.NorthOf(R0), N1 = G.NorthOf(R1);
		const double Away = FMath::Sqrt(FMath::Square(FMath::Max3(E0 - Here.east_m, 0.0, Here.east_m - E1)) +
										FMath::Square(FMath::Max3(N0 - Here.north_m, 0.0, Here.north_m - N1)));
		const bool bCollide = IsCoreLike(Level) ? bCollision : (IsFarLike(Level) && bCollision && Away < QueryReachM);
		// A component per square: each cooks its own collision once (sections of one component would re-cook all of it).
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
		Tile->SetupAttachment(Root);
		Tile->bUseAsyncCooking = true;
		Tile->SetCastShadow(IsCoreLike(Level));
		Tile->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uv, Colours, Tangents, bCollide);
		Tile->SetMaterial(0, Textured[static_cast<int32>(Level)] != nullptr ? Textured[static_cast<int32>(Level)] : GroundMaterial);
		Tile->SetCollisionEnabled(!bCollide ? ECollisionEnabled::NoCollision
											: (IsCoreLike(Level) ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly));
		Tile->RegisterComponent();
		GroundTiles.Add(Tile);
	};
	for (std::size_t Level = 0; Level < Levels.size(); ++Level)
	{
		const simcore::MapGrid& G = *Levels[Level];
		const int32 Square = 50;
		for (int32 R0 = 0; R0 < G.rows - 1; R0 += Square)
		{
			for (int32 C0 = 0; C0 < G.cols - 1; C0 += Square)
			{
				AddSection(Level, C0, FMath::Min(C0 + Square, G.cols - 1), R0, FMath::Min(R0 + Square, G.rows - 1));
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
	// The five surfaces (simcore::MapSurface), then a road's and a railway's shoulders (gravel 1.5 m each side, under the
	// surface) and a river's banks (6 m each side, from under the water's edge out to the ground: roadmap R6). Each ribbon
	// has UVs along it (a unit every 10 m) and across it (0 to 1), for a material that wants them.
	constexpr int32 Surfaces = 5;
	constexpr int32 Shoulder = Surfaces, Bank = Surfaces + 1, Kinds = Surfaces + 2;
	constexpr double ShoulderM = 1.5, BankM = 6.0, BankIntoWaterM = 1.0;  // game values
	TArray<FVector> Verts[Kinds];
	TArray<int32> Tris[Kinds];
	TArray<FVector> Normals[Kinds];
	TArray<FVector2D> Uvs[Kinds];
	// A ribbon along the strip's points from Inner to Outer metres right of its line (negative: left), at the heights
	// HeightAt gives for each edge.
	const auto Ribbon = [&](const simcore::MapStrip& S, int32 K, double Inner, double Outer, const TFunctionRef<double(const simcore::Vec3&, double, double, bool)>& HeightAt)
	{
		const int32 N = static_cast<int32>(S.points.size());
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(S.points[N / 2].x, S.points[N / 2].y, Ae, An, Au);
		const FVector Up = DirToUnreal(Au);
		const int32 Base = Verts[K].Num();
		double Along = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const simcore::Vec3& P = S.points[i];
			const simcore::Vec3& A = S.points[FMath::Max(i - 1, 0)];
			const simcore::Vec3& B = S.points[FMath::Min(i + 1, N - 1)];
			if (i > 0)
			{
				Along += FMath::Sqrt(FMath::Square(P.x - S.points[i - 1].x) + FMath::Square(P.y - S.points[i - 1].y));
			}
			const double Dx = B.x - A.x, Dy = B.y - A.y, L = FMath::Max(FMath::Sqrt(Dx * Dx + Dy * Dy), 1e-6);
			const double Re = Dy / L, Rn = -Dx / L;  // to the right of the line
			const double Lo = FMath::Min(Inner, Outer), Hi = FMath::Max(Inner, Outer);
			const double El = P.x + Re * Lo, Nl = P.y + Rn * Lo, Er = P.x + Re * Hi, Nr = P.y + Rn * Hi;
			Verts[K].Add(ToUnreal(Link.GroundPoint(El, Nl, HeightAt(P, El, Nl, Lo == Inner))));
			Verts[K].Add(ToUnreal(Link.GroundPoint(Er, Nr, HeightAt(P, Er, Nr, Hi == Inner))));
			Normals[K].Add(Up);
			Normals[K].Add(Up);
			Uvs[K].Add(FVector2D(Along / 10.0, 0.0));
			Uvs[K].Add(FVector2D(Along / 10.0, 1.0));
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 L0 = Base + 2 * i, R0 = L0 + 1, L1 = L0 + 2, R1 = L0 + 3;
			Tris[K].Append({L0, R0, L1, R0, R1, L1});  // facing up, as the ground's
		}
	};
	const simcore::MapData& M = *Map;
	for (const simcore::MapStrip& S : M.Layout().strips)
	{
		const int32 K = static_cast<int32>(S.surface);
		if (S.points.size() < 2)
		{
			continue;
		}
		const double Half = 0.5 * S.width_m;
		const auto OnStrip = [](const simcore::Vec3& P, double, double, bool) { return P.z; };
		Ribbon(S, K, -Half, Half, OnStrip);
		if (S.surface == simcore::MapSurface::Water)
		{
			// The bank: its inner edge under the water's edge, its outer edge on the ground.
			const auto BankAt = [&M](const simcore::Vec3& P, double E, double N, bool bInner)
			{
				return bInner ? P.z - 0.2 : M.GroundHeight(E, N) + 0.05;
			};
			Ribbon(S, Bank, Half - BankIntoWaterM, Half + BankM, BankAt);
			Ribbon(S, Bank, -Half + BankIntoWaterM, -Half - BankM, BankAt);
		}
		else if (S.surface != simcore::MapSurface::Track)
		{
			const auto ShoulderAt = [](const simcore::Vec3& P, double, double, bool) { return P.z - 0.06; };  // under the surface
			Ribbon(S, Shoulder, -Half - ShoulderM, Half + ShoulderM, ShoulderAt);
		}
	}
	const TArray<FLinearColor> NoColours;
	const TArray<FProcMeshTangent> NoTangents;
	int32 Section = 0;
	for (int32 K = 0; K < Kinds; ++K)
	{
		if (Tris[K].Num() == 0)
		{
			continue;
		}
		const FLinearColor Colour = K == Shoulder ? FLinearColor(0.30f, 0.28f, 0.23f)
								  : K == Bank	  ? FLinearColor(0.17f, 0.19f, 0.10f)
												  : SurfaceColour(static_cast<simcore::MapSurface>(K));
		Strips->CreateMeshSection_LinearColor(Section, Verts[K], Tris[K], Normals[K], Uvs[K], NoColours, NoTangents, false);
		Strips->SetMaterial(Section, ColourMaterial(Colour));
		++Section;
	}
}

void ASimMapActor::BuildBoxes(const simcore::MapFrameLink& Link)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	constexpr int32 Kinds = static_cast<int32>(simcore::MapPart::Count);
	TArray<FTransform> Instances[Kinds];
	// Houses, barns, flats and halls as their meshes (roadmap R6, FSimMapAssets): by part key and variant.
	struct FDressed
	{
		FString Part;
		int32 Variant = 0;
		simcore::MapPart Kind = simcore::MapPart::House;
		TArray<FTransform> Xf;
	};
	TMap<FString, FDressed> Dressed;
	TArray<FString> DressedOrder;  // the order met, so the components are made the same way each time
	const int32 HouseVariants = DressRules ? DressRules->house_variants : simcore::scenery::kHouseVariants;
	for (const simcore::MapBox& B : Map->Layout().boxes)
	{
		if (B.part == simcore::MapPart::Canopy || B.part == simcore::MapPart::TreeRow)
		{
			continue;  // the woods, tree rows and groves are trees and masses (BuildTrees)
		}
		if (B.part == simcore::MapPart::Marsh)
		{
			continue;  // the marsh is the ground's own (its land cover's colour and its layer's texture), not a slab laid over it
		}
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(B.east_m, B.north_m, Ae, An, Au);
		const double S = FMath::Sin(B.heading_rad), C = FMath::Cos(B.heading_rad);
		const FVector Forward = DirToUnreal(Ae * S + An * C);
		const FVector Up = DirToUnreal(Au);
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
		const FVector At = ToUnreal(Link.GroundPoint(B.east_m, B.north_m, B.base_m + 0.5 * B.height_m));
		const bool bRound = simcore::MapPartIsRound(B.part);
		const FVector Scale(B.length_m, bRound ? B.length_m : B.width_m, B.height_m);  // the engine's cube and cylinder are 1 m
		int Variant = 0;
		const std::string Part = simcore::scenery::BuildingPart(B, Variant, HouseVariants);
		if (!Part.empty())
		{
			// The part's mesh fills the same unit cube as the engine's (scenery/map_meshes.hpp), so the same transform.
			const FString Key = FString::Printf(TEXT("%s#%d"), UTF8_TO_TCHAR(Part.c_str()), Variant);
			FDressed* Found = Dressed.Find(Key);
			if (Found == nullptr)
			{
				Found = &Dressed.Add(Key);
				Found->Part = UTF8_TO_TCHAR(Part.c_str());
				Found->Variant = Variant;
				Found->Kind = B.part;
				DressedOrder.Add(Key);
			}
			Found->Xf.Add(FTransform(Rot, At, Scale));
			continue;
		}
		Instances[static_cast<int32>(B.part)].Add(FTransform(Rot, At, Scale));
	}
	for (const FString& Key : DressedOrder)
	{
		FDressed& Dr = Dressed[Key];
		FTransform Fix;
		UStaticMesh* Mesh = FSimMapAssets::MeshFor(Dr.Part, Dr.Variant, Fix);
		if (Mesh == nullptr)
		{
			Instances[static_cast<int32>(Dr.Kind)].Append(Dr.Xf);  // no mesh: the box, as before
			continue;
		}
		TArray<FTransform> Xf;
		Xf.Reserve(Dr.Xf.Num());
		for (const FTransform& T : Dr.Xf)
		{
			Xf.Add(Fix * T);  // the asset's own fix into the part's frame first, then the box's
		}
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this, FName(*FString::Printf(TEXT("Map_%s_%d"), *Dr.Part, Dr.Variant)));
		H->SetupAttachment(Root);
		H->SetStaticMesh(Mesh);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCastShadow(true);
		H->RegisterComponent();
		H->AddInstances(Xf, false, true);
		Parts.Add(H);
		BoxCount += Xf.Num();
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
		if (Part == simcore::MapPart::Bridge)
		{
			// A bridge is drawn as its deck, parapets and piers (BuildBridges); its box stays, unseen, for the traces.
			H->SetVisibility(false);
			H->SetCastShadow(false);
		}
		H->RegisterComponent();
		H->AddInstances(Instances[K], false, true);
		Parts.Add(H);
		BoxCount += Instances[K].Num();
	}
}

void ASimMapActor::BuildTrees(const simcore::MapFrameLink& Link)
{
	// The woods (roadmap R6, simcore/scenery/map_dressing.hpp): trees where simcore plants them (the map proper's woods'
	// open sides, the detail areas' woods whole), one instanced component per kind; the rest of each wood a mass, one per
	// variant. No collision (nothing drives into a wood; the sight lines are simcore's), the trees cast shadows.
	if (!Dressing)
	{
		return;
	}
	const simcore::scenery::MapDressing& Dr = *Dressing;
	const auto Add = [&](const FString& Part, int32 Variant, const TArray<FTransform>& Xf, bool bShadow, float CullM)
	{
		if (Xf.Num() == 0)
		{
			return;
		}
		FTransform Fix;
		UStaticMesh* Mesh = FSimMapAssets::MeshFor(Part, Variant, Fix);
		if (Mesh == nullptr)
		{
			return;
		}
		TArray<FTransform> Placed;
		Placed.Reserve(Xf.Num());
		for (const FTransform& T : Xf)
		{
			Placed.Add(Fix * T);
		}
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this, FName(*FString::Printf(TEXT("Map_%s_%d"), *Part, Variant)));
		H->SetupAttachment(Root);
		H->SetStaticMesh(Mesh);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCastShadow(bShadow);
		if (CullM > 0.0f)
		{
			H->SetCullDistances(0, static_cast<int32>(CullM * 100.0f));
		}
		H->RegisterComponent();
		H->AddInstances(Placed, false, true);
		Parts.Add(H);
		BoxCount += Placed.Num();
	};
	// The masses: the box the wood's block was, inset, on the engine cube's frame.
	TArray<FTransform> Masses[simcore::scenery::kCanopyMassVariants];
	for (const simcore::scenery::CanopyMass& C : Dr.masses)
	{
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(C.east_m, C.north_m, Ae, An, Au);
		const FVector Forward = DirToUnreal(Ae * FMath::Sin(C.heading_rad) + An * FMath::Cos(C.heading_rad));
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Forward, DirToUnreal(Au)).Rotator();
		const FVector At = ToUnreal(Link.GroundPoint(C.east_m, C.north_m, C.base_m + 0.5 * C.height_m));
		Masses[C.variant % simcore::scenery::kCanopyMassVariants].Add(FTransform(Rot, At, FVector(C.length_m, C.width_m, C.height_m)));
	}
	for (int32 V = 0; V < simcore::scenery::kCanopyMassVariants; ++V)
	{
		Add(TEXT("canopy_mass"), V, Masses[V], false, 0.0f);
	}
	if (!bDrawTrees)
	{
		return;
	}
	// The trees: base on the ground (simcore's, 0.3 m into it), turned by their own yaw, scaled (crown, crown, height)
	// from the unit tree (scenery/map_meshes.hpp TreeMesh).
	constexpr int32 TreeKinds = static_cast<int32>(simcore::scenery::TreeKind::Count);
	TArray<FTransform> Trees[TreeKinds];
	for (const simcore::scenery::TreeInstance& T : Dr.trees)
	{
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(T.east_m, T.north_m, Ae, An, Au);
		const FVector Forward = DirToUnreal(Ae * FMath::Sin(T.yaw_rad) + An * FMath::Cos(T.yaw_rad));
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Forward, DirToUnreal(Au)).Rotator();
		const FVector At = ToUnreal(Link.GroundPoint(T.east_m, T.north_m, T.base_m));
		Trees[T.kind % TreeKinds].Add(FTransform(Rot, At, FVector(T.crown_m, T.crown_m, T.height_m)));
	}
	for (int32 K = 0; K < TreeKinds; ++K)
	{
		Add(FString(TEXT("tree_")) + UTF8_TO_TCHAR(simcore::scenery::TreeKindName(static_cast<simcore::scenery::TreeKind>(K))), 0, Trees[K], true,
			TreeCullDistanceM);
	}
}

void ASimMapActor::BuildBridges(const simcore::MapFrameLink& Link)
{
	// Each crossing's bridge (roadmap R6, simcore/scenery/map_meshes.hpp BridgeMesh): the deck from end to end at the
	// crossing's height, its parapets and its piers down into the river's bed, in one mesh of the map's vertex colours.
	// The deck's box (BuildBoxes) stays, unseen, for the traces that find the deck.
	const simcore::MapData& M = *Map;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colours;
	for (const simcore::MapCrossing& C : M.Layout().crossings)
	{
		double Width = 8.0;
		for (const simcore::MapBox& B : M.Layout().boxes)
		{
			if (B.part == simcore::MapPart::Bridge && FMath::Abs(B.east_m - C.at.east_m) < 0.5 && FMath::Abs(B.north_m - C.at.north_m) < 0.5)
			{
				Width = B.width_m;
				break;
			}
		}
		const double Se = FMath::Sin(C.heading_rad), Sn = FMath::Cos(C.heading_rad);
		const simcore::Vec3 EndA(C.at.east_m - Se * C.half_length_m, C.at.north_m - Sn * C.half_length_m, C.deck_m);
		const simcore::Vec3 EndB(C.at.east_m + Se * C.half_length_m, C.at.north_m + Sn * C.half_length_m, C.deck_m);
		const simcore::scenery::TriMesh T = simcore::scenery::BridgeMesh(EndA, EndB, M.GroundHeight(C.at.east_m, C.at.north_m), Width);
		if (T.Empty())
		{
			continue;
		}
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(C.at.east_m, C.at.north_m, Ae, An, Au);
		const int32 Base = Verts.Num();
		for (std::size_t i = 0; i < T.positions.size(); ++i)
		{
			const simcore::Vec3& P = T.positions[i];
			const simcore::Vec3& Nrm = T.normals[i];
			Verts.Add(ToUnreal(Link.GroundPoint(EndA.x + P.x, EndA.y + P.y, EndA.z + P.z)));
			Normals.Add(DirToUnreal(Ae * Nrm.x + An * Nrm.y + Au * Nrm.z));
			Colours.Add(Linear(T.colours[i]));
		}
		// simcore's winding (outward by (b - a) x (c - a) in east, north, up) is the ground's here: east and north become
		// Unreal's Y and X, as the ground's triangles {A, B east, C north} face up.
		for (const std::uint32_t I : T.indices)
		{
			Tris.Add(Base + static_cast<int32>(I));
		}
	}
	if (Tris.Num() == 0)
	{
		return;
	}
	Bridges = NewObject<UProceduralMeshComponent>(this, TEXT("MapBridges"));
	Bridges->SetupAttachment(Root);
	Bridges->SetCastShadow(true);
	Bridges->RegisterComponent();
	Bridges->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	Bridges->SetMaterial(0, FSimMapAssets::Material());
	Bridges->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASimMapActor::BuildCountry(const FVector& FrameOriginLonLatAlt)
{
	const simcore::CountryMap& Cm = *Country;
	const simcore::CountryDef& Cd = Cm.Def();
	const simcore::MapFrameLink Link(Map->Def().origin,
		simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z));
	const simcore::MapFrameLink ToMap(Map->Def().origin, Cd.ground.origin);
	const simcore::MapDef& D = Map->Def();
	// Whether a point of the country lies under the map proper (which draws its own lines and buildings there).
	const auto UnderMap = [&](double E, double N, double Margin)
	{
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		return P.east_m > D.west_m - Margin && P.east_m < D.east_m + Margin && P.north_m > D.south_m - Margin && P.north_m < D.north_m + Margin;
	};
	// The country's ground is drawn with the map's (BuildGround: the country's lattices are levels of the map's ground,
	// roadmap R5). Here what stands on it: its lines, lakes and towns, each on the ground the sim samples there
	// (MapData::GroundHeight), placed down the frame's up (MapFrameLink::GroundPoint).
	const auto GroundOf = [&](double E, double N)
	{
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		return Map->GroundHeight(P.east_m, P.north_m);
	};
	const auto PointOf = [&](double E, double N, double H)
	{
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		return ToUnreal(Link.GroundPoint(P.east_m, P.north_m, H));
	};
	// The spacing of the ground drawn at a point: a line is resampled at a quarter of it, so it follows the triangles.
	const auto SpacingAt = [&](double E, double N)
	{
		const simcore::MapPoint P = ToMap.FrameToMap(E, N);
		const int32 Level = Map->LevelAt(P.east_m, P.north_m);
		return Level < 0 ? 4000.0 : Map->DrawnLevels()[static_cast<std::size_t>(Level)]->spacing_m;
	};
	constexpr double Lift = 0.15;  // a line's ribbon over the ground (a game value)
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
	const auto Ribbon = [&](const std::vector<simcore::MapPoint>& Points, double Width, FKind& K, bool bClosed)
	{
		// Resampled at a quarter of the ground's spacing (at most every 400 m), each point level across at the highest of
		// the ground under its middle and its two edges; a run is broken where it passes under the map.
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
			const double Every = FMath::Min(400.0, 0.25 * SpacingAt(A.east_m, A.north_m));
			const int32 Steps = FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(L / Every)));
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
				const double El = Q.east_m - Re * Half, Nl = Q.north_m - Rn * Half, Er = Q.east_m + Re * Half, Nr = Q.north_m + Rn * Half;
				const double H = FMath::Max3(GroundOf(Q.east_m, Q.north_m), GroundOf(El, Nl), GroundOf(Er, Nr)) + Lift;
				simcore::Vec3 Ae, An, Au;
				Link.AxesAt(Q.east_m, Q.north_m, Ae, An, Au);
				K.Verts.Add(PointOf(El, Nl, H));
				K.Verts.Add(PointOf(Er, Nr, H));
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
			// Not under the map proper, nor in a detail area beyond it (the map lays the country's lines there as its own
			// strips and bridges, roadmap R6).
			const simcore::MapPoint Pm = ToMap.FrameToMap(P[static_cast<std::size_t>(i)].east_m, P[static_cast<std::size_t>(i)].north_m);
			const bool bOut = !UnderMap(P[static_cast<std::size_t>(i)].east_m, P[static_cast<std::size_t>(i)].north_m, 0.0) && !D.InDetail(Pm.east_m, Pm.north_m);
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
		Ribbon(L.points, FMath::Max(L.width_m, 60.0), Kinds[0], false);
	}
	for (const simcore::MapLineDef& L : Cd.ground.roads)
	{
		const bool bMain = L.line_class == "main";
		Ribbon(L.points, bMain ? 30.0 : 18.0, Kinds[bMain ? 1 : 2], false);
	}
	for (const simcore::MapLineDef& L : Cd.ground.railways)
	{
		Ribbon(L.points, 14.0, Kinds[3], false);
	}
	for (const simcore::MapLineDef& L : Cd.pipelines)
	{
		Ribbon(L.points, 8.0, Kinds[4], false);  // the pipeline's cleared strip
	}
	Ribbon(Cd.outline, 25.0, Kinds[4], true);  // the border
	// The lakes: their surface as a sheet of 250 m cells at their level, not where the map draws its own. Under it the
	// ground lies below the level (NaturalGround deepens it from the shore), drawn under the water: no lift.
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
				// Not under the map proper, nor in a detail area (roadmap R6: the map draws its own water there; no country's
				// lake reaches one today, and one that does must be imported as its rivers are, MapData::ImportedLines).
				const simcore::MapPoint LakeAt = ToMap.FrameToMap(Ce, Cn);
				if (!simcore::mapgeo::InPolygon(Lake.points, Ce, Cn) || UnderMap(Ce, Cn, 0.0) || D.InDetail(LakeAt.east_m, LakeAt.north_m))
				{
					continue;
				}
				const double H = Lake.level_m;
				simcore::Vec3 Ae, An, Au;
				Link.AxesAt(Ce, Cn, Ae, An, Au);
				FKind& K = Kinds[0];
				const int32 A = K.Verts.Num(), B = A + 1, Cc = A + 2, Dd = A + 3;
				K.Verts.Add(PointOf(E, N, H));
				K.Verts.Add(PointOf(E + Cell, N, H));
				K.Verts.Add(PointOf(E, N + Cell, H));
				K.Verts.Add(PointOf(E + Cell, N + Cell, H));
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

	// The towns beyond the map: blocks on the town's ground (tinted in BuildGround: no patch laid over it, R5), the
	// capital's and the cities' taller, placed by a hash of the town's id so they stand where they stood. Each stands
	// on the lowest corner of its footprint, so none floats on a slope.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TArray<FTransform> Houses, Blocks;
	for (const simcore::CountrySettlementDef& S : Cd.settlements)
	{
		if (UnderMap(S.centre.east_m, S.centre.north_m, S.radius_m))
		{
			continue;
		}
		// A town the map lays out itself in a detail area (the capital, roadmap R6) is drawn with the map's boxes.
		bool bLaidByMap = false;
		for (const simcore::MapSettlementDef& Laid : Map->Def().detail_settlements)
		{
			bLaidByMap = bLaidByMap || Laid.id == S.id;
		}
		if (bLaidByMap)
		{
			continue;
		}
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(S.centre.east_m, S.centre.north_m, Ae, An, Au);
		const FVector Up = DirToUnreal(Au);
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
			const double Turn = U3 * 3.0;  // the box's long side, from north
			double Base = 1e18;
			for (const FVector2D& Corner : {FVector2D(0.5, 0.5), FVector2D(0.5, -0.5), FVector2D(-0.5, 0.5), FVector2D(-0.5, -0.5)})
			{
				const double Along = Corner.X * Size.X, Across = Corner.Y * Size.Y;
				const double Ec = E + Along * FMath::Sin(Turn) + Across * FMath::Cos(Turn), Nc = N + Along * FMath::Cos(Turn) - Across * FMath::Sin(Turn);
				Base = FMath::Min(Base, GroundOf(Ec, Nc));
			}
			const FRotator Rot = FRotationMatrix::MakeFromXZ(DirToUnreal(Ae * FMath::Sin(Turn) + An * FMath::Cos(Turn)), Up).Rotator();
			(bBig ? Blocks : Houses).Add(FTransform(Rot, PointOf(E, N, Base + 0.5 * Hb), Size));
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
	const simcore::MapFrameLink ToMap(Map->Def().origin, Th->origin);
	const simcore::MapFrameLink MapLink(Map->Def().origin, FrameOrigin);
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
	// One basic model on the ground at (E, N) of the theatre's frame, facing a horizontal direction there: on the ground
	// the sim samples and the map draws (roadmap R5), down the frame's up.
	auto PlaceModel = [&](const std::string& Model, double E, double N, const simcore::Vec3& Facing, bool bWrecked)
	{
		const simcore::MapPoint Pm = ToMap.FrameToMap(E, N);
		const double H = Map->GroundHeight(Pm.east_m, Pm.north_m);
		simcore::Vec3 Ae, An, Au;
		Link.AxesAt(E, N, Ae, An, Au);
		const simcore::Vec3 Fwd = Ae * Facing.x + An * Facing.y;
		USceneComponent* Holder = NewObject<USceneComponent>(this);
		Holder->SetupAttachment(Root);
		Holder->SetRelativeLocationAndRotation(ToUnreal(MapLink.GroundPoint(Pm.east_m, Pm.north_m, H)),
			FRotationMatrix::MakeFromXZ(DirToUnreal(Fwd), DirToUnreal(Au)).Rotator());
		Holder->RegisterComponent();
		AssetParts.Add(Holder);
		TArray<UStaticMeshComponent*> Built;
		FSimBasicModels::Build(UTF8_TO_TCHAR(Model.c_str()), Holder, &Built, bWrecked ? &Wreck : nullptr);
		for (UStaticMeshComponent* Part : Built)
		{
			AssetParts.Add(Part);
		}
		++AssetCount;
		return Holder->GetComponentLocation();
	};
	TArray<FSimViewPlace> Places;  // each object drawn, for the views (F12): the theatre is our own side's
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
		FVector First = FVector::ZeroVector;
		for (const std::pair<std::string, simcore::Vec3>& V : Site)
		{
			// Each faces out from its site's centre (launchers round their radar), the centre's north.
			const double Len = V.second.HorizontalLength();
			const simcore::Vec3 Facing = Len > 1.0 ? simcore::Vec3{V.second.x / Len, V.second.y / Len, 0.0} : simcore::Vec3{0.0, 1.0, 0.0};
			const FVector At = PlaceModel(V.first, Pos.x + V.second.x, Pos.y + V.second.y, Facing, Function <= 0.05);
			First = &V == &Site.front() ? At : First;
		}
		if (!Site.empty())
		{
			FSimViewPlace Place;
			Place.Name = UTF8_TO_TCHAR(O.name.c_str());
			Place.Location = First;
			Place.SizeM = 60.0;
			Places.Add(Place);
		}
	}
	if (USimWorldSubsystem* Views = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Views->SetViewPlaces(TEXT("Map"), MoveTemp(Places));
	}
	// The chain of command's places (roadmap P7): the posts, offices, works and villages that are not theatre objects; and
	// with a campaign running, each post's holder where he is at its hour (the people move with their routines).
	const simcore::command::InfoNet* Net = AirWar != nullptr ? AirWar->GetInfoNet() : nullptr;
	const simcore::command::ChainData* Chain = Net != nullptr ? &Net->Data() : nullptr;
	static TSharedPtr<simcore::command::ChainData> FileChain;
	if (Chain == nullptr)
	{
		if (!FileChain.IsValid())
		{
			FileChain = MakeShared<simcore::command::ChainData>();
			std::string Error;
			if (!simcore::command::LoadChainData(TCHAR_TO_UTF8(*FindDataDirectory()), "command/chain_pannonia.json", *FileChain, &Error))
			{
				UE_LOG(LogSAM3D, Warning, TEXT("Map: the chain of command does not load (%s): its places not drawn"), UTF8_TO_TCHAR(Error.c_str()));
			}
		}
		Chain = FileChain->places.empty() ? nullptr : FileChain.Get();
	}
	if (Chain != nullptr)
	{
		std::vector<simcore::command::DistrictPiece> Pieces = simcore::command::DistrictPlaces(*Chain, *Lib);
		if (Net != nullptr)
		{
			const std::vector<simcore::command::DistrictPiece> People = simcore::command::DistrictPeople(*Net, Net->Now());
			Pieces.insert(Pieces.end(), People.begin(), People.end());
		}
		for (const simcore::command::DistrictPiece& P : Pieces)
		{
			PlaceModel(P.model, P.pos.x, P.pos.y, P.facing, false);
		}
	}
}
