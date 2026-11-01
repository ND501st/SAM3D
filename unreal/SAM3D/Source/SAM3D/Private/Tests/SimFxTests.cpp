#include "Misc/AutomationTest.h"
#include "SimFxImpl.h"
#include "SimFxLogic.h"

// The weapons effects' plain logic (docs/fx/README.md 4.1, 5): detail by projected size and the views, the sprite
// budget, ribbon decimation, the feed's cursor and the wrecks' falls. No world needed.

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A view at the origin looking north (Unreal +X). */
	FSimFxView SimFxTestView(double FovDeg, double WidthPx)
	{
		FSimFxView V;
		V.Set(FVector::ZeroVector, FRotator::ZeroRotator, FovDeg, WidthPx);
		return V;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxLodTierTest, "SAM3D.Fx.LodTier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxLodTierTest::RunTest(const FString& Parameters)
{
	// size / (2 d tan(fov / 2)) * width: 10 m at 1 km on a 90 deg, 1280 px view is 6.4 px.
	TestEqual(TEXT("10 m at 1 km, 90 deg, 1280 px"), SimFxLogic::ProjectedPx(10.0, 1000.0, 90.0, 1280.0), 6.4, 1e-9);
	TestEqual(TEXT("100 m at 1 km, 90 deg, 1280 px"), SimFxLogic::ProjectedPx(100.0, 1000.0, 90.0, 1280.0), 64.0, 1e-9);
	// The TV sight's narrow lens: 30 m at 3 km on 1.44 deg, 640 px.
	const double Narrow = 30.0 / (2.0 * 3000.0 * FMath::Tan(FMath::DegreesToRadians(0.72))) * 640.0;
	TestEqual(TEXT("30 m at 3 km, 1.44 deg, 640 px"), SimFxLogic::ProjectedPx(30.0, 3000.0, 1.44, 640.0), Narrow, 1e-6);

	FSimFxLod Day;
	TestTrue(TEXT("64 px: full"), SimFxLogic::TierForPx(64.0, 1000.0, Day) == ESimFxTier::Full);
	TestTrue(TEXT("6.4 px: impostor"), SimFxLogic::TierForPx(6.4, 1000.0, Day) == ESimFxTier::Impostor);
	// By day a burst between the speck and the impostor size is a grey speck (LOOK T12), under it nothing.
	TestTrue(TEXT("3.2 px by day: a speck"), SimFxLogic::TierForPx(3.2, 1000.0, Day) == ESimFxTier::Speck);
	TestTrue(TEXT("1.2 px by day: none"), SimFxLogic::TierForPx(1.2, 1000.0, Day) == ESimFxTier::None);
	TestTrue(TEXT("past the range: none"), SimFxLogic::TierForPx(500.0, Day.MaxRangeM + 1.0, Day) == ESimFxTier::None);
	FSimFxLod Hazy;
	Hazy.VisibilityM = 5000.0;
	TestTrue(TEXT("past the visibility: none"), SimFxLogic::TierForPx(500.0, 6000.0, Hazy) == ESimFxTier::None);
	FSimFxLod Night;
	Night.bNight = true;
	TestTrue(TEXT("3.2 px at night: a glint"), SimFxLogic::TierForPx(3.2, 1000.0, Night) == ESimFxTier::Glint);

	// A 30 m effect 3 km ahead: 6.4 px on the 90 deg camera (impostor), 255 px on the 1.44 deg sight (full).
	const FVector Ahead(300000.0, 0.0, 0.0);
	TArray<FSimFxView> Views;
	Views.Add(SimFxTestView(90.0, 1280.0));
	double Px = 0.0;
	TestTrue(TEXT("the camera alone: impostor"), SimFxLogic::TierFor(Views, Ahead, 30.0, Day, &Px) == ESimFxTier::Impostor);
	TestEqual(TEXT("the camera's px"), Px, 6.4, 1e-6);
	Views.Add(SimFxTestView(1.44, 640.0));
	TestTrue(TEXT("with the narrow sight: full"), SimFxLogic::TierFor(Views, Ahead, 30.0, Day, &Px) == ESimFxTier::Full);
	TestEqual(TEXT("the largest px is the sight's"), Px, Narrow, 1e-6);
	// Off the sight's axis by 10 deg: only the camera sees it.
	const FVector Off = FRotator(0.0, 10.0, 0.0).RotateVector(Ahead);
	TestTrue(TEXT("outside the sight: impostor"), SimFxLogic::TierFor(Views, Off, 30.0, Day) == ESimFxTier::Impostor);
	// A sight not set this frame counts no more.
	Views[1].bActive = false;
	TestTrue(TEXT("the sight inactive: impostor"), SimFxLogic::TierFor(Views, Ahead, 30.0, Day) == ESimFxTier::Impostor);
	// Behind every view: nothing.
	TestTrue(TEXT("behind: none"), SimFxLogic::TierFor(Views, -Ahead, 30.0, Day) == ESimFxTier::None);
	TestFalse(TEXT("behind: in no view"), SimFxLogic::InAnyView(Views, -Ahead, 1500.0, Day.Margin));
	TestTrue(TEXT("ahead: in a view"), SimFxLogic::InAnyView(Views, Ahead, 1500.0, Day.Margin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxSpriteBudgetTest, "SAM3D.Fx.SpriteBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxSpriteBudgetTest::RunTest(const FString& Parameters)
{
	FSimFxParticles Particles;
	Particles.SetMax(100);
	FSimFxParticle P;
	P.Life = 1.0f;
	int32 Accepted = 0;
	for (int32 I = 0; I < 100; ++I)
	{
		P.Layer = static_cast<ESimFxLayer>(I % SimFxLayerCount);
		Accepted += Particles.Spawn(P) ? 1 : 0;
	}
	TestEqual(TEXT("the first 100 fit"), Accepted, 100);
	TestEqual(TEXT("no room left"), Particles.Room(), 0);
	TestFalse(TEXT("the 101st is refused"), Particles.Spawn(P));
	for (int32 I = 0; I < 5000; ++I)
	{
		Accepted += Particles.Spawn(P) ? 1 : 0;
	}
	TestEqual(TEXT("5000 more asked, none taken"), Accepted, 100);
	TestEqual(TEXT("the count holds at the budget"), Particles.Num(), 100);
	int32 Held = 0;
	for (int32 L = 0; L < SimFxLayerCount; ++L)
	{
		Held += Particles.Layer(static_cast<ESimFxLayer>(L)).Num();
	}
	TestEqual(TEXT("the layers hold the budget"), Held, 100);
	// They die, and the budget is free again.
	Particles.Update(1.5f);
	TestEqual(TEXT("all dead after their life"), Particles.Num(), 0);
	TestTrue(TEXT("spawning again"), Particles.Spawn(P));
	FSimFxParticle Dead = P;
	Dead.Life = 0.0f;
	TestFalse(TEXT("a particle with no life is refused"), Particles.Spawn(Dead));
	Particles.Clear();
	TestEqual(TEXT("cleared"), Particles.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxRibbonDecimationTest, "SAM3D.Fx.RibbonDecimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxRibbonDecimationTest::RunTest(const FString& Parameters)
{
	TArray<int32> Points;
	for (int32 I = 0; I < 1000; ++I)
	{
		Points.Add(I);
	}
	SimFxLogic::DecimateKeepingEnds(Points, 256);
	TestTrue(TEXT("at most 256 points"), Points.Num() <= 256);
	TestTrue(TEXT("not thinned past half the cap"), Points.Num() >= 128);
	TestEqual(TEXT("the first kept"), Points[0], 0);
	TestEqual(TEXT("the last kept"), Points.Last(), 999);
	bool bOrdered = true;
	for (int32 I = 1; I < Points.Num(); ++I)
	{
		bOrdered &= Points[I] > Points[I - 1];
	}
	TestTrue(TEXT("still in order"), bOrdered);
	// The new end keeps its detail: the last steps are single points.
	TestEqual(TEXT("the newest two adjacent"), Points[Points.Num() - 1] - Points[Points.Num() - 2], 1);
	// Ribbon points themselves, and a short trail untouched.
	TArray<FSimFxRibbonPoint> Trail;
	for (int32 I = 0; I < 600; ++I)
	{
		FSimFxRibbonPoint& R = Trail.AddDefaulted_GetRef();
		R.Pos = FVector(I * 100.0, 0.0, 0.0);
		R.BornT = static_cast<float>(I);
	}
	SimFxLogic::DecimateKeepingEnds(Trail, 256);
	TestTrue(TEXT("a trail: at most 256"), Trail.Num() <= 256);
	TestEqual(TEXT("a trail: first point"), Trail[0].BornT, 0.0f);
	TestEqual(TEXT("a trail: last point"), Trail.Last().BornT, 599.0f);
	TArray<int32> Short = {1, 2, 3};
	SimFxLogic::DecimateKeepingEnds(Short, 256);
	TestEqual(TEXT("a short trail untouched"), Short.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxFeedCursorTest, "SAM3D.Fx.FeedCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxFeedCursorTest::RunTest(const FString& Parameters)
{
	simcore::fx::Feed First;
	simcore::fx::Feed Second;
	for (int32 I = 0; I < 3; ++I)
	{
		First.Push(simcore::fx::Event{});
	}
	FSimFxFeedCursor Cursor;
	TestTrue(TEXT("the first runner: a reset"), Cursor.Sync(&First, First.Last()));
	TestEqual(TEXT("read from the start"), static_cast<int32>(First.Since(Cursor.Seq).size()), 3);
	Cursor.Seq = First.Last();
	First.Push(simcore::fx::Event{});
	TestFalse(TEXT("the same runner going on: no reset"), Cursor.Sync(&First, First.Last()));
	TestEqual(TEXT("only the new event"), static_cast<int32>(First.Since(Cursor.Seq).size()), 1);
	Cursor.Seq = First.Last();
	// A new runner (a new scenario): back to the start, even when its feed is ahead.
	for (int32 I = 0; I < 9; ++I)
	{
		Second.Push(simcore::fx::Event{});
	}
	TestTrue(TEXT("a new runner: a reset"), Cursor.Sync(&Second, Second.Last()));
	TestEqual(TEXT("the cursor at 0"), static_cast<int32>(Cursor.Seq), 0);
	TestTrue(TEXT("the cursor follows the new runner"), Cursor.Runner == &Second);
	Cursor.Seq = Second.Last();
	// The same runner's feed gone backwards (Last below the cursor): a reset.
	TestTrue(TEXT("a feed gone backwards: a reset"), Cursor.Sync(&Second, 4));
	TestEqual(TEXT("the cursor at 0 again"), static_cast<int32>(Cursor.Seq), 0);
	TestFalse(TEXT("then steady"), Cursor.Sync(&Second, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWrecksTest, "SAM3D.Fx.Wrecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWrecksTest::RunTest(const FString& Parameters)
{
	// The seed rule every client and the audio share.
	TestTrue(TEXT("seed of 42"), FSimFxWrecks::SeedFor(42) == (static_cast<uint32>(42) * 2654435761u ^ 0x5A17u));
	TestTrue(TEXT("seed of 7"), FSimFxWrecks::SeedFor(7) == (static_cast<uint32>(7) * 2654435761u ^ 0x5A17u));
	TestTrue(TEXT("different aircraft, different seeds"), FSimFxWrecks::SeedFor(42) != FSimFxWrecks::SeedFor(43));

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bFalls = Pass == 0;
		simcore::fx::WreckFallParams Params;
		Params.pos = simcore::Vec3(1000.0, 2000.0, 3000.0);
		Params.vel = simcore::Vec3(200.0, 50.0, 0.0);
		Params.ground_z_m = 150.0;
		Params.seed = FSimFxWrecks::SeedFor(42 + Pass);
		Params.falls = bFalls;
		const simcore::fx::WreckFall Ref(Params);
		const double Ground = Ref.GroundTime();
		TestTrue(*FString::Printf(TEXT("pass %d: it reaches the ground"), Pass), Ground > 1.0 && Ground < simcore::fx::WreckFall::kMaxS);

		FSimFxWrecks Wrecks;
		const double StartT = 120.0;
		Wrecks.Add(42 + Pass, StartT, Params, true, TEXT("mig21"));
		TestTrue(*FString::Printf(TEXT("pass %d: found"), Pass), Wrecks.Find(42 + Pass) != nullptr);
		const double Times[] = {0.0, 0.5, 2.0, 5.0, Ground * 0.5, Ground - 0.05};
		for (const double T : Times)
		{
			simcore::Vec3 Pos;
			simcore::fx::WreckRotation Rot;
			bool bVisible = false;
			const bool bKnown = Wrecks.PoseEnu(42 + Pass, StartT + T, Pos, Rot, bVisible);
			TestTrue(*FString::Printf(TEXT("pass %d, %.2f s: known"), Pass, T), bKnown);
			const simcore::Vec3 Want = Ref.Position(T);
			const simcore::fx::WreckRotation WantRot = Ref.Rotation(T);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: east"), Pass, T), Pos.x, Want.x, 1e-6);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: north"), Pass, T), Pos.y, Want.y, 1e-6);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: up"), Pass, T), Pos.z, Want.z, 1e-6);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: yaw"), Pass, T), Rot.yaw_rad, WantRot.yaw_rad, 1e-9);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: pitch"), Pass, T), Rot.pitch_rad, WantRot.pitch_rad, 1e-9);
			TestEqual(*FString::Printf(TEXT("pass %d, %.2f s: roll"), Pass, T), Rot.roll_rad, WantRot.roll_rad, 1e-9);
			TestTrue(*FString::Printf(TEXT("pass %d, %.2f s: in the air"), Pass, T), bVisible);
		}
		// It starts where the aircraft was destroyed, and goes down.
		simcore::Vec3 Pos;
		simcore::fx::WreckRotation Rot;
		bool bVisible = true;
		Wrecks.PoseEnu(42 + Pass, StartT, Pos, Rot, bVisible);
		TestEqual(*FString::Printf(TEXT("pass %d: starts at the kill"), Pass), Pos.z, 3000.0, 1.0);
		// After the ground time: on the ground and no longer drawn.
		const bool bKnown = Wrecks.PoseEnu(42 + Pass, StartT + Ground + 1.0, Pos, Rot, bVisible);
		TestTrue(*FString::Printf(TEXT("pass %d: known after landing"), Pass), bKnown);
		TestFalse(*FString::Printf(TEXT("pass %d: invisible after the ground time"), Pass), bVisible);
		TestEqual(*FString::Printf(TEXT("pass %d: on the ground"), Pass), Pos.z, 150.0, 1.0);
		// An aircraft not shot down is unknown.
		TestFalse(*FString::Printf(TEXT("pass %d: another id unknown"), Pass), Wrecks.PoseEnu(99, StartT + 1.0, Pos, Rot, bVisible));
		// Added again: replaced, not doubled.
		Wrecks.Add(42 + Pass, StartT + 10.0, Params, false, TEXT("mig21"));
		TestEqual(*FString::Printf(TEXT("pass %d: one wreck per id"), Pass), Wrecks.Items().Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxArtBaseTest, "SAM3D.Fx.ArtBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxArtBaseTest::RunTest(const FString& Parameters)
{
	// D7: growth within its growth time (1 - 1/e after one), none later than its life; without one an ease-out.
	TestEqual(TEXT("grown 63 % after one growth time"), SimFxLogic::GrowShare(0.4f, 10.0f, 0.4f), 1.0f - FMath::Exp(-1.0f), 1e-5f);
	TestTrue(TEXT("grown 99 % after five"), SimFxLogic::GrowShare(2.0f, 10.0f, 0.4f) > 0.99f);
	TestEqual(TEXT("no growth time: ease-out at half life"), SimFxLogic::GrowShare(5.0f, 10.0f, 0.0f), 0.75f, 1e-5f);
	// D7: erosion only in the second half of the life.
	TestEqual(TEXT("no erosion at 40 %"), SimFxLogic::ErosionAt(0.4f), 0.0f, 1e-6f);
	TestTrue(TEXT("some erosion at 75 %"), SimFxLogic::ErosionAt(0.75f) > 0.3f && SimFxLogic::ErosionAt(0.75f) < 0.7f);
	TestEqual(TEXT("eroded at the end"), SimFxLogic::ErosionAt(1.0f), 1.0f, 1e-6f);

	// D2: Gross's curve: held 0.05 s, then e-folding by 0.15 s (W/100 kg)^(1/3).
	TestEqual(TEXT("tau for 100 kg"), SimFxLogic::FireballTauS(100.0), 0.15, 1e-9);
	TestEqual(TEXT("tau for 800 kg"), SimFxLogic::FireballTauS(800.0), 0.30, 1e-9);
	TestEqual(TEXT("held hot"), SimFxLogic::HeatAt(1.0f, 0.0f, 0.1f, 0.04f, 0.05f, 0.15f), 1.0f, 1e-6f);
	TestEqual(TEXT("1/e one tau after the hold"), SimFxLogic::HeatAt(1.0f, 0.0f, 0.5f, 0.20f, 0.05f, 0.15f), FMath::Exp(-1.0f), 1e-5f);
	TestEqual(TEXT("no tau: a straight line"), SimFxLogic::HeatAt(1.0f, 0.0f, 0.25f, 9.0f, 0.0f, 0.0f), 0.75f, 1e-6f);

	// D8: a sprite widened to its pixel floor keeps its true light.
	TestEqual(TEXT("0.4 px drawn 2 px wide: a fifth"), SimFxLogic::SubPixelAlpha(0.4, 2.0, 2.0, 2.0), 0.2f, 1e-6f);
	TestEqual(TEXT("not widened: whole"), SimFxLogic::SubPixelAlpha(5.0, 5.0, 5.0, 5.0), 1.0f, 1e-6f);
	float Alpha = 0.0f;
	// 0.01 px per cm: a 1.2 px floor is 120 cm; a 60 cm line drawn 120 cm wide at half alpha.
	TestEqual(TEXT("ribbon floored"), SimFxLogic::RibbonWidthFloor(60.0f, 0.01, 1.2, Alpha), 120.0f, 1e-3f);
	TestEqual(TEXT("ribbon floored: half alpha"), Alpha, 0.5f, 1e-5f);
	TestEqual(TEXT("ribbon over the floor"), SimFxLogic::RibbonWidthFloor(300.0f, 0.01, 1.2, Alpha), 300.0f, 1e-3f);
	TestEqual(TEXT("ribbon over the floor: whole alpha"), Alpha, 1.0f, 1e-6f);

	// D6: the fire is drawn in the smoke's sorted draw; the others in their own.
	TestTrue(TEXT("fire with the smoke"), SimFxLogic::DrawLayerOf(ESimFxLayer::Fire) == ESimFxLayer::Smoke);
	TestTrue(TEXT("smoke in its own"), SimFxLogic::DrawLayerOf(ESimFxLayer::Smoke) == ESimFxLayer::Smoke);
	TestTrue(TEXT("glow in its own"), SimFxLogic::DrawLayerOf(ESimFxLayer::Glow) == ESimFxLayer::Glow);

	// T9: a light on the smoke: lumens / (4 pi d^2) / pi, windowed to its radius, held at the puff's size near it.
	const double Lm = 4.0 * UE_DOUBLE_PI * UE_DOUBLE_PI * 100.0;  // 100 at 1 m before the window
	const FLinearColor At10 = SimFxLogic::LightOnSurface(FLinearColor::White, Lm, 1000.0, 1.0e9, 0.0);
	TestEqual(TEXT("light at 10 m"), At10.R, 1.0f, 1e-3f);
	TestEqual(TEXT("light past its radius"), SimFxLogic::LightOnSurface(FLinearColor::White, Lm, 1000.0, 900.0, 0.0).R, 0.0f, 1e-9f);
	TestEqual(TEXT("light inside the puff held at its size"),
		SimFxLogic::LightOnSurface(FLinearColor::White, Lm, 10.0, 1.0e9, 1000.0).R, At10.R, 1e-3f);
	const double Half = FMath::Square(1.0 - FMath::Pow(0.5, 4.0));
	TestEqual(TEXT("light windowed at half its radius"), SimFxLogic::LightOnSurface(FLinearColor::White, Lm, 1000.0, 2000.0, 0.0).R,
		static_cast<float>(Half), 1e-3f);

	// N1: the sky's share by the sun's elevation.
	TestEqual(TEXT("day"), SimFxLogic::AmbientShare(20.0, 0.3, 0.1, 0.004), 0.3, 1e-9);
	TestEqual(TEXT("sunset"), SimFxLogic::AmbientShare(0.0, 0.3, 0.1, 0.004), 0.1, 1e-9);
	TestEqual(TEXT("civil twilight's end"), SimFxLogic::AmbientShare(-6.0, 0.3, 0.1, 0.004), 0.1, 1e-9);
	TestEqual(TEXT("a decade per 2 degrees"), SimFxLogic::AmbientShare(-8.0, 0.3, 0.1, 0.0001), 0.01, 1e-9);
	TestEqual(TEXT("night's floor"), SimFxLogic::AmbientShare(-30.0, 0.3, 0.1, 0.004), 0.004, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxExplosionsTest, "SAM3D.Fx.Explosions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxExplosionsTest::RunTest(const FString& Parameters)
{
	// LOOK 2.2: a Mk 82's column (about 100 kg of charge) 26-37 m at 1 s, 45-65 at 3 s, 83-118 at 10 s, 53 m wide.
	const double Scale = 0.85;
	const double At1 = SimFxLogic::ColumnTopM(100.0, 1.0, Scale);
	const double At3 = SimFxLogic::ColumnTopM(100.0, 3.0, Scale);
	const double At10 = SimFxLogic::ColumnTopM(100.0, 10.0, Scale);
	TestTrue(TEXT("Mk 82 column at 1 s"), At1 >= 26.0 && At1 <= 37.0);
	TestTrue(TEXT("Mk 82 column at 3 s"), At3 >= 45.0 && At3 <= 65.0);
	TestTrue(TEXT("Mk 82 column at 10 s"), At10 >= 83.0 && At10 <= 118.0);
	TestEqual(TEXT("rises as sqrt(t)"), At3 / At1, FMath::Sqrt(3.0), 1e-6);
	TestEqual(TEXT("stabilised at 60 s for 100 kg"), SimFxLogic::ColumnStabS(100.0), 60.0, 1e-6);
	TestEqual(TEXT("no higher after it stabilised"), SimFxLogic::ColumnTopM(100.0, 300.0, Scale), SimFxLogic::ColumnTopM(100.0, 60.0, Scale), 1e-6);
	TestEqual(TEXT("K sqrt(t)"), SimFxLogic::ColumnRiseK(100.0, Scale) * 2.0, SimFxLogic::ColumnTopM(100.0, 4.0, Scale), 1e-6);
	TestEqual(TEXT("column 53 m wide"), SimFxLogic::ColumnWidthM(100.0), 53.0, 0.5);

	// LOOK 6: 500 kg of fuel: a 46 m fireball for 3.6 s (inside 39-58 m and 3-4.5 s).
	TestEqual(TEXT("fuel fireball size"), SimFxLogic::FuelFireballM(500.0), 46.0, 0.5);
	TestEqual(TEXT("fuel fireball time"), SimFxLogic::FuelFireballS(500.0), 3.57, 0.02);

	// LOOK 3: Sachs scaling, x 1.23 at 5 km and x 1.56 at 10 km; none at sea level.
	TestEqual(TEXT("Sachs at sea level"), SimFxLogic::SachsScale(0.0), 1.0, 1e-9);
	TestEqual(TEXT("Sachs at 5 km"), SimFxLogic::SachsScale(5000.0), 1.23, 0.01);
	TestEqual(TEXT("Sachs at 10 km"), SimFxLogic::SachsScale(10000.0), 1.56, 0.01);

	// LOOK 4: the flak table interpolated by calibre, held past its ends.
	TArray<SimFxLogic::FFlakRow> Rows;
	Rows.Add({23.0, 1.5, 20.0, 0.0});
	Rows.Add({57.0, 4.0, 40.0, 0.0});
	Rows.Add({85.0, 10.0, 120.0, 1.0});
	TestEqual(TEXT("flak: a row"), SimFxLogic::FlakFor(Rows, 57.0).PuffM, 4.0, 1e-9);
	TestEqual(TEXT("flak: between rows"), SimFxLogic::FlakFor(Rows, 71.0).HangS, 80.0, 1e-9);
	TestEqual(TEXT("flak: between rows, half black"), SimFxLogic::FlakFor(Rows, 71.0).Black, 0.5, 1e-9);
	TestEqual(TEXT("flak: under the table"), SimFxLogic::FlakFor(Rows, 12.7).PuffM, 1.5, 1e-9);
	TestEqual(TEXT("flak: over the table"), SimFxLogic::FlakFor(Rows, 130.0).PuffM, 10.0, 1e-9);
	const TArray<SimFxLogic::FFlakRow> NoRows;
	TestEqual(TEXT("flak: empty table"), SimFxLogic::FlakFor(NoRows, 57.0).PuffM, SimFxLogic::FFlakRow().PuffM, 1e-9);

	// The art review's section 2: the ring faded at grazing angles; the day's cap on the flashes' light.
	TestEqual(TEXT("ring seen from above"), SimFxLogic::RingAlpha(1.0), 1.0f, 1e-6f);
	TestEqual(TEXT("ring edge-on"), SimFxLogic::RingAlpha(0.0), 0.0f, 1e-6f);
	TestEqual(TEXT("ring from 5 degrees"), SimFxLogic::RingAlpha(FMath::Sin(FMath::DegreesToRadians(5.0))), 0.513f, 0.01f);
	const FLinearColor Hot(3.0f, 1.5f, 0.6f, 1.0f);
	TestEqual(TEXT("light capped: peak"), SimFxLogic::CapLight(Hot, 0.6).R, 0.6f, 1e-5f);
	TestEqual(TEXT("light capped: hue kept"), SimFxLogic::CapLight(Hot, 0.6).G, 0.3f, 1e-5f);
	TestEqual(TEXT("no cap at night"), SimFxLogic::CapLight(Hot, 0.0).R, 3.0f, 1e-6f);
	TestEqual(TEXT("under the cap"), SimFxLogic::CapLight(FLinearColor(0.2f, 0.1f, 0.0f), 0.6).R, 0.2f, 1e-6f);

	// LOOK 3, 8: fragments in a forward cone along the missile's velocity.
	const FVector Along = FVector(1.0, 2.0, -0.5).GetSafeNormal();
	const double Half = FMath::DegreesToRadians(35.0);
	bool bInCone = true;
	bool bUnit = true;
	for (int32 I = 0; I <= 10; ++I)
	{
		for (int32 J = 0; J < 8; ++J)
		{
			const FVector D = SimFxLogic::DirInCone(Along, Half, I / 10.0, J / 8.0);
			bInCone = bInCone && FVector::DotProduct(D, Along) >= FMath::Cos(Half) - 1e-6;
			bUnit = bUnit && FMath::IsNearlyEqual(D.Size(), 1.0, 1e-6);
		}
	}
	TestTrue(TEXT("cone: within its half angle"), bInCone);
	TestTrue(TEXT("cone: unit directions"), bUnit);
	TestTrue(TEXT("cone: U1 0 is the axis"), SimFxLogic::DirInCone(Along, Half, 0.0, 0.3).Equals(Along, 1e-6));
	TestEqual(TEXT("cone: U1 1 on its edge"), FVector::DotProduct(SimFxLogic::DirInCone(Along, Half, 1.0, 0.7), Along), FMath::Cos(Half), 1e-6);

	// The closed form against the particles' own stepping (a clod's path, for the puffs laid along it).
	{
		FSimFxParticles Ps;
		FSimFxParticle P;
		P.Life = 10.0f;
		P.Vel = FVector(3000.0, 0.0, 4000.0);
		P.Wind = FVector(500.0, 0.0, 0.0);
		P.Drag = 0.8f;
		P.Gravity = 981.0f;
		P.Layer = ESimFxLayer::Debris;
		Ps.Spawn(P);
		for (int32 I = 0; I < 480; ++I)
		{
			Ps.Update(1.0f / 240.0f);
		}
		const FVector Want = SimFxLogic::BallisticAt(FVector::ZeroVector, P.Vel, P.Wind, P.Drag, P.Gravity, 2.0);
		TestTrue(TEXT("ballistic: stepped and closed agree to 1 %"), Ps.Layer(ESimFxLayer::Debris)[0].Pos.Equals(Want, Want.Size() * 0.01));
		const FVector Free = SimFxLogic::BallisticAt(FVector::ZeroVector, FVector(100.0, 0.0, 0.0), FVector::ZeroVector, 0.0, 981.0, 2.0);
		TestTrue(TEXT("ballistic: no drag"), Free.Equals(FVector(200.0, 0.0, -1962.0), 1e-6));
	}

	// A waiting particle (a later puff) is neither moved nor drawn; then it starts.
	{
		FSimFxParticles Ps;
		FSimFxParticle P;
		P.Age = -0.5f;
		P.Life = 5.0f;
		P.Vel = FVector(100.0, 0.0, 0.0);
		Ps.Spawn(P);
		Ps.Update(0.25f);
		TestTrue(TEXT("waiting"), Ps.Layer(ESimFxLayer::Smoke)[0].IsWaiting());
		TestTrue(TEXT("unmoved while waiting"), Ps.Layer(ESimFxLayer::Smoke)[0].Pos.IsNearlyZero());
		Ps.Update(0.5f);
		TestFalse(TEXT("begun"), Ps.Layer(ESimFxLayer::Smoke)[0].IsWaiting());
		TestTrue(TEXT("moving once begun"), Ps.Layer(ESimFxLayer::Smoke)[0].Pos.X > 0.0);
	}

	// A column's member rises RiseK sqrt(t) until the column stabilises, whatever the step.
	{
		FSimFxParticles Ps;
		FSimFxParticle P;
		P.Life = 100.0f;
		P.RiseK = 1000.0f;
		P.RiseStopS = 9.0f;
		Ps.Spawn(P);
		for (int32 I = 0; I < 40; ++I)
		{
			Ps.Update(0.1f);
		}
		TestEqual(TEXT("member at 4 s: K sqrt(4)"), Ps.Layer(ESimFxLayer::Smoke)[0].Pos.Z, 2000.0, 1.0);
		for (int32 I = 0; I < 20; ++I)
		{
			Ps.Update(0.5f);
		}
		TestEqual(TEXT("member held once stabilised"), Ps.Layer(ESimFxLayer::Smoke)[0].Pos.Z, 3000.0, 1.0);
	}

	// A rooted stem is drawn from its root up its axis, at least LengthK sqrt(age) long; a tint turned over its life.
	{
		FSimFxParticle P;
		P.Age = 4.0f;
		P.Life = 8.0f;
		P.Size0 = 100.0f;
		P.Size1 = 100.0f;
		P.Facing = ESimFxFacing::Axis;
		P.FixedAxis = FVector::UpVector;
		P.LengthK = 500.0f;
		P.bRooted = true;
		P.Tint = FLinearColor::Black;
		P.Tint1 = FLinearColor::White;
		P.bTintOverLife = true;
		const FSimFxSprite S = P.ToSprite();
		TestEqual(TEXT("stem length"), S.Length, 1000.0f, 1e-3f);
		TestTrue(TEXT("stem centred half its length above its root"), S.Pos.Equals(FVector(0.0, 0.0, 500.0), 1e-3));
		TestEqual(TEXT("tint half turned at half life"), S.Tint.R, 0.5f, 1e-5f);
		P.SpreadCmps = 10.0f;
		TestEqual(TEXT("spread widens it after its growth"), P.ToSprite().Width, 140.0f, 1e-3f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxTrailsGunsTest, "SAM3D.Fx.TrailsGuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxTrailsGunsTest::RunTest(const FString& Parameters)
{
	// LOOK 9.1, 12: a ribbon point carried by its drift, rolled toward its side (e-folding), sunk toward its floor.
	const FVector Drift(100.0, 0.0, 0.0);
	const FVector Side(0.0, 1000.0, 0.0);
	TestTrue(TEXT("point: drift only"), SimFxLogic::RibbonPointAt(FVector::ZeroVector, Drift, FVector::ZeroVector, 0.0f, 0.0f, 0.0f, 3.0f).Equals(FVector(300.0, 0.0, 0.0), 1e-3));
	TestTrue(TEXT("point: rolled at once with no roll time"), SimFxLogic::RibbonPointAt(FVector::ZeroVector, FVector::ZeroVector, Side, 0.0f, 0.0f, 0.0f, 0.5f).Equals(Side, 1e-3));
	TestEqual(TEXT("point: rolled 1 - 1/e in one roll time"), SimFxLogic::RibbonPointAt(FVector::ZeroVector, FVector::ZeroVector, Side, 2.0f, 0.0f, 0.0f, 2.0f).Y,
		1000.0 * (1.0 - FMath::Exp(-1.0)), 0.5);
	TestEqual(TEXT("point: sinks 2.5 m/s at first"), SimFxLogic::RibbonPointAt(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 0.0f, 250.0f, 27000.0f, 0.1f).Z,
		-25.0, 0.1);
	TestEqual(TEXT("point: sinks no lower than its floor (LOOK 12: 270 m)"),
		SimFxLogic::RibbonPointAt(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 0.0f, 250.0f, 27000.0f, 10000.0f).Z, -27000.0, 1.0);

	// LOOK 12: the Crow rings, none before 30 s, grown by 60 s, a ring every 8.6 spans.
	TestEqual(TEXT("crow: none before"), SimFxLogic::CrowAt(0.0f, 86.0f, 20.0f, 30.0f, 60.0f), 0.0f, 1e-6f);
	TestEqual(TEXT("crow: grown, at a ring"), SimFxLogic::CrowAt(86.0f, 86.0f, 70.0f, 30.0f, 60.0f), 1.0f, 1e-4f);
	TestEqual(TEXT("crow: grown, between rings"), SimFxLogic::CrowAt(43.0f, 86.0f, 70.0f, 30.0f, 60.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("crow: half grown"), SimFxLogic::CrowAt(0.0f, 86.0f, 45.0f, 30.0f, 60.0f), 0.5f, 1e-4f);
	TestEqual(TEXT("crow: no period, none"), SimFxLogic::CrowAt(0.0f, 0.0f, 100.0f, 30.0f, 60.0f), 0.0f, 1e-6f);

	// LOOK 9.3: opacity 0.8 at the missile thins to 0.5 by 15 s, and fades at the end of its life; erosion from 15 s of 40.
	TestEqual(TEXT("trail opacity at the missile"), 0.8f * SimFxLogic::TrailOpacityAt(0.0f, 40.0f, 15.0f, 0.625f), 0.8f, 1e-5f);
	TestEqual(TEXT("trail opacity at 15 s"), 0.8f * SimFxLogic::TrailOpacityAt(15.0f, 40.0f, 15.0f, 0.625f), 0.5f, 1e-4f);
	TestEqual(TEXT("trail gone at its life's end"), SimFxLogic::TrailOpacityAt(40.0f, 40.0f, 15.0f, 0.625f), 0.0f, 1e-6f);
	TestEqual(TEXT("no thinning without a thin time"), SimFxLogic::TrailOpacityAt(5.0f, 40.0f, 0.0f, 0.625f), 1.0f, 1e-6f);
	TestEqual(TEXT("no erosion before its start"), SimFxLogic::TrailErosionAt(10.0f, 40.0f, 0.35f), 0.0f, 1e-6f);
	TestEqual(TEXT("full erosion at its life's end"), SimFxLogic::TrailErosionAt(40.0f, 40.0f, 0.35f), 0.85f, 1e-5f);

	// LOOK 14: the tracer's colour by the light (day above +6 degrees, night below -6), its length a frame's flight,
	// and only the belt's traced rounds seen.
	const FLinearColor Day(1.0f, 0.81f, 0.53f), Night(1.0f, 0.068f, 0.018f);
	TestEqual(TEXT("night share by day"), SimFxLogic::NightShare(20.0), 0.0f, 1e-6f);
	TestEqual(TEXT("night share at night"), SimFxLogic::NightShare(-12.0), 1.0f, 1e-6f);
	TestEqual(TEXT("night share at sunset"), SimFxLogic::NightShare(0.0), 0.5f, 1e-5f);
	TestEqual(TEXT("tracer by day"), SimFxLogic::TracerColourAt(Day, Night, 30.0).G, 0.81f, 1e-5f);
	TestEqual(TEXT("tracer at night"), SimFxLogic::TracerColourAt(Day, Night, -20.0).G, 0.068f, 1e-5f);
	TestEqual(TEXT("tracer 16 m at 970 m/s and 60 Hz"), SimFxLogic::TracerLengthCm(97000.0, 1.0 / 120.0, 1.0 / 60.0), 1616.7, 0.1);
	TestEqual(TEXT("tracer longer at a slower frame"), SimFxLogic::TracerLengthCm(97000.0, 1.0 / 30.0, 1.0 / 60.0), 3233.3, 0.1);
	int32 Traced = 0;
	for (int32 I = 0; I < 40; ++I)
	{
		Traced += SimFxLogic::IsTracedRound(I, 4) ? 1 : 0;
	}
	TestEqual(TEXT("the Shilka's belt: 1 in 4 traced"), Traced, 10);
	TestTrue(TEXT("the belt's first round traced"), SimFxLogic::IsTracedRound(0, 5));
	TestFalse(TEXT("the next not"), SimFxLogic::IsTracedRound(1, 5));
	TestTrue(TEXT("every round with no belt given"), SimFxLogic::IsTracedRound(7, 1));

	// LOOK 9.2: the MANPADS corkscrew, its radius kept, one turn in a pitch, square to its way.
	const FVector Fwd = FVector(1.0, 1.0, 0.5).GetSafeNormal();
	const FVector H0 = SimFxLogic::HelixOffset(Fwd, 0.0, 21.5, 0.6, 0.3);
	const FVector HQ = SimFxLogic::HelixOffset(Fwd, 21.5 * 0.25, 21.5, 0.6, 0.3);
	TestEqual(TEXT("helix radius"), H0.Size(), 60.0, 1e-6);
	TestEqual(TEXT("helix square to its way"), FVector::DotProduct(H0, Fwd), 0.0, 1e-6);
	TestTrue(TEXT("helix: one turn in a pitch"), SimFxLogic::HelixOffset(Fwd, 21.5, 21.5, 0.6, 0.3).Equals(H0, 1e-6));
	TestEqual(TEXT("helix: a quarter turn a quarter pitch on"), FVector::DotProduct(H0, HQ), 0.0, 1e-6);
	// The camera fade: a 17.5 m launch puff clear within 9.75 m, whole by 21.5 m; a puff 400 m off untouched.
	TestEqual(TEXT("camera fade: inside the puff"), SimFxLogic::CameraFade(500.0, 1750.0), 0.0f, 1e-6f);
	TestEqual(TEXT("camera fade: halfway"), SimFxLogic::CameraFade(975.0 + 0.5 * 1175.0, 1750.0), 0.5f, 1e-5f);
	TestEqual(TEXT("camera fade: whole past its size"), SimFxLogic::CameraFade(2150.0, 1750.0), 1.0f, 1e-6f);
	TestEqual(TEXT("camera fade: far"), SimFxLogic::CameraFade(40000.0, 1750.0), 1.0f, 1e-6f);
	TestEqual(TEXT("end-on: along the trail"), SimFxLogic::EndOnAlpha(0.0), 0.0f, 1e-6f);
	TestEqual(TEXT("end-on: across it"), SimFxLogic::EndOnAlpha(1.0), 1.0f, 1e-6f);
	TestEqual(TEXT("end-on: halfway"), SimFxLogic::EndOnAlpha(0.115), 0.5f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxGunRhythmTest, "SAM3D.Fx.GunRhythm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxGunRhythmTest::RunTest(const FString& Parameters)
{
	// A gun pass's bursts (LOOK 14, effects.json guns): the picture and the sound draw them from the event's seq alone,
	// so the same seq gives the same bursts; each within its lengths, the pauses within theirs, the first at the pass's
	// start, none past its end.
	TArray<SimFxLogic::FGunBurst> A, B, C;
	SimFxLogic::GunBursts(42u, 10.0, 30.0, 0.2, 0.35, 2.0, 3.0, A);
	SimFxLogic::GunBursts(42u, 10.0, 30.0, 0.2, 0.35, 2.0, 3.0, B);
	SimFxLogic::GunBursts(43u, 10.0, 30.0, 0.2, 0.35, 2.0, 3.0, C);
	TestTrue(FString::Printf(TEXT("%d bursts in 20 s (one every 2.2-3.35 s)"), A.Num()), A.Num() >= 6 && A.Num() <= 10);
	bool bSame = A.Num() == B.Num();
	for (int32 I = 0; bSame && I < A.Num(); ++I)
	{
		bSame = A[I].StartS == B[I].StartS && A[I].EndS == B[I].EndS;
	}
	TestTrue(TEXT("the same seq, the same bursts"), bSame);
	TestTrue(TEXT("another seq, other bursts"), C.Num() > 0 && (C.Num() != A.Num() || C[0].EndS != A[0].EndS));
	TestEqual(TEXT("the first at the pass's start"), A[0].StartS, 10.0);
	for (int32 I = 0; I < A.Num(); ++I)
	{
		const double Len = A[I].EndS - A[I].StartS;
		TestTrue(FString::Printf(TEXT("burst %d lasts %.3f s"), I, Len), (Len >= 0.2 - 1e-9 && Len <= 0.35 + 1e-9) || A[I].EndS == 30.0);
		TestTrue(FString::Printf(TEXT("burst %d inside the pass"), I), A[I].StartS < 30.0 && A[I].EndS <= 30.0);
		if (I > 0)
		{
			const double Pause = A[I].StartS - A[I - 1].EndS;
			TestTrue(FString::Printf(TEXT("pause %d is %.3f s"), I, Pause), Pause >= 2.0 - 1e-9 && Pause <= 3.0 + 1e-9);
		}
	}
	// Its rounds: the first at the burst's start, then one every 1/rate while it lasts (a ZSU-23-4 at 3400 a minute
	// fires 15 in 0.26 s; an S-60 at 120 a minute one).
	TArray<double> Shots;
	SimFxLogic::GunBurstShots(SimFxLogic::FGunBurst{5.0, 5.26}, 3400.0 / 60.0, 100.0, Shots);
	TestEqual(TEXT("a Shilka's burst"), Shots.Num(), 15);
	TestEqual(TEXT("from its start"), Shots[0], 5.0);
	SimFxLogic::GunBurstShots(SimFxLogic::FGunBurst{5.0, 5.3}, 2.0, 100.0, Shots);
	TestEqual(TEXT("an S-60's burst: one round"), Shots.Num(), 1);
	SimFxLogic::GunBurstShots(SimFxLogic::FGunBurst{5.0, 6.0}, 10.0, 5.35, Shots);
	TestEqual(TEXT("cut at the pass's end"), Shots.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxPolishTest, "SAM3D.Fx.Polish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxPolishTest::RunTest(const FString& Parameters)
{
	// The sun on a puff as a ball turned to the camera (the second art review's item 1), by hand: the middle front-lit is
	// fully lit; the underside with the sun overhead and the camera level faces (0.6, 0, -1)/1.166, N.L = -0.857, so only
	// the quarter from the whole puff's lit is left; the top faces (0.6, 0, 1)/1.166, N.L = 0.857: 0.75 (0.3 + 0.6) + 0.25.
	const FVector Cam(1.0, 0.0, 0.0);
	const FVector Up(0.0, 0.0, 1.0);
	TestEqual(TEXT("sun share: the middle, front-lit"), SimFxLogic::SunShare(FVector::ZeroVector, Cam, Cam, 1.0f), 1.0f, 1e-5f);
	TestEqual(TEXT("sun share: the underside"), SimFxLogic::SunShare(-Up, Cam, Up, 0.6f), 0.25f * 0.6f, 1e-4f);
	TestEqual(TEXT("sun share: the top"), SimFxLogic::SunShare(Up, Cam, Up, 0.6f), 0.75f * (0.3f + 0.7f / 1.16619f) + 0.25f * 0.6f, 1e-4f);
	TestEqual(TEXT("sun share: past the rim held to the rim"), SimFxLogic::SunShare(-Up * 3.0, Cam, Up, 0.6f), 0.15f, 1e-4f);

	// A trail near the camera seen along it is drawn as puffs (T10): wholly under a sine of 0.2 within 200 m, not at all
	// from 0.5 or past 300 m; end-on (sine 0.02) as far as 2 km, gone by 3 km.
	TestEqual(TEXT("puffs: end-on, near"), SimFxLogic::TrailPuffShare(0.1, 5000.0), 1.0f, 1e-6f);
	TestEqual(TEXT("puffs: across"), SimFxLogic::TrailPuffShare(0.6, 5000.0), 0.0f, 1e-6f);
	TestEqual(TEXT("puffs: halfway by angle"), SimFxLogic::TrailPuffShare(0.35, 5000.0), 0.5f, 1e-5f);
	TestEqual(TEXT("puffs: past 300 m at 0.2"), SimFxLogic::TrailPuffShare(0.2, 30000.0), 0.0f, 1e-6f);
	TestEqual(TEXT("puffs: end-on at 1.9 km"), SimFxLogic::TrailPuffShare(0.02, 190000.0), 1.0f, 1e-6f);
	TestEqual(TEXT("puffs: end-on at 3 km"), SimFxLogic::TrailPuffShare(0.02, 300000.0), 0.0f, 1e-6f);

	// The dust skirt thins from 0.6 to 0.3 by 3 s and lingers (LOOK 2.1): ThinTo 0.5 over 3 s.
	TestEqual(TEXT("thin: at birth"), SimFxLogic::ThinAt(0.0f, 3.0f, 0.5f), 1.0f, 1e-6f);
	TestEqual(TEXT("thin: halfway"), SimFxLogic::ThinAt(1.5f, 3.0f, 0.5f), 0.75f, 1e-5f);
	TestEqual(TEXT("thin: by 3 s"), SimFxLogic::ThinAt(3.0f, 3.0f, 0.5f), 0.5f, 1e-6f);
	TestEqual(TEXT("thin: held after"), SimFxLogic::ThinAt(30.0f, 3.0f, 0.5f), 0.5f, 1e-6f);
	TestEqual(TEXT("thin: none"), SimFxLogic::ThinAt(2.0f, 0.0f, 0.5f), 1.0f, 1e-6f);

	// A flash's light: white for its first frames, then from its colour to the ember's (#FF8100) by its end.
	const FLinearColor Base(1.0f, 0.7f, 0.4f), Ember(1.0f, 0.22f, 0.0f);
	TestTrue(TEXT("flash light: white at first"), SimFxLogic::FlashLightColour(Base, Ember, 0.01, 0.3, 0.035).Equals(FLinearColor::White, 1e-6f));
	TestTrue(TEXT("flash light: its colour once white is over"), SimFxLogic::FlashLightColour(Base, Ember, 0.035, 0.3, 0.035).Equals(Base, 1e-5f));
	TestTrue(TEXT("flash light: the ember at its end"), SimFxLogic::FlashLightColour(Base, Ember, 0.3, 0.3, 0.035).Equals(Ember, 1e-5f));

	// A slow gun's bursts in rounds (the second art review's item 7): the S-60 at 2 rounds a second, 4 rounds at least:
	// 1.625-3.625 s, which GunBurstShots fires as 4 to 8 rounds; the Shilka keeps its 0.2-0.35 s.
	double MinS = 0.0, MaxS = 0.0;
	SimFxLogic::GunBurstSpan(0.2, 0.35, 4, 2.0, MinS, MaxS);
	TestEqual(TEXT("S-60 burst: shortest"), MinS, 1.625, 1e-9);
	TestEqual(TEXT("S-60 burst: longest"), MaxS, 3.625, 1e-9);
	TArray<double> Shots;
	SimFxLogic::GunBurstShots(SimFxLogic::FGunBurst{0.0, MinS}, 2.0, 100.0, Shots);
	TestEqual(TEXT("S-60 burst: 4 rounds in the shortest"), Shots.Num(), 4);
	SimFxLogic::GunBurstShots(SimFxLogic::FGunBurst{0.0, MaxS}, 2.0, 100.0, Shots);
	TestEqual(TEXT("S-60 burst: 8 rounds in the longest"), Shots.Num(), 8);
	SimFxLogic::GunBurstSpan(0.2, 0.35, 1, 3400.0 / 60.0, MinS, MaxS);
	TestEqual(TEXT("Shilka burst: unchanged"), MinS, 0.2, 1e-12);
	TestEqual(TEXT("Shilka burst: unchanged, longest"), MaxS, 0.35, 1e-12);
	const TPair<FString, int32> Rows[] = {TPair<FString, int32>(TEXT("shell_57"), 4), TPair<FString, int32>(TEXT("shell_37"), 5)};
	TestEqual(TEXT("table: by the munition"), SimFxLogic::NamedIntFor(Rows, TEXT("shell_57_s60"), TEXT(""), 1), 4);
	TestEqual(TEXT("table: by the weapon"), SimFxLogic::NamedIntFor(Rows, TEXT(""), TEXT("SHELL_37_aaa"), 1), 5);
	TestEqual(TEXT("table: neither"), SimFxLogic::NamedIntFor(Rows, TEXT("shell_23_zsu234"), TEXT("zsu234"), 1), 1);

	// The sky on a puff (SkyShare): all of it facing up, Low of it facing down, halfway between level; the normal it is
	// read from is SunShare's, the middle of a puff facing the camera.
	TestEqual(TEXT("sky: a top"), SimFxLogic::SkyShare(1.0, 0.55), 1.0f, 1e-6f);
	TestEqual(TEXT("sky: an underside"), SimFxLogic::SkyShare(-1.0, 0.55), 0.55f, 1e-6f);
	TestEqual(TEXT("sky: a side"), SimFxLogic::SkyShare(0.0, 0.55), 0.775f, 1e-6f);
	TestTrue(TEXT("puff normal: the middle faces the camera"), SimFxLogic::PuffNormal(FVector::ZeroVector, Cam).Equals(Cam, 1e-9));
	TestEqual(TEXT("puff normal: the underside"), SimFxLogic::PuffNormal(-Up * 2.0, Cam).Z, -1.0 / FMath::Sqrt(1.36), 1e-9);

	// A stick's bursts share out their detail (CrowdShare): alone, all of it; beside one, two thirds; the sixth among
	// five, 2/7; never under a quarter.
	TestEqual(TEXT("crowd: alone"), SimFxLogic::CrowdShare(0), 1.0f, 1e-6f);
	TestEqual(TEXT("crowd: beside one"), SimFxLogic::CrowdShare(1), 2.0f / 3.0f, 1e-6f);
	TestEqual(TEXT("crowd: the third"), SimFxLogic::CrowdShare(2), 0.5f, 1e-6f);
	TestEqual(TEXT("crowd: the sixth"), SimFxLogic::CrowdShare(5), 2.0f / 7.0f, 1e-6f);
	TestEqual(TEXT("crowd: many"), SimFxLogic::CrowdShare(40), 0.25f, 1e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxGlintTracerTest, "SAM3D.Fx.GlintTracer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxGlintTracerTest::RunTest(const FString& Parameters)
{
	// A glint keeps its opacity at its pixel floor (GlintAlpha): a 1 px glow drawn 8 px is 1/64 by area, held at the keep;
	// one wider than its keep's share keeps its area's; no keep is SubPixelAlpha.
	TestEqual(TEXT("glint: held at the keep"), SimFxLogic::GlintAlpha(1.0, 1.0, 8.0, 8.0, 0.5f), 0.5f, 1e-6f);
	TestEqual(TEXT("glint: its area when more"), SimFxLogic::GlintAlpha(6.0, 8.0, 8.0, 8.0, 0.5f), 0.75f, 1e-6f);
	TestEqual(TEXT("glint: no keep"), SimFxLogic::GlintAlpha(1.0, 1.0, 8.0, 8.0, 0.0f), 1.0f / 64.0f, 1e-7f);
	TestEqual(TEXT("glint: keep held under 1"), SimFxLogic::GlintAlpha(1.0, 1.0, 8.0, 8.0, 3.0f), 1.0f, 1e-6f);

	// A tracer's colour (LOOK 14): an orange munition's tracer is the day's yellow-white by day and its own orange at
	// night; a green one halfway to white by day; none given, the day and night colours.
	const FLinearColor Day(1.0f, 0.81f, 0.53f), Night(1.0f, 0.068f, 0.018f), Orange(1.0f, 0.195f, 0.023f), Green(0.25f, 1.0f, 0.1f);
	TestTrue(TEXT("tracer: orange by day"), SimFxLogic::TracerColourFor(Orange, true, Day, Night, 40.0).Equals(Day, 1e-6f));
	TestTrue(TEXT("tracer: orange at night"), SimFxLogic::TracerColourFor(Orange, true, Day, Night, -30.0).Equals(Orange, 1e-6f));
	TestTrue(TEXT("tracer: green by day"), SimFxLogic::TracerColourFor(Green, true, Day, Night, 40.0).Equals(FLinearColor(0.625f, 1.0f, 0.55f), 1e-6f));
	TestTrue(TEXT("tracer: green at night"), SimFxLogic::TracerColourFor(Green, true, Day, Night, -30.0).Equals(Green, 1e-6f));
	TestTrue(TEXT("tracer: none given, by day"), SimFxLogic::TracerColourFor(Green, false, Day, Night, 40.0).Equals(Day, 1e-6f));
	TestTrue(TEXT("tracer: none given, at night"), SimFxLogic::TracerColourFor(Green, false, Day, Night, -30.0).Equals(Night, 1e-6f));

	// The corkscrew's radius (LOOK 9.2, 0.3-1.5 m about a 0.6 m mean): within half again either way at every point, at its
	// mean where both waves cross zero (the start, no phase), and not the same two turns running.
	double Lo = 1e9, Hi = -1e9;
	for (int32 K = 0; K < 2000; ++K)
	{
		const double R = SimFxLogic::HelixRadiusAt(0.37 * K, 21.5, 0.6, 0.8);
		Lo = FMath::Min(Lo, R);
		Hi = FMath::Max(Hi, R);
	}
	TestTrue(TEXT("helix: never under half its mean"), Lo >= 0.3 - 1e-9);
	TestTrue(TEXT("helix: never over half again"), Hi <= 0.9 + 1e-9);
	TestTrue(TEXT("helix: it varies"), Hi - Lo > 0.3);
	TestEqual(TEXT("helix: its mean at the start"), SimFxLogic::HelixRadiusAt(0.0, 21.5, 0.6, 0.0), 0.6, 1e-12);
	TestTrue(TEXT("helix: one turn unlike the next"), FMath::Abs(SimFxLogic::HelixRadiusAt(21.5, 21.5, 0.6, 0.0) - SimFxLogic::HelixRadiusAt(43.0, 21.5, 0.6, 0.0)) > 0.05);
	return true;
}

// The fix-up review of 1 Nov (docs/fx/README.md 4.1, 5): the effects' step under time compression, trails within the
// visual range, the ribbon pool's take-over and release by owner, a repeated kill, and the trails' mesh section shrinking.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxFixUpTest, "SAM3D.Fx.FixUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxFixUpTest::RunTest(const FString& Parameters)
{
	// The step: the render clock's advance up to a 0.25 s hitch times the scale (at 32x and 60 fps, 0.53 s whole).
	TestEqual(TEXT("step: 32x at 60 fps whole"), SimFxLogic::FrameDt(0.53, 32.0, 0.25), 0.53, 1e-12);
	TestEqual(TEXT("step: 1x hitch capped"), SimFxLogic::FrameDt(0.53, 1.0, 0.25), 0.25, 1e-12);
	TestEqual(TEXT("step: a long stall at 32x capped at 8 s"), SimFxLogic::FrameDt(9.0, 32.0, 0.25), 8.0, 1e-12);
	TestEqual(TEXT("step: slow motion still a 0.25 s cap"), SimFxLogic::FrameDt(0.4, 0.5, 0.25), 0.25, 1e-12);
	TestEqual(TEXT("step: never backwards"), SimFxLogic::FrameDt(-1.0, 4.0, 0.25), 0.0, 1e-12);
	TestEqual(TEXT("slices: 0.53 s in three"), SimFxLogic::SubSteps(0.53, 0.25, 8), 3);
	TestEqual(TEXT("slices: 0.25 s in one"), SimFxLogic::SubSteps(0.25, 0.25, 8), 1);
	TestEqual(TEXT("slices: none, one"), SimFxLogic::SubSteps(0.0, 0.25, 8), 1);
	TestEqual(TEXT("slices: at most the cap"), SimFxLogic::SubSteps(8.0, 0.25, 8), 8);

	// Trails within the nearer of the range and the visibility, plus the margin; a contrail to twice the range.
	FSimFxLod Lod;
	Lod.MaxRangeM = 40000.0;
	Lod.VisibilityM = 5000.0;
	TestEqual(TEXT("trail range: the visibility's 5 km and 2 km"), SimFxLogic::TrailRangeM(Lod, 1.0, 2000.0), 7000.0, 1e-9);
	TestEqual(TEXT("contrail range: in 5 km visibility too"), SimFxLogic::TrailRangeM(Lod, 2.0, 2000.0), 7000.0, 1e-9);
	Lod.VisibilityM = 100000.0;
	TestEqual(TEXT("trail range: clear air, the range's 40 km"), SimFxLogic::TrailRangeM(Lod, 1.0, 2000.0), 42000.0, 1e-9);
	TestEqual(TEXT("contrail range: clear air, twice it"), SimFxLogic::TrailRangeM(Lod, 2.0, 2000.0), 82000.0, 1e-9);

	// Who may take a ribbon still being laid: only a contrail is taken; a contrail takes one over 1.5 times as far.
	TestTrue(TEXT("take-over: smoke takes a contrail"), SimFxLogic::RibbonTakeOver(false, 30000.0, true, 1000.0, 1.5));
	TestFalse(TEXT("take-over: never smoke"), SimFxLogic::RibbonTakeOver(false, 100.0, false, 60000.0, 1.5));
	TestTrue(TEXT("take-over: a near contrail a far one"), SimFxLogic::RibbonTakeOver(true, 10000.0, true, 70000.0, 1.5));
	TestFalse(TEXT("take-over: not one nearly as near"), SimFxLogic::RibbonTakeOver(true, 40000.0, true, 50000.0, 1.5));
	TestFalse(TEXT("take-over: a contrail never smoke"), SimFxLogic::RibbonTakeOver(true, 100.0, false, 60000.0, 1.5));

	// The trails' mesh section: grows at once in 256-quad chunks; shrinks only after 30 frames under half.
	int32 Low = 0;
	bool bRebuild = false;
	TestEqual(TEXT("section: the first trail"), SimFxLogic::RibbonSectionQuads(10, 0, 256, 30, Low, bRebuild), 256);
	TestTrue(TEXT("section: made"), bRebuild);
	TestEqual(TEXT("section: grows"), SimFxLogic::RibbonSectionQuads(300, 256, 256, 30, Low, bRebuild), 512);
	TestTrue(TEXT("section: grown"), bRebuild);
	int32 Cap = 12288;
	int32 Rebuilt = 0;
	for (int32 F = 0; F < 30; ++F)
	{
		Cap = SimFxLogic::RibbonSectionQuads(100, Cap, 256, 30, Low, bRebuild);
		Rebuilt += bRebuild ? 1 : 0;
		if (F < 29)
		{
			TestEqual(TEXT("section: held while the spike may come back"), Cap, 12288);
		}
	}
	TestEqual(TEXT("section: shrunk to fit after 30 frames"), Cap, 256);
	TestEqual(TEXT("section: rebuilt once"), Rebuilt, 1);
	TestEqual(TEXT("section: the count reset"), Low, 0);
	Low = 5;
	TestEqual(TEXT("section: over half used, kept"), SimFxLogic::RibbonSectionQuads(600, 1024, 256, 30, Low, bRebuild), 1024);
	TestTrue(TEXT("section: and the count reset"), Low == 0 && !bRebuild);

	// The ribbon pool: a missile's smoke takes the farthest contrail; that contrail's aircraft releasing its stale index
	// leaves the missile's trail laid; a near contrail takes a far one, not a near one.
	TUniquePtr<FSimFxWorld> W = MakeUnique<FSimFxWorld>();
	W->Budgets.MaxRibbons = 2;
	W->CamPos = FVector::ZeroVector;
	const FSimFxRibbonStyle Style;
	auto Lay = [&W, &Style](int32& Ribbon, uint64 Key, const FVector& At)
	{
		FVector Last = FVector::ZeroVector;
		bool bHave = false;
		W->TrailPoint(Ribbon, Key, Last, bHave, At, 100.0f, 1.0f, 10.0, Style, FVector::ZeroVector);
	};
	const uint64 KeyA = FSimFxWorld::ContrailKey(1, 0), KeyB = FSimFxWorld::ContrailKey(2, 0), KeyC = FSimFxWorld::ContrailKey(3, 0),
		KeyD = FSimFxWorld::ContrailKey(4, 0);
	const uint64 KeyM = 7;
	int32 RibA = -1, RibB = -1, RibC = -1, RibD = -1, RibM = -1;
	Lay(RibA, KeyA, FVector(6.0e6, 0.0, 1.0e6));  // 60 km
	Lay(RibB, KeyB, FVector(5.0e6, 0.0, 1.0e6));  // 50 km
	TestTrue(TEXT("pool: two contrails"), RibA >= 0 && RibB >= 0 && RibA != RibB);
	Lay(RibM, KeyM, FVector(1.0e5, 0.0, 1.0e5));
	TestEqual(TEXT("pool: the missile takes the farthest contrail"), RibM, RibA);
	W->ReleaseRibbon(RibA, KeyA);  // A's aircraft leaves the contrail air the same frame, its index stale
	TestTrue(TEXT("pool: a stale release leaves the new owner's trail laid"), W->Ribbons[RibM].bEmitting && W->Ribbons[RibM].Owner == KeyM);
	Lay(RibC, KeyC, FVector(5.0e5, 0.0, 1.0e6));  // 5 km
	TestEqual(TEXT("pool: a near contrail takes a far one"), RibC, RibB);
	Lay(RibD, KeyD, FVector(4.5e6, 0.0, 1.0e6));  // 45 km
	TestEqual(TEXT("pool: a far contrail takes no near one, nor smoke"), RibD, -1);
	W->ReleaseRibbon(RibM, KeyM);
	TestFalse(TEXT("pool: its owner's release stops it"), W->Ribbons[RibM].bEmitting);

	// The same aircraft down twice: the first fall's smoke trail stops (fades and frees its ribbon), one wreck's effects.
	TUniquePtr<FSimFxWorld> K = MakeUnique<FSimFxWorld>();
	K->Budgets.MaxRibbons = 4;
	simcore::fx::Event Down;
	Down.kind = simcore::fx::EventKind::AircraftDown;
	Down.world_id = 7;
	Down.position = simcore::Vec3{0.0, 2000.0, 3000.0};
	Down.velocity = simcore::Vec3{200.0, 0.0, 0.0};
	Down.falls = true;
	Down.fire = true;
	K->AircraftDown(Down);
	if (TestEqual(TEXT("kill: one wreck's effects"), K->WreckFx.Num(), 1))
	{
		FSimFxWreckFx& F = K->WreckFx[0];
		K->TrailPoint(F.Ribbon, F.Key, F.LastTrailPos, F.bHaveTrail, FVector(2.0e5, 0.0, 3.0e5), 1000.0f, 1.0f, 50.0, Style, FVector::ZeroVector);
		const int32 First = F.Ribbon;
		TestTrue(TEXT("kill: its smoke laid"), First >= 0 && K->Ribbons[First].bEmitting);
		K->AircraftDown(Down);
		TestEqual(TEXT("kill again: still one wreck's effects"), K->WreckFx.Num(), 1);
		TestTrue(TEXT("kill again: the first smoke stops"), First >= 0 && !K->Ribbons[First].bEmitting);
	}
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
