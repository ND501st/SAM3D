#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SimBasicModels.h"
#include "SimEntityActor.h"
#include "SimFxSpriteLayer.h"
#include "SimMunitionModels.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A game world for one test, torn down by its destructor. */
	struct FSimFxWorldTestWorld
	{
		UWorld* World = nullptr;

		explicit FSimFxWorldTestWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FSimFxWorldTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** The widest a munition is round its axis: its body, its sections, rings and fins (tip to tip). */
	double SimFxWorldTestWidestM(const simcore::scenery::MunitionSpec& M)
	{
		double W = M.diameter_m;
		for (const simcore::scenery::MunitionSection& S : M.sections)
		{
			W = FMath::Max(W, S.diameter_m);
		}
		for (const simcore::scenery::MunitionRing& R : M.rings)
		{
			W = FMath::Max(W, R.diameter_m);
		}
		for (const simcore::scenery::MunitionFins& F : M.fins)
		{
			W = FMath::Max(W, F.span_m);
		}
		return W;
	}
}

// Every munition with a shape (docs/fx/README.md 3.2) builds a runtime mesh (FSimMunitionModels) with four LODs whose
// bounds are its length along X, centred on the body's middle, and at least its diameter across.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldMunitionMeshesTest, "SAM3D.Fx.World.MunitionMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldMunitionMeshesTest::RunTest(const FString& Parameters)
{
	const simcore::scenery::MunitionLibrary* Lib = FSimMunitionModels::Library();
	TestNotNull(TEXT("data/scenery/munitions.json loads"), Lib);
	if (Lib == nullptr)
	{
		return false;
	}
	TestNotNull(TEXT("a material for them"), FSimMunitionModels::Material());
	int32 Built = 0;
	for (const simcore::scenery::MunitionSpec& M : Lib->munitions)
	{
		const FString Id(UTF8_TO_TCHAR(M.id.c_str()));
		UStaticMesh* Mesh = FSimMunitionModels::Mesh(Id);
		if (!M.HasMesh())
		{
			TestNull(FString::Printf(TEXT("%s (a gun) has no mesh"), *Id), Mesh);
			continue;
		}
		if (!TestNotNull(FString::Printf(TEXT("%s builds"), *Id), Mesh))
		{
			continue;
		}
		++Built;
		TestEqual(FString::Printf(TEXT("%s: four LODs"), *Id), Mesh->GetNumLODs(), 4);
		TestTrue(FString::Printf(TEXT("%s: the same mesh the second time (cached)"), *Id), FSimMunitionModels::Mesh(Id) == Mesh);
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Size = Box.GetSize();
		const double LengthCm = M.length_m * 100.0;
		const double DiameterCm = M.diameter_m * 100.0;
		const double WidestCm = SimFxWorldTestWidestM(M) * 100.0;
		TestTrue(FString::Printf(TEXT("%s: %.1f cm long (spec %.1f)"), *Id, Size.X, LengthCm), FMath::Abs(Size.X - LengthCm) <= 0.02 * LengthCm);
		TestTrue(FString::Printf(TEXT("%s: centred on its middle (%.1f cm off)"), *Id, Box.GetCenter().X), FMath::Abs(Box.GetCenter().X) <= 0.02 * LengthCm);
		TestTrue(FString::Printf(TEXT("%s: at least its diameter across (%.1f x %.1f cm, spec %.1f)"), *Id, Size.Y, Size.Z, DiameterCm),
			Size.Y >= 0.98 * DiameterCm && Size.Z >= 0.98 * DiameterCm);
		TestTrue(FString::Printf(TEXT("%s: no wider than its widest part (%.1f x %.1f cm, widest %.1f)"), *Id, Size.Y, Size.Z, WidestCm),
			Size.Y <= 1.02 * WidestCm && Size.Z <= 1.02 * WidestCm);
	}
	TestTrue(FString::Printf(TEXT("munitions built: %d"), Built), Built > 0);
	for (const TCHAR* ModelId : {TEXT("missile_9m33"), TEXT("missile_arm"), TEXT("kh22m"), TEXT("tlam_c"), TEXT("agm86c")})
	{
		const simcore::scenery::MunitionSpec* Spec = FSimMunitionModels::Resolve(ModelId);
		TestNotNull(FString::Printf(TEXT("the basic model %s is a munition"), ModelId), Spec);
		TestNotNull(FString::Printf(TEXT("%s has its mesh"), ModelId), FSimMunitionModels::Mesh(ModelId));
	}
	// The air battle's (roadmap B13): every air-to-air missile of data/air_war/air_combat.json and the drop tank a fighter
	// throws off, with their meshes; its fighters' guns known (no mesh).
	for (const TCHAR* Id : {TEXT("k13"), TEXT("r13m"), TEXT("r60"), TEXT("r60m"), TEXT("r23r"), TEXT("r23t"), TEXT("r24r"), TEXT("r24t"),
			 TEXT("r73"), TEXT("r27r"), TEXT("r27t"), TEXT("aim9b"), TEXT("aim7e"), TEXT("aim9j"), TEXT("aim9l"), TEXT("aim9m"),
			 TEXT("aim7f"), TEXT("aim7m"), TEXT("drop_tank")})
	{
		const simcore::scenery::MunitionSpec* Spec = FSimMunitionModels::Resolve(Id);
		TestNotNull(FString::Printf(TEXT("the air battle's %s is a munition"), Id), Spec);
		TestNotNull(FString::Printf(TEXT("%s has its mesh"), Id), FSimMunitionModels::Mesh(Id));
	}
	for (const TCHAR* Id : {TEXT("gsh23l"), TEXT("m61a1"), TEXT("aden30"), TEXT("mg127"), TEXT("gsh6_30"), TEXT("gsh6_23"),
			 TEXT("gsh30_2"), TEXT("nr30"), TEXT("bk27"), TEXT("gau8_air")})
	{
		const simcore::scenery::MunitionSpec* Spec = FSimMunitionModels::Resolve(Id);
		TestTrue(FString::Printf(TEXT("the fighters' gun %s is known"), Id), Spec != nullptr && Spec->kind == "gun");
	}
	TestNull(TEXT("an unknown id has no mesh"), FSimMunitionModels::Mesh(TEXT("no_such_munition")));
	return true;
}

