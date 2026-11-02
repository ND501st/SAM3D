#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
#include "Misc/ScopeExit.h"
#include "SimMapActor.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// One ground (roadmap R5): the map actor built for the dynamic campaign testbed's frame draws, on every level of the
// test map, the ground the sim samples. Judged on the ground tiles' meshes as built (ASimMapActor::DrawnGroundAt reads
// their sections' vertices and triangles, nothing simcore computes), so a wrong vertex index, a cell split the other
// way, a skipped cell or a coarse cell drawn over a finer level fails; and the actor's GroundAt, which places things,
// is on that mesh.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimMapGroundAgreesTest, "SAM3D.Map.GroundAgrees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimMapGroundAgreesTest::RunTest(const FString& Parameters)
{
	// A world of its own, not begun (the map actor's BeginPlay would draw for no scenario): the actor only builds.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SAM3DMapGroundAgrees"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	ASimMapActor* MapActor = World->SpawnActor<ASimMapActor>();
	if (!TestNotNull(TEXT("a map actor"), MapActor))
	{
		return false;
	}
	MapActor->CountryFile = TEXT("");  // the country's lines and towns are not ground; its lattices are the map's own
	MapActor->bDrawAssets = false;
	MapActor->bCollision = false;
	// The dynamic campaign testbed's frame (data/scenarios/dynamic_campaign_test.json), 68 km off the map's origin.
	const double LatDeg = 47.420689, LonDeg = 20.177063, AltM = 108.9;
	FString Error;
	const bool bBuilt = MapActor->BuildForFrame(FVector(LonDeg, LatDeg, AltM), Error);
	if (!TestTrue(FString::Printf(TEXT("the map is drawn for the testbed's frame %s"), *Error), bBuilt))
	{
		return false;
	}
	const simcore::MapData* Map = MapActor->GetMapData();
	if (!TestNotNull(TEXT("the map's data"), Map))
	{
		return false;
	}
	const simcore::MapFrameLink Link(Map->Def().origin, simcore::GeoPoint::FromDegrees(LatDeg, LonDeg, AltM));
	const std::vector<const simcore::MapGrid*> Levels = Map->DrawnLevels();
	TestEqual(TEXT("four drawn levels: core, far, country, beyond"), static_cast<int32>(Levels.size()), 4);
	FRandomStream Random(0x52355);
	for (std::size_t Li = 0; Li < Levels.size(); ++Li)
	{
		const simcore::MapGrid& G = *Levels[Li];
		const int32 Level = static_cast<int32>(Li);
		// A triangle is flat, the frame's ground bends between its corners: the sim's point stands over the face by up
		// to the chord's sag, s^2 / 4R (1 cm at 500 m, 0.6 m on the 4 km land beyond), as DrawnGroundAgrees allows.
		const double SagCm = 100.0 * (1.1 * G.spacing_m * G.spacing_m / (4.0 * 6371000.0) + 0.01);
		double WorstCm = 0.0, WorstPlacedCm = 0.0;
		int32 Done = 0, Missing = 0;
		for (int32 Tried = 0; Done < 150 && Tried < 15000; ++Tried)
		{
			const double E = G.west_m + Random.FRand() * G.spacing_m * (G.cols - 1);
			const double N = G.south_m + Random.FRand() * G.spacing_m * (G.rows - 1);
			if (Map->LevelAt(E, N) != static_cast<int>(Li))
			{
				continue;
			}
			++Done;
			// The sim's ground point there (MapData::GroundHeight on its level's triangle, put in the frame).
			const FVector Sim = USimWorldSubsystem::EnuToUnreal(Link.GroundPoint(E, N, Map->GroundHeight(E, N)));
			FVector Drawn;
			if (!MapActor->DrawnGroundAt(Sim, Drawn))
			{
				++Missing;
				continue;
			}
			WorstCm = FMath::Max(WorstCm, FMath::Abs(Sim.Z - Drawn.Z));
			FVector Placed;
			if (MapActor->GroundAt(Sim, Placed))
			{
				WorstPlacedCm = FMath::Max(WorstPlacedCm, FMath::Abs(Placed.Z - Drawn.Z));
			}
		}
		TestEqual(FString::Printf(TEXT("level %d: 150 points where it is the ground"), Level), Done, 150);
		TestEqual(FString::Printf(TEXT("level %d: drawn ground under every one"), Level), Missing, 0);
		TestTrue(FString::Printf(TEXT("level %d: the drawn mesh is the sim's ground (worst %.2f cm, sag %.2f cm)"), Level, WorstCm, SagCm),
			WorstCm <= SagCm);
		TestTrue(FString::Printf(TEXT("level %d: GroundAt places on the drawn mesh (worst %.2f cm)"), Level, WorstPlacedCm),
			WorstPlacedCm <= SagCm);
	}
	std::string Why;
	const bool bOnce = simcore::NoGroundDrawnTwice(*Map, &Why);
	TestTrue(FString::Printf(TEXT("no ground drawn twice %s"), UTF8_TO_TCHAR(Why.c_str())), bOnce);
	return true;
}

#endif
