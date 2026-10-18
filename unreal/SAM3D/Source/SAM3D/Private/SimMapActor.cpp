#include "SimMapActor.h"

#include "Camera/CameraActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "SAM3D.h"
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
	FLinearColor GroundColour(const simcore::MapData& Map, double E, double N, double H)
	{
		static const FLinearColor Fields[] = {
			FLinearColor(0.20f, 0.26f, 0.10f), FLinearColor(0.26f, 0.28f, 0.12f), FLinearColor(0.30f, 0.27f, 0.15f),
			FLinearColor(0.17f, 0.23f, 0.09f), FLinearColor(0.33f, 0.30f, 0.19f)};
		const uint64 Seed = Map.Def().seed ^ 0xF1E1Dull;
		const double U = simcore::noise::Value(Seed, E, N, 450.0);
		FLinearColor C = Fields[FMath::Clamp(static_cast<int32>(U * 5.0), 0, 4)];
		const double Rise = FMath::Clamp((H - Map.Def().base_m) / 60.0, -0.3, 1.0);
		C = FMath::Lerp(C, FLinearColor(0.30f, 0.27f, 0.18f), static_cast<float>(FMath::Max(0.0, Rise) * 0.4));
		if (Rise < 0.0)
		{
			C = FMath::Lerp(C, FLinearColor(0.13f, 0.20f, 0.08f), static_cast<float>(-Rise * 2.0));
		}
		return C;
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
	const double T1 = FPlatformTime::Seconds();
	Clear();
	const simcore::MapFrameLink Link(Map->Def().origin,
		simcore::GeoPoint::FromDegrees(FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, FrameOriginLonLatAlt.Z));
	BuildGround(Link);
	BuildStrips(Link);
	BuildBoxes(Link);
	BuiltFor = FrameOriginLonLatAlt;
	UE_LOG(LogSAM3D, Log, TEXT("Map %s drawn for the frame at %.5f N %.5f E: %d boxes; laid out in %.2f s, meshes %.2f s"),
		UTF8_TO_TCHAR(Map->Def().map_id.c_str()), FrameOriginLonLatAlt.Y, FrameOriginLonLatAlt.X, BoxCount, T1 - T0,
		FPlatformTime::Seconds() - T1);
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
				Colours.Add(GroundColour(M, E, N, H));
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