// A model id that is a munition builds one component with the munition's mesh, casting no shadow; an aircraft keeps
// its basic model's parts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldMunitionComponentTest, "SAM3D.Fx.World.MunitionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldMunitionComponentTest::RunTest(const FString& Parameters)
{
	FSimFxWorldTestWorld W(TEXT("SAM3DFxWorldMunitionComponent"));
	AActor* Holder = W.World->SpawnActor<ASimEntityActor>();
	if (!TestNotNull(TEXT("an entity actor"), Holder))
	{
		return false;
	}
	USceneComponent* Root = Holder->GetRootComponent();
	for (const TCHAR* ModelId : {TEXT("missile_9m33"), TEXT("missile_arm"), TEXT("kh22m")})
	{
		TArray<UStaticMeshComponent*> Parts;
		const int32 N = FSimBasicModels::Build(ModelId, Root, &Parts);
		TestEqual(FString::Printf(TEXT("%s: one component"), ModelId), N, 1);
		if (Parts.Num() == 1)
		{
			TestTrue(FString::Printf(TEXT("%s: the munition's mesh"), ModelId), Parts[0]->GetStaticMesh() == FSimMunitionModels::Mesh(ModelId));
			TestFalse(FString::Printf(TEXT("%s: no shadow"), ModelId), Parts[0]->CastShadow != 0);
			TestTrue(FString::Printf(TEXT("%s: no collision"), ModelId), Parts[0]->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		}
	}
	{
		TArray<UStaticMeshComponent*> Parts;
		const int32 N = FSimBasicModels::Build(TEXT("f4e"), Root, &Parts);
		TestTrue(FString::Printf(TEXT("an aircraft keeps its parts (%d)"), N), N > 1);
		TestNull(TEXT("and is no munition"), FSimBasicModels::MunitionMesh(TEXT("f4e")));
	}
	TestNull(TEXT("the Strela-2 in a man's hands is equipment, not a munition"), FSimBasicModels::MunitionMesh(TEXT("strela2")));
	ASimEntityActor* Missile = W.World->SpawnActor<ASimEntityActor>();
	TestTrue(TEXT("an entity actor wears missile_9m33"), Missile != nullptr && Missile->UseBasicModel(TEXT("missile_9m33")));
	return true;
}

// The entity snapshot (docs/fx/README.md 4.3): built once a frame at the end of the tick, after its steps, the same states
// by id as in the list (O(1) through its map), each stamped with its sim time; a display id never listed answers not alive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldEntityCacheTest, "SAM3D.Fx.World.EntityCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldEntityCacheTest::RunTest(const FString& Parameters)
{
	FSimFxWorldTestWorld W(TEXT("SAM3DFxWorldEntityCache"));
	USimWorldSubsystem* Sim = W.World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the stand-off jammer scenario loads %s"), *Error), Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_soj_strike.json"), Error)))
	{
		return false;
	}
	for (int32 I = 0; I < 40; ++I)
	{
		Sim->Tick(0.05f);
	}
	const TArray<FSimEntityState>& Cached = Sim->GetEntityStatesCached();
	TestTrue(FString::Printf(TEXT("entities listed: %d"), Cached.Num()), Cached.Num() > 0);
	// A rebuild swaps the snapshot's storage with the last one's: the same storage is the same snapshot.
	const FSimEntityState* const Storage = Cached.GetData();
	TestTrue(TEXT("the same snapshot within a frame (not rebuilt)"), Sim->GetEntityStatesCached().GetData() == Storage);
	const TArray<FSimEntityState> List = Sim->GetEntityStates();
	TestEqual(TEXT("the Blueprint list is the snapshot"), List.Num(), Cached.Num());
	const double Now = Sim->GetSimTimeSeconds();
	for (const FSimEntityState& S : List)
	{
		FSimEntityState One;
		TestTrue(FString::Printf(TEXT("entity %d found by id"), S.Id), Sim->GetEntityState(S.Id, One));
		TestEqual(FString::Printf(TEXT("entity %d: the same name"), S.Id), One.Name, S.Name);
		TestTrue(FString::Printf(TEXT("entity %d: the same place"), S.Id), One.Location.Equals(S.Location, 1e-6));
		TestTrue(FString::Printf(TEXT("entity %d: the same velocity"), S.Id), One.Velocity.Equals(S.Velocity, 1e-6));
		TestTrue(FString::Printf(TEXT("entity %d: alive the same"), S.Id), One.bAlive == S.bAlive);
		TestEqual(FString::Printf(TEXT("entity %d: the same model"), S.Id), One.Model, S.Model);
		TestTrue(FString::Printf(TEXT("entity %d: stamped with the sim time"), S.Id), FMath::IsNearlyEqual(S.SimTimeSeconds, Now, 1e-9));
		const FSimEntityState* Ptr = Sim->FindEntityStateCached(S.Id);
		TestTrue(FString::Printf(TEXT("entity %d: in the snapshot"), S.Id), Ptr != nullptr && Ptr->Id == S.Id);
	}
	FSimEntityState None;
	TestFalse(TEXT("an id never listed is not found"), Sim->GetEntityState(4242, None));
	FSimEntityState Display;
	TestTrue(TEXT("a display id never listed answers"), Sim->GetEntityState(123456, Display));
	TestTrue(TEXT("as not alive"), Display.Id == 123456 && !Display.bAlive);
	// The tick rebuilds it after its steps: the aircraft move and the stamp is the new time; read again, the same one.
	Sim->Tick(0.05f);
	const TArray<FSimEntityState>& After = Sim->GetEntityStatesCached();
	TestTrue(TEXT("rebuilt after a step"), After.Num() > 0 && After[0].SimTimeSeconds > Now);
	const FSimEntityState* const AfterStorage = After.GetData();
	TestTrue(TEXT("one snapshot after the tick (not rebuilt by a read)"), Sim->GetEntityStatesCached().GetData() == AfterStorage);
	return true;
}

