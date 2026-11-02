#include "Misc/AutomationTest.h"
#include "SimMapActor.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// One ground (roadmap R5): the map actor's drawn vertices are the sim's ground points put through EnuToUnreal, on every
// drawn level of the test map, and the map's triangles agree with the frame's ground (simcore::DrawnGroundAgrees).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimMapGroundAgreesTest, "SAM3D.Map.GroundAgrees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimMapGroundAgreesTest::RunTest(const FString& Parameters)
{
	std::string Error;
	const std::shared_ptr<const simcore::MapData> Map =
		simcore::LoadMapCached(TCHAR_TO_UTF8(*ASimMapActor::FindDataDirectory()), "maps/kesmark_range.json", &Error);
	TestTrue(FString::Printf(TEXT("the test map loads %s"), UTF8_TO_TCHAR(Error.c_str())), Map != nullptr);
	if (!Map)
	{
		return true;
	}
	// The dynamic campaign testbed's frame (data/scenarios/dynamic_campaign_test.json), as the level draws it.
	const simcore::MapFrameLink Link(Map->Def().origin, simcore::GeoPoint::FromDegrees(47.420689, 20.177063, 108.9));
	const std::vector<const simcore::MapGrid*> Levels = Map->DrawnLevels();
	TestEqual(TEXT("four drawn levels: core, far, country, beyond"), static_cast<int32>(Levels.size()), 4);
	double Worst = 0.0;
	for (const simcore::MapGrid* G : Levels)
	{
		const int32 StepC = FMath::Max(1, G->cols / 17), StepR = FMath::Max(1, G->rows / 17);
		for (int32 R = 0; R < G->rows; R += StepR)
		{
			for (int32 C = 0; C < G->cols; C += StepC)
			{
				const FVector Drawn = ASimMapActor::GroundVertex(Link, *G, C, R);
				const FVector Sim = USimWorldSubsystem::EnuToUnreal(Link.GroundPoint(G->EastOf(C), G->NorthOf(R), G->At(C, R)));
				Worst = FMath::Max(Worst, FVector::Dist(Drawn, Sim));
			}
		}
	}
	TestTrue(FString::Printf(TEXT("drawn vertices on the sim's ground points (worst %.3f cm)"), Worst), Worst < 0.1);
	std::string Why;
	TestTrue(FString::Printf(TEXT("the drawn ground agrees with the frame's: %s"), UTF8_TO_TCHAR(Why.c_str())),
		simcore::DrawnGroundAgrees(*Map, Link, 400, &Why));
	return true;
}

#endif