// Drawn at render time (docs/fx/README.md 4.3): sim time plus the step's leftover; a state carried on by its velocity,
// at most 0.15 s; paused, the sim time itself.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldRenderTimeTest, "SAM3D.Fx.World.RenderTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldRenderTimeTest::RunTest(const FString& Parameters)
{
	FSimFxWorldTestWorld W(TEXT("SAM3DFxWorldRenderTime"));
	USimWorldSubsystem* Sim = W.World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the stand-off jammer scenario loads %s"), *Error), Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_soj_strike.json"), Error)))
	{
		return false;
	}
	const float Scale = Sim->GetTimeScale();
	Sim->SetTimeScale(1.0f);
	Sim->Tick(0.05f);
	Sim->Tick(0.07f);  // one more 0.05 s step and 0.02 s left over
	const double Now = Sim->GetSimTimeSeconds();
	const double Render = Sim->GetRenderTimeSeconds();
	TestTrue(FString::Printf(TEXT("render time %.4f s: the sim's %.4f s and the leftover"), Render, Now), Render - Now > 0.015 && Render - Now < 0.025);
	const FSimEntityState* Moving = nullptr;
	for (const FSimEntityState& S : Sim->GetEntityStatesCached())
	{
		if (S.bAlive && !S.bIsGround && S.Velocity.Size() > 1000.0)
		{
			Moving = &S;
			break;
		}
	}
	if (TestNotNull(TEXT("an aircraft flying"), Moving))
	{
		const FVector Drawn = Sim->GetRenderLocation(*Moving);
		const FVector Want = Moving->Location + Moving->Velocity * (Render - Moving->SimTimeSeconds);
		TestTrue(FString::Printf(TEXT("carried on by its velocity (%.3f cm off)"), FVector::Dist(Drawn, Want)), Drawn.Equals(Want, 0.01));
		FSimEntityState Old = *Moving;
		Old.SimTimeSeconds -= 10.0;
		const double Ahead = FVector::Dist(Sim->GetRenderLocation(Old), Old.Location) / Old.Velocity.Size();
		TestTrue(FString::Printf(TEXT("never more than 0.15 s ahead (%.3f s)"), Ahead), FMath::IsNearlyEqual(Ahead, 0.15, 1e-6));
		FSimEntityState Dead = *Moving;
		Dead.bAlive = false;
		TestTrue(TEXT("not alive: where it was"), Sim->GetRenderLocation(Dead).Equals(Dead.Location, 1e-6));
	}
	Sim->SetSimPaused(true);
	TestTrue(TEXT("paused: the sim time"), FMath::IsNearlyEqual(Sim->GetRenderTimeSeconds(), Sim->GetSimTimeSeconds(), 1e-12));
	Sim->SetSimPaused(false);
	Sim->SetTimeScale(Scale);
	return true;
}

// Our missile in the practice shoot (docs/fx/README.md 3.3, 4.3): from the feed's MissileLaunch it is drawn out of the
// launcher along its launch line and gathers onto the simulation's line by 2 s; it wears its munition's mesh; once it
// has finished it is given as not alive where it was last drawn, not at the origin.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldMissileDisplayTest, "SAM3D.Fx.World.MissileDisplayPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldMissileDisplayTest::RunTest(const FString& Parameters)
{
	FSimFxWorldTestWorld W(TEXT("SAM3DFxWorldMissileDisplay"));
	USimWorldSubsystem* Sim = W.World->GetSubsystem<USimWorldSubsystem>();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the practice shoot loads %s"), *Error), Sim != nullptr && Sim->LoadScenario(TEXT("scenarios/osa_practice.json"), Error)))
	{
		return false;
	}
	const float Scale = Sim->GetTimeScale();
	Sim->SetTimeScale(1.0f);
	simcore::ScenarioRunner* Fx = Sim->GetFxRunner();
	if (!TestNotNull(TEXT("the runner that fires"), Fx))
	{
		Sim->SetTimeScale(Scale);
		return false;
	}
	// To the first launch, a quarter second a tick (the crew's drill, the drone's approach: several minutes).
	simcore::fx::Event Launch;
	bool bLaunched = false;
	uint64 Cursor = 0;
	while (!bLaunched && Fx->Time() < 900.0 && !Fx->Finished())
	{
		Sim->Tick(0.25f);
		for (const simcore::fx::Event& E : Fx->Fx().Since(Cursor))
		{
			if (E.kind == simcore::fx::EventKind::MissileLaunch && !bLaunched)
			{
				Launch = E;
				bLaunched = true;
			}
		}
		Cursor = Fx->Fx().Last();
	}
	if (!TestTrue(FString::Printf(TEXT("a missile launched (by %.0f s)"), Fx->Time()), bLaunched))
	{
		Sim->SetTimeScale(Scale);
		return false;
	}
	const int32 Id = 100000 + Launch.number;  // USimWorldSubsystem's display ids for our missiles
	auto SimMissile = [Fx, &Launch](simcore::Vec3& OutPos) -> bool
	{
		for (const simcore::EngagementModel::MissileView& M : Fx->MissilesInFlight())
		{
			if (M.number == Launch.number)
			{
				OutPos = M.position;
				return true;
			}
		}
		return false;
	};
	// Its first seconds, one sim step a tick.
	bool bDeparted = false;
	bool bGathered = false;
	bool bModel = false;
	FVector Last = FVector::ZeroVector;
	double WorstM = 0.0;
	for (int32 I = 0; I < 80; ++I)
	{
		simcore::Vec3 SimPos;
		if (!SimMissile(SimPos))
		{
			break;  // a short flight: it has ended
		}
		const FSimEntityState* S = Sim->FindEntityStateCached(Id);
		if (!TestNotNull(FString::Printf(TEXT("missile %d listed at %.2f s"), Launch.number, Fx->Time()), S))
		{
			break;
		}
		if (!bModel)
		{
			bModel = true;
			TestTrue(FString::Printf(TEXT("it wears a munition's mesh (%s)"), *S->Model), FSimMunitionModels::Mesh(S->Model) != nullptr);
			TestTrue(TEXT("a missile, alive"), S->bIsMissile && S->bAlive);
		}
		const double Since = Fx->Time() - Launch.t_s;
		const simcore::Vec3 Want = simcore::fx::MissileDisplayPosition(Launch.position, Launch.velocity, Since, SimPos);
		const simcore::Vec3 Drawn = Sim->ToEnu(S->Location);
		WorstM = FMath::Max(WorstM, (Drawn - Want).Length());
		bDeparted = bDeparted || (Since < 1.5 && (Want - SimPos).Length() > 1.0);
		if (Since >= 2.0)
		{
			bGathered = true;
			TestTrue(FString::Printf(TEXT("on the simulation's line from 2 s (%.2f m off at %.2f s)"), (Drawn - SimPos).Length(), Since),
				(Drawn - SimPos).Length() < 0.1);
		}
		Last = S->Location;
		Sim->Tick(0.05f);
	}
	TestTrue(FString::Printf(TEXT("drawn on the display path (worst %.3f m off)"), WorstM), WorstM < 0.1);
	TestTrue(TEXT("its first second on the launch line, apart from the simulation's"), bDeparted);
	TestTrue(TEXT("gathered onto the simulation's line"), bGathered);
	// To its end: then not alive, where it was last drawn.
	for (int32 I = 0; I < 400; ++I)
	{
		simcore::Vec3 SimPos;
		if (!SimMissile(SimPos))
		{
			break;
		}
		if (const FSimEntityState* S = Sim->FindEntityStateCached(Id))
		{
			Last = S->Location;
		}
		Sim->Tick(0.25f);
	}
	simcore::Vec3 Unused;
	if (TestFalse(TEXT("the missile has finished"), SimMissile(Unused)))
	{
		Sim->GetEntityStatesCached();  // the snapshot after its end
		FSimEntityState Gone;
		TestTrue(TEXT("still known by its id"), Sim->GetEntityState(Id, Gone));
		TestFalse(TEXT("not alive"), Gone.bAlive);
		TestTrue(FString::Printf(TEXT("where it was last drawn (%.1f m off), not at the origin"), FVector::Dist(Gone.Location, Last) / 100.0),
			Gone.Location.Equals(Last, 1.0) && !Gone.Location.IsNearlyZero(100.0));
	}
	Sim->SetTimeScale(Scale);
	return true;
}

// A sprite layer's bounds follow what it draws (USimFxSpriteLayerComponent): instances moved without a render-state
// rebuild, as the effects move theirs each frame, and the bounds the subsystem sets hold them and not the spares' place.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFxWorldSpriteLayerBoundsTest, "SAM3D.Fx.World.SpriteLayerBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimFxWorldSpriteLayerBoundsTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!TestNotNull(TEXT("the engine's plane"), Plane))
	{
		return false;
	}
	FSimFxWorldTestWorld W(TEXT("SAM3DFxWorldSpriteLayerBounds"));
	AActor* Holder = W.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("a host actor"), Holder))
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"), RF_Transient);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	// Four hidden spares at the origin, then the first moved to a burst 500 m off and 300 m up, 5 m across.
	const FVector Burst(40000.0, 30000.0, 30000.0);
	auto Make = [&](UClass* Class, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(Holder, Class, Name, RF_Transient);
		Ism->SetStaticMesh(Plane);
		Ism->SetupAttachment(Root);
		Ism->RegisterComponent();
		TArray<FTransform> Spares;
		Spares.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1.0e-3)), 4);
		Ism->AddInstances(Spares, false, false, false);
		Ism->UpdateBounds();
		TArray<FTransform> Moved;
		Moved.Add(FTransform(FQuat::Identity, Burst, FVector(5.0, 5.0, 1.0)));
		Ism->BatchUpdateInstancesTransforms(0, Moved, false, false, false);
		return Ism;
	};

	UInstancedStaticMeshComponent* Stock = Make(UInstancedStaticMeshComponent::StaticClass(), TEXT("Stock"));
	Stock->UpdateBounds();
	AddInfo(FString::Printf(TEXT("the stock component's bounds after the move: %s (holds the burst: %s)"), *Stock->Bounds.GetBox().ToString(),
		Stock->Bounds.GetBox().IsInside(Burst) ? TEXT("yes") : TEXT("no")));

	USimFxSpriteLayerComponent* Layer = Cast<USimFxSpriteLayerComponent>(Make(USimFxSpriteLayerComponent::StaticClass(), TEXT("Layer")));
	if (!TestNotNull(TEXT("a sprite layer"), Layer))
	{
		return false;
	}
	const double Half = 0.5 * FMath::Sqrt(2.0) * 500.0;
	Layer->SetFxBounds(FBox(Burst - FVector(Half), Burst + FVector(Half)));
	Layer->UpdateBounds();
	const FBox Box = Layer->Bounds.GetBox();
	TestTrue(TEXT("the layer's bounds hold the burst"), Box.IsInside(Burst));
	TestFalse(TEXT("and not the spares' old place"), Box.IsInside(FVector::ZeroVector));
	TestTrue(FString::Printf(TEXT("and are the sprite's size (%s)"), *Box.GetSize().ToString()), Box.GetSize().Equals(FVector(2.0 * Half), 1.0));

	// The host moved: the bounds go with the component (they are set in its space).
	Holder->SetActorLocation(FVector(100000.0, 0.0, 0.0));
	Layer->UpdateBounds();
	TestTrue(TEXT("the bounds follow the component"), Layer->Bounds.GetBox().IsInside(Burst + FVector(100000.0, 0.0, 0.0)));

	// Nothing drawn: the stock bounds again.
	Layer->SetFxBounds(FBox(ForceInit));
	Layer->UpdateBounds();
	TestTrue(TEXT("no box: the stock bounds"), Layer->Bounds.SphereRadius >= 0.0);
	return true;
}

#endif
