// The weapons' effects, drawn (docs/fx/README.md 4.1, 5): the frame (the clock, the views, the sun, the feed read with
// its own cursor) and the back end every effect is drawn by: one instanced quad per sprite layer, one procedural mesh for
// every ribbon, pools of lights, decals and munition meshes, all made once on a transient host actor. The effects
// themselves (events to effects, moving them, the frame's draw lists) are SimFxEffects.cpp's.

#include "SimFxSubsystem.h"

#include "SimFxImpl.h"
#include "SimFxShots.h"
#include "SimFxSpriteLayer.h"
#include "SimMunitionModels.h"
#include "SimWorldSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimFxSubsystem, Log, All);

DECLARE_CYCLE_STAT(TEXT("Fx frame"), STAT_SimFxSubTick, STATGROUP_SAM3DFx);
DECLARE_CYCLE_STAT(TEXT("Fx sprites upload"), STAT_SimFxSubSprites, STATGROUP_SAM3DFx);
DECLARE_CYCLE_STAT(TEXT("Fx ribbons build"), STAT_SimFxSubRibbons, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx sprites"), STAT_SimFxSubSpriteCount, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx particles"), STAT_SimFxSubParticleCount, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx ribbons"), STAT_SimFxSubRibbonCount, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx ribbon points"), STAT_SimFxSubRibbonPointCount, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx lights"), STAT_SimFxSubLightCount, STATGROUP_SAM3DFx);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fx munitions"), STAT_SimFxSubMunitionCount, STATGROUP_SAM3DFx);

namespace
{
	TAutoConsoleVariable<int32> CVarSimFxSubEnable(TEXT("sam3d.fx.Enable"), 1,
		TEXT("The weapons' effects: 1 drawn, 0 none (docs/fx/README.md 4.1)."));
	TAutoConsoleVariable<int32> CVarSimFxSubMaxSprites(TEXT("sam3d.fx.MaxSprites"), 4000,
		TEXT("The effects' sprites (particles and quads) drawn at most in a frame."));
	TAutoConsoleVariable<int32> CVarSimFxSubMaxRibbons(TEXT("sam3d.fx.MaxRibbons"), 48,
		TEXT("Smoke trails and contrails kept at most."));
	TAutoConsoleVariable<int32> CVarSimFxSubMaxLights(TEXT("sam3d.fx.MaxLights"), 8,
		TEXT("Shadowless point lights the effects light the scene with at most (the brightest nearest)."));
	TAutoConsoleVariable<int32> CVarSimFxSubMaxDecals(TEXT("sam3d.fx.MaxDecals"), 32,
		TEXT("Scorch decals kept at most (the oldest reused)."));
	TAutoConsoleVariable<int32> CVarSimFxSubMaxMunitions(TEXT("sam3d.fx.MaxMunitions"), 48,
		TEXT("Munition meshes (bombs, rockets, missiles in flight) drawn at most."));
	TAutoConsoleVariable<float> CVarSimFxSubMaxRange(TEXT("sam3d.fx.MaxRange"), 40000.0f,
		TEXT("Metres: no effect is drawn farther than this from every view."));
	TAutoConsoleVariable<float> CVarSimFxSubFullPx(TEXT("sam3d.fx.FullPx"), 48.0f,
		TEXT("Pixels: an effect this large on a view is drawn in full; smaller, as an impostor."));
	TAutoConsoleVariable<float> CVarSimFxSubImpostorPx(TEXT("sam3d.fx.ImpostorPx"), 6.0f,
		TEXT("Pixels: under this an effect is not drawn by day (a glint at night)."));
	TAutoConsoleVariable<float> CVarSimFxSubLogSpikes(TEXT("sam3d.fx.LogSpikes"), 0.0f,
		TEXT("Milliseconds: log the effects' frames that take longer, stage by stage (0: off)."));
	TAutoConsoleVariable<float> CVarSimFxSubEmissiveScale(TEXT("sam3d.fx.EmissiveScale"), 1.0f,
		TEXT("Scales every flash, flame, tracer and effect light."));

	// Per-instance custom data (tools/ue_make_fx_assets.py's contract): frame, opacity, heat, tint rgb, erosion, lit, kind,
	// the flashes' and fires' light rgb (T9), the soft-particle fade distance in cm.
	constexpr int32 SimFxSubFloats = 13;
	constexpr float SimFxSubFadeShare = 0.3f;    // DepthFade over 0.3 x the sprite's width (LOOK T2: 0.3-0.5 x its radius)
	constexpr float SimFxSubRibbonMinPx = 1.2f;  // a ribbon's pixel floor (T10, the art review's D8)
	constexpr int32 SimFxSubSmokeLights = 8;     // the frame's first lights that light the smoke (T9)
	constexpr int32 SimFxSubInstanceChunk = 64;
	constexpr int32 SimFxSubQuadChunk = 256;
	// The trails' section shrinks once under half of it has been used this many frames running (about half a second).
	constexpr int32 SimFxSubRibbonShrinkFrames = 30;
	constexpr float SimFxSubHiddenScale = 1.0e-3f;
	constexpr int32 SimFxSubHideAfterFrames = 300;  // a layer empty this long is hidden (about 5 s at 60 fps)

	const TCHAR* const SimFxSubMaterialRoot = TEXT("/Game/SAM3D/FX/Materials/");
	const TCHAR* const SimFxSubTextureRoot = TEXT("/Game/SAM3D/FX/Textures/");

	template <typename T>
	T* SimFxSubLoad(const TCHAR* Root, const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), Root, Name, Name);
		return LoadObject<T>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}

	/** A point light of the pool: shadowless, inverse-squared, lumens, hidden until used. */
	UPointLightComponent* SimFxSubMakeLight(AActor* Host, USceneComponent* Root, int32 Index)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Host, *FString::Printf(TEXT("FxLight%d"), Index), RF_Transient);
		L->SetMobility(EComponentMobility::Movable);
		L->SetCastShadows(false);
		L->bUseInverseSquaredFalloff = true;
		L->IntensityUnits = ELightUnits::Lumens;
		L->Intensity = 0.0f;
		L->AttenuationRadius = 1000.0f;
		L->SetVisibility(false);
		L->SetupAttachment(Root);
		L->RegisterComponent();
		return L;
	}

	UStaticMeshComponent* SimFxSubMakeMunition(AActor* Host, USceneComponent* Root, int32 Index)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Host, *FString::Printf(TEXT("FxMunition%d"), Index), RF_Transient);
		M->SetMobility(EComponentMobility::Movable);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->SetCastShadow(false);
		M->SetVisibleInRayTracing(false);
		M->SetAffectDistanceFieldLighting(false);
		M->SetVisibility(false);
		M->SetupAttachment(Root);
		M->RegisterComponent();
		if (UMaterialInterface* Mat = FSimMunitionModels::Material())
		{
			M->SetMaterial(0, Mat);
		}
		return M;
	}

	/** The engine plane's texture axes in its own XY plane, from three of its vertices (false if they can't be read). */
	bool SimFxSubPlaneAxes(UStaticMesh* Plane, FVector& OutU, FVector& OutV)
	{
		if (Plane == nullptr)
		{
			return false;
		}
		const FStaticMeshRenderData* RD = Plane->GetRenderData();
		if (RD == nullptr || RD->LODResources.Num() == 0)
		{
			return false;
		}
		const FStaticMeshLODResources& Lod = RD->LODResources[0];
		const FPositionVertexBuffer& Pos = Lod.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& Smv = Lod.VertexBuffers.StaticMeshVertexBuffer;
		const uint32 Num = Pos.GetNumVertices();
		if (Num < 3 || Pos.GetVertexData() == nullptr || Smv.GetTexCoordData() == nullptr || Smv.GetNumVertices() < Num)
		{
			return false;
		}
		// Three vertices not in a line: U = a x + b y + c over the plane, solved from them; (a, b) is where U grows.
		for (uint32 I1 = 1; I1 < Num; ++I1)
		{
			for (uint32 I2 = I1 + 1; I2 < Num; ++I2)
			{
				const FVector3f P0 = Pos.VertexPosition(0), P1 = Pos.VertexPosition(I1), P2 = Pos.VertexPosition(I2);
				const double Dx1 = P1.X - P0.X, Dy1 = P1.Y - P0.Y, Dx2 = P2.X - P0.X, Dy2 = P2.Y - P0.Y;
				const double Det = Dx1 * Dy2 - Dx2 * Dy1;
				if (FMath::Abs(Det) < 1.0)
				{
					continue;
				}
				const FVector2f T0 = Smv.GetVertexUV(0, 0), T1 = Smv.GetVertexUV(I1, 0), T2 = Smv.GetVertexUV(I2, 0);
				auto Grad = [&](double D1, double D2) {
					return FVector((D1 * Dy2 - D2 * Dy1) / Det, (Dx1 * D2 - Dx2 * D1) / Det, 0.0);
				};
				const FVector Gu = Grad(T1.X - T0.X, T2.X - T0.X);
				const FVector Gv = Grad(T1.Y - T0.Y, T2.Y - T0.Y);
				auto Snap = [](const FVector& G) {
					return FMath::Abs(G.X) >= FMath::Abs(G.Y) ? FVector(FMath::Sign(G.X), 0.0, 0.0) : FVector(0.0, FMath::Sign(G.Y), 0.0);
				};
				if (Gu.IsNearlyZero(1e-6) || Gv.IsNearlyZero(1e-6))
				{
					return false;
				}
				OutU = Snap(Gu);
				OutV = Snap(Gv);
				return FMath::Abs(OutU | OutV) < 0.5;
			}
		}
		return false;
	}

	/** How lit a puff's side to the camera is: 1 with the sun behind the camera, Backlit with the sun behind the puff. */
	float SimFxSubLit(const FVector& ToSun, const FVector& ToCam, double Backlit)
	{
		const double D = FVector::DotProduct(ToSun, ToCam);
		return static_cast<float>(FMath::Lerp(Backlit, 1.0, 0.5 * (1.0 + D)));
	}
}

// ------------------------------------------------------------------ subsystem

void USimFxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USimWorldSubsystem>();
	Fx = MakePimpl<FSimFxWorld>();

	const UWorld* World = GetWorld();
	const USimWorldSubsystem* Sim = World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
	if (Sim != nullptr)
	{
		const FString Path = FPaths::Combine(Sim->GetDataDirectory(), TEXT("fx"), TEXT("effects.json"));
		FString Error;
		if (!Fx->Tune.Load(Path, &Error))
		{
			UE_LOG(LogSimFxSubsystem, Warning, TEXT("Effects tuning %s not read (%s): the defaults are used."), *Path, *Error);
		}
	}
	LayerCapacity.Init(0, SimFxLayerCount);
	LayerShown.Init(0, SimFxLayerCount);
	LayerEmptyFrames.Init(0, SimFxLayerCount);
}

void USimFxSubsystem::Deinitialize()
{
	Shots.Reset();
	if (Host != nullptr && IsValid(Host))
	{
		Host->Destroy();
	}
	Host = nullptr;
	Layers.Reset();
	LayerMaterials.Reset();
	RibbonMesh = nullptr;
	RibbonMaterial = nullptr;
	LightPool.Reset();
	DecalPool.Reset();
	DecalMaterials.Reset();
	DecalBornT.Reset();
	DecalOpacity.Reset();
	MunitionPool.Reset();
	ScorchMaterial = nullptr;
	bBackEnd = false;
	Fx.Reset();
	Super::Deinitialize();
}

bool USimFxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// The effects are for a viewer: a dedicated server simulates and draws nothing (as USimFxAudioSubsystem). A listen
	// server has its player and keeps them.
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

bool USimFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USimFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimFxSubsystem, STATGROUP_Tickables);
}

void USimFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// -SimFxShots=<folder> [-SimFxShotsNight], or -SimFxShotsNight=<folder>.
	FString Folder;
	bool bNight = false;
	if (FParse::Value(FCommandLine::Get(), TEXT("-SimFxShotsNight="), Folder) && !Folder.IsEmpty())
	{
		bNight = true;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("-SimFxShots="), Folder) && !Folder.IsEmpty())
	{
		bNight = FParse::Param(FCommandLine::Get(), TEXT("SimFxShotsNight"));
	}
	if (!Folder.IsEmpty())
	{
		Shots = MakePimpl<FSimFxShots>(&InWorld, Folder, bNight);
		UE_LOG(LogSimFxSubsystem, Log, TEXT("Effects photographer started: %s%s"), *Folder, bNight ? TEXT(" (night)") : TEXT(""));
	}
}

void USimFxSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_SimFxSubTick);

	auto Frame = [this, DeltaTime]()
	{
		UWorld* World = GetWorld();
		USimWorldSubsystem* Sim = World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
		simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetFxRunner() : nullptr;
		if (!Fx.IsValid() || CVarSimFxSubEnable.GetValueOnGameThread() == 0 || World == nullptr || Runner == nullptr)
		{
			HideAll();
			return;
		}
		bHidden = false;
		FSimFxWorld& W = *Fx;
		W.Sim = Sim;
		W.Runner = Runner;

		// The clock: the moment drawn, frozen while paused; a hitch's cap scaled with the time compression, as the
		// simulation's own step is, so the effects keep the clock their events are timed by.
		W.Now = Sim->GetRenderTimeSeconds();
		W.Dt = (W.LastNow < 0.0 || Sim->IsSimPaused()) ? 0.0f
			: static_cast<float>(SimFxLogic::FrameDt(W.Now - W.LastNow, static_cast<double>(Sim->GetTimeScale()), 0.25));
		W.LastNow = W.Now;
		++W.Frame;

		// The budgets and the detail thresholds.
		W.Budgets.MaxSprites = FMath::Max(CVarSimFxSubMaxSprites.GetValueOnGameThread(), 0);
		W.Budgets.MaxRibbons = FMath::Max(CVarSimFxSubMaxRibbons.GetValueOnGameThread(), 0);
		W.Budgets.MaxLights = FMath::Max(CVarSimFxSubMaxLights.GetValueOnGameThread(), 0);
		W.Budgets.MaxDecals = FMath::Max(CVarSimFxSubMaxDecals.GetValueOnGameThread(), 0);
		W.Budgets.MaxMunitions = FMath::Max(CVarSimFxSubMaxMunitions.GetValueOnGameThread(), 0);
		W.Budgets.EmissiveScale = FMath::Max(static_cast<double>(CVarSimFxSubEmissiveScale.GetValueOnGameThread()), 0.0);
		W.Lod.FullPx = CVarSimFxSubFullPx.GetValueOnGameThread();
		W.Lod.ImpostorPx = CVarSimFxSubImpostorPx.GetValueOnGameThread();
		W.Lod.MaxRangeM = CVarSimFxSubMaxRange.GetValueOnGameThread();
		W.Lod.Margin = W.Tune.Lod.ViewMargin;
		W.Lod.SpeckPx = W.Tune.Lod.SpeckPx;
		W.Lod.VisibilityM = FMath::Max(Runner->GetWeather().Preset().visibility_km, 0.1) * 1000.0;

		// The frame's one snapshot, by id.
		W.States = &Sim->GetEntityStatesCached();
		W.StateIndex.Reset();
		for (int32 I = 0; I < W.States->Num(); ++I)
		{
			W.StateIndex.Add((*W.States)[I].Id, I);
		}

		// Stage times for sam3d.fx.LogSpikes (what a slow frame spent its time on).
		double StageMs[9] = {};
		uint64 StageT = FPlatformTime::Cycles64();
		auto Stage = [&StageMs, &StageT](int32 Index)
		{
			const uint64 T = FPlatformTime::Cycles64();
			StageMs[Index] += FPlatformTime::ToMilliseconds64(T - StageT);
			StageT = T;
		};
		if (!bBackEnd)
		{
			MakeBackEnd();
		}
		UpdateViews();
		SunTimer -= DeltaTime;
		if (SunTimer <= 0.0 || !W.Sun.bFound)
		{
			SunTimer = 1.0;
			UpdateSun();
		}
		Stage(0);
		W.Lod.bNight = W.Sun.bNight;

		// The frame's lists.
		for (int32 L = 0; L < SimFxLayerCount; ++L)
		{
			W.Sprites[L].Reset();
		}
		W.SpriteCount = 0;
		W.Meshes.Reset();
		W.Lights.Reset();
		W.NewDecals.Reset();

		// The feed: what happened since last frame (all of it again after a new scenario or a restart).
		const simcore::fx::Feed& Feed = Runner->Fx();
		if (W.Cursor.Sync(Runner, Feed.Last()))
		{
			ClearEffects();
			W.TimeBase = W.Now;
		}
		const std::vector<simcore::fx::Event> Events = Feed.Since(W.Cursor.Seq);
		for (const simcore::fx::Event& E : Events)
		{
			W.OnEvent(E);
			W.Cursor.Seq = E.seq;
		}
		Stage(1);
		W.UpdateEffects();
		Stage(2);

		// The lights brightest first (by their light at the camera): the first few light the smoke and the trails (T9),
		// and the light pool takes the same order. The trails before the sprites: near the camera a trail adds puffs.
		W.Lights.Sort([](const FSimFxLightDraw& A, const FSimFxLightDraw& B) { return A.Score > B.Score; });
		const int32 QuadsBefore = RibbonOrderCount;
		DrawRibbons();
		Stage(3);
		DrawSprites();
		Stage(4);
		DrawLights();
		Stage(5);
		DrawDecals();
		Stage(6);
		DrawMunitions();
		Stage(7);
		const float SpikeMs = CVarSimFxSubLogSpikes.GetValueOnGameThread();
		double TotalMs = 0.0;
		for (const double Ms : StageMs)
		{
			TotalMs += Ms;
		}
		if (SpikeMs > 0.0f && TotalMs > SpikeMs)
		{
			UE_LOG(LogSimFxSubsystem, Log,
				TEXT("Effects frame %.2f ms: views and sun %.2f, events %.2f (%d), effects %.2f, trails %.2f (%d points, %d quads%s), sprites %.2f (%d), lights %.2f, decals %.2f, munitions %.2f"),
				TotalMs, StageMs[0], StageMs[1], static_cast<int32>(Events.size()), StageMs[2], StageMs[3], W.StatRibbonPoints, RibbonOrderCount,
				RibbonOrderCount != QuadsBefore ? TEXT(", rebuilt") : TEXT(""), StageMs[4], W.SpriteCount, StageMs[5], StageMs[6], StageMs[7]);
		}

		SET_DWORD_STAT(STAT_SimFxSubSpriteCount, W.SpriteCount);
		SET_DWORD_STAT(STAT_SimFxSubParticleCount, W.Particles.Num());
		SET_DWORD_STAT(STAT_SimFxSubRibbonCount, W.StatRibbons);
		SET_DWORD_STAT(STAT_SimFxSubRibbonPointCount, W.StatRibbonPoints);
		SET_DWORD_STAT(STAT_SimFxSubLightCount, W.StatLights);
		SET_DWORD_STAT(STAT_SimFxSubMunitionCount, W.StatMunitions);
	};
	const uint64 FrameStart = FPlatformTime::Cycles64();
	Frame();
	LastFrameMs = static_cast<float>(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - FrameStart));

	if (Shots.IsValid())
	{
		Shots->Tick(*this, DeltaTime);
	}
}

// ------------------------------------------------------------------ the back end

void USimFxSubsystem::MakeBackEnd()
{
	UWorld* World = GetWorld();
	if (bBackEnd || World == nullptr || !Fx.IsValid())
	{
		return;
	}
	bBackEnd = true;

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (Host == nullptr)
	{
		UE_LOG(LogSimFxSubsystem, Warning, TEXT("The effects' host actor could not be spawned: no effects drawn."));
		return;
	}
	Host->SetActorTickEnabled(false);
	Host->SetCanBeDamaged(false);
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("FxRoot"), RF_Transient);
	Root->SetMobility(EComponentMobility::Movable);
	Host->SetRootComponent(Root);
	Root->RegisterComponent();

	UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	TArray<FString> Missing;
	auto Material = [&](const TCHAR* Name) -> UMaterialInterface*
	{
		UMaterialInterface* M = SimFxSubLoad<UMaterialInterface>(SimFxSubMaterialRoot, Name);
		if (M == nullptr)
		{
			Missing.AddUnique(Name);
			return Fallback;
		}
		return M;
	};
	auto Texture = [&](const TCHAR* Name) -> UTexture*
	{
		UTexture* T = SimFxSubLoad<UTexture>(SimFxSubTextureRoot, Name);
		if (T == nullptr)
		{
			Missing.AddUnique(Name);
		}
		return T;
	};

	// The sprite layers: one instanced plane each.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FVector FoundU, FoundV;
	if (SimFxSubPlaneAxes(Plane, FoundU, FoundV))
	{
		PlaneU = FoundU;
		PlaneV = FoundV;
	}
	UMaterialInterface* SmokeBase = Material(TEXT("M_FX_Smoke"));
	UMaterialInterface* AdditiveBase = Material(TEXT("M_FX_Additive"));
	struct FLayerLook
	{
		UMaterialInterface* Base;
		const TCHAR* TextureParam;
		const TCHAR* Texture;
		float Cols;
		float Rows;
	};
	const FLayerLook Looks[SimFxLayerCount] = {
		{SmokeBase, TEXT("Atlas"), TEXT("T_FX_SmokeAtlas"), 4.0f, 4.0f},
		{SmokeBase, TEXT("Atlas"), TEXT("T_FX_DebrisAtlas"), 4.0f, 4.0f},
		{SmokeBase, TEXT("Atlas"), TEXT("T_FX_SmokeAtlas"), 4.0f, 4.0f},  // the fire's sprites are drawn in the smoke's layer
		{AdditiveBase, TEXT("Tex"), TEXT("T_FX_Glow"), 1.0f, 1.0f},
		{AdditiveBase, TEXT("Tex"), TEXT("T_FX_Spark"), 1.0f, 1.0f},
		{AdditiveBase, TEXT("Tex"), TEXT("T_FX_Streak"), 1.0f, 1.0f},
		{AdditiveBase, TEXT("Tex"), TEXT("T_FX_Ring"), 1.0f, 1.0f},
		{AdditiveBase, TEXT("Tex"), TEXT("T_FX_Plume"), 1.0f, 1.0f},
	};
	Layers.SetNum(SimFxLayerCount);
	LayerMaterials.SetNum(SimFxLayerCount);
	LayerCapacity.Init(0, SimFxLayerCount);
	LayerShown.Init(0, SimFxLayerCount);
	LayerEmptyFrames.Init(0, SimFxLayerCount);
	for (int32 L = 0; L < SimFxLayerCount; ++L)
	{
		UInstancedStaticMeshComponent* Ism =
			NewObject<USimFxSpriteLayerComponent>(Host, *FString::Printf(TEXT("FxLayer%d"), L), RF_Transient);
		Ism->SetStaticMesh(Plane);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCanEverAffectNavigation(false);
		Ism->SetCastShadow(false);
		Ism->SetAffectDistanceFieldLighting(false);
		Ism->SetVisibleInRayTracing(false);
		Ism->SetReceivesDecals(false);
		Ism->InstanceStartCullDistance = 0;
		Ism->InstanceEndCullDistance = 0;
		Ism->NumCustomDataFloats = SimFxSubFloats;
		Ism->SetTranslucentSortPriority(L);
		Ism->SetupAttachment(Root);
		Ism->RegisterComponent();
		if (Ism->NumCustomDataFloats != SimFxSubFloats)
		{
			Ism->SetNumCustomDataFloats(SimFxSubFloats);
		}
		// Its first chunk of spares now, not on the frame of its first burst (that frame's spike: art-polish-2).
		XfScratch.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(SimFxSubHiddenScale)), SimFxSubInstanceChunk);
		Ism->AddInstances(XfScratch, false, false, false);
		LayerCapacity[L] = SimFxSubInstanceChunk;
		UMaterialInstanceDynamic* Mid = nullptr;
		if (Looks[L].Base != nullptr)
		{
			Mid = UMaterialInstanceDynamic::Create(Looks[L].Base, Host);
			if (Looks[L].Base != Fallback)
			{
				if (UTexture* Tex = Texture(Looks[L].Texture))
				{
					Mid->SetTextureParameterValue(Looks[L].TextureParam, Tex);
				}
				Mid->SetScalarParameterValue(TEXT("Cols"), Looks[L].Cols);
				Mid->SetScalarParameterValue(TEXT("Rows"), Looks[L].Rows);
			}
			Ism->SetMaterial(0, Mid);
		}
		Ism->SetVisibility(false);
		Layers[L] = Ism;
		LayerMaterials[L] = Mid;
	}

	// The colour flipbook (the fetched explosion): without it the fireballs keep the generated flipbook.
	bColourFlipbook = SimFxSubLoad<UTexture>(SimFxSubTextureRoot, TEXT("T_FXR_Unity_Explosion02HD_5x5")) != nullptr &&
		SimFxSubLoad<UTexture>(SimFxSubTextureRoot, TEXT("T_FXR_Unity_Explosion02HD_5x5_EmissionLog")) != nullptr;
	if (!bColourFlipbook)
	{
		UE_LOG(LogSimFxSubsystem, Log, TEXT("No colour explosion flipbook (run tools/ue_make_fx_assets.py): fireballs use the generated one."));
	}

	// The ribbons: one procedural mesh.
	RibbonMesh = NewObject<UProceduralMeshComponent>(Host, TEXT("FxRibbons"), RF_Transient);
	RibbonMesh->SetMobility(EComponentMobility::Movable);
	RibbonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RibbonMesh->SetCanEverAffectNavigation(false);
	RibbonMesh->SetCastShadow(false);
	RibbonMesh->SetAffectDistanceFieldLighting(false);
	RibbonMesh->SetVisibleInRayTracing(false);
	RibbonMesh->SetReceivesDecals(false);
	RibbonMesh->bUseAsyncCooking = true;
	RibbonMesh->SetTranslucentSortPriority(-1);
	RibbonMesh->SetupAttachment(Root);
	RibbonMesh->RegisterComponent();
	if (UMaterialInterface* RibbonBase = Material(TEXT("M_FX_Ribbon")))
	{
		RibbonMaterial = UMaterialInstanceDynamic::Create(RibbonBase, Host);
		RibbonMesh->SetMaterial(0, RibbonMaterial);
	}
	RibbonOrderCount = 0;

	// The pools.
	ScorchMaterial = Material(TEXT("M_FX_Scorch"));
	const FSimFxBudgets& B = Fx->Budgets;
	for (int32 I = 0; I < B.MaxLights; ++I)
	{
		LightPool.Add(SimFxSubMakeLight(Host, Root, I));
	}
	AddDecals(B.MaxDecals);
	for (int32 I = 0; I < B.MaxMunitions; ++I)
	{
		MunitionPool.Add(SimFxSubMakeMunition(Host, Root, I));
	}
	// The munitions' meshes now, with the scenario's loading: built on first use, a launch's or a release's first frame
	// took 3.5-4 ms of the game thread.
	FSimMunitionModels::BuildAll();

	if (Missing.Num() > 0 && !bMissingAssetsLogged)
	{
		bMissingAssetsLogged = true;
		UE_LOG(LogSimFxSubsystem, Warning, TEXT("Effects assets missing (run tools/ue_make_fx_assets.py): %s. The engine's basic material stands in."),
			*FString::Join(Missing, TEXT(", ")));
	}
	UpdateSun();
}

// ------------------------------------------------------------------ views and sun

void USimFxSubsystem::UpdateViews()
{
	FSimFxWorld& W = *Fx;
	W.Views.Reset();
	UWorld* World = GetWorld();
	APlayerCameraManager* Cam = World != nullptr ? UGameplayStatics::GetPlayerCameraManager(World, 0) : nullptr;
	if (Cam != nullptr)
	{
		double WidthPx = 1280.0;
		double Aspect = 0.5625;
		if (GEngine != nullptr && GEngine->GameViewport != nullptr)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			if (Size.X > 1.0 && Size.Y > 1.0)
			{
				WidthPx = Size.X;
				Aspect = Size.Y / Size.X;
			}
		}
		FSimFxView View;
		View.Set(Cam->GetCameraLocation(), Cam->GetCameraRotation(), Cam->GetFOVAngle(), WidthPx, Aspect);
		W.Views.Add(View);
		W.CamPos = View.Location;
	}
	for (auto It = W.Aux.CreateIterator(); It; ++It)
	{
		if (GFrameCounter - It.Value().Frame > 2)
		{
			It.RemoveCurrent();  // not set for a moment: it counts no more
			continue;
		}
		if (It.Value().View.bActive)
		{
			W.Views.Add(It.Value().View);
			if (Cam == nullptr && W.Views.Num() == 1)
			{
				W.CamPos = It.Value().View.Location;
			}
		}
	}
}

void USimFxSubsystem::UpdateSun()
{
	if (!Fx.IsValid())
	{
		return;
	}
	FSimFxWorld& W = *Fx;
	FSimFxSun& Sun = W.Sun;
	const FSimFxTuning::FLighting& T = W.Tune.Lighting;
	UWorld* World = GetWorld();

	// The world's sun: the directional light the sky uses, else the brightest.
	UDirectionalLightComponent* Best = nullptr;
	bool bBestAtmosphere = false;
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		UDirectionalLightComponent* L = *It;
		if (L == nullptr || L->GetWorld() != World || !L->IsRegistered() || !L->IsVisible() || !L->bAffectsWorld)
		{
			continue;
		}
		const bool bAtmosphere = L->IsUsedAsAtmosphereSunLight();
		if (Best == nullptr || (bAtmosphere && !bBestAtmosphere) || (bAtmosphere == bBestAtmosphere && L->Intensity > Best->Intensity))
		{
			Best = L;
			bBestAtmosphere = bAtmosphere;
		}
	}
	if (Best != nullptr)
	{
		Sun.ToSun = -Best->GetForwardVector();
		Sun.ElevationDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Sun.ToSun.Z, -1.0, 1.0)));
		Sun.Lux = FMath::Max(static_cast<double>(Best->Intensity), 0.0);
		FLinearColor C = Best->GetLightColor();
		if (Best->bUseTemperature)
		{
			C *= FLinearColor::MakeFromColorTemperature(Best->Temperature);
		}
		// Low, the light comes through more air: redder.
		const float Low = static_cast<float>(FMath::Clamp(Sun.ElevationDeg / 12.0, 0.0, 1.0));
		C.G *= FMath::Lerp(0.78f, 1.0f, Low);
		C.B *= FMath::Lerp(0.55f, 1.0f, Low);
		Sun.Colour = C;
		if (!Sun.bFound)
		{
			UE_LOG(LogSimFxSubsystem, Log, TEXT("Effects lit by %s: %.1f lux, %.1f deg up."), *Best->GetPathName(), Sun.Lux, Sun.ElevationDeg);
		}
		Sun.bFound = true;
	}
	else
	{
		if (!bSunLogged)
		{
			bSunLogged = true;
			UE_LOG(LogSimFxSubsystem, Log, TEXT("No directional light in the world: the effects keep their default sun."));
		}
		Sun.bFound = true;  // the defaults stand; looked for again in a second
	}

	const double E = Sun.ElevationDeg;
	// The sun's light on a white surface facing it (lux * colour / pi; the reference 10-lux sun: about the terrain's lit white).
	const double SunK = E > -2.0 ? Sun.Lux / UE_DOUBLE_PI * FMath::Clamp((E + 2.0) / 4.0, 0.0, 1.0) : 0.0;
	Sun.SunWhite = Sun.Colour * static_cast<float>(SunK);
	// The sky's light (N1): its share of the sun's by the elevation, as the level's sky light gives it: none when the
	// level has its sky light off (the terrain then gets none either), so the smoke never glows on a black world.
	const double AmbientK = SimFxLogic::AmbientShare(E, T.AmbientDay, T.AmbientDusk, T.AmbientNight);
	double SkyK = 1.0;
	FLinearColor SkyColour = FLinearColor::White;
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		const USkyLightComponent* Sky = *It;
		if (Sky == nullptr || Sky->GetWorld() != World || !Sky->IsRegistered())
		{
			continue;
		}
		SkyK = (Sky->IsVisible() && Sky->bAffectsWorld) ? FMath::Max(static_cast<double>(Sky->Intensity), 0.0) : 0.0;
		SkyColour = Sky->GetLightColor();
		break;
	}
	const double AmbientLux = FMath::Max(Sun.Lux, T.SunReferenceLux);
	Sun.Ambient = T.SkyTint * SkyColour * static_cast<float>(AmbientK * SkyK * AmbientLux / UE_DOUBLE_PI);
	const bool bWasNight = Sun.bNight;
	Sun.bNight = E < -6.0;
	if (Sun.bNight != bWasNight)
	{
		UE_LOG(LogSimFxSubsystem, Log, TEXT("Effects at %s: sun %.1f deg up, the sky's share %.5f, sky light %.2f, ambient %.5f."),
			Sun.bNight ? TEXT("night") : TEXT("day"), E, AmbientK, SkyK, Sun.Ambient.G);
	}

	// The sun on smoke (the second art review's item 1: sunlit smoke clipped to white with its shading lost).
	Sun.SmokeSunWhite = Sun.SunWhite * static_cast<float>(FMath::Max(T.SmokeSun, 0.0));
	Sun.SmokeAmbient = Sun.Ambient * static_cast<float>(FMath::Max(T.SmokeSky, 0.0));
	// The ribbon's vertex colours are the lit colour over its brightness.
	const FLinearColor Max = Sun.SmokeSunWhite + Sun.SmokeAmbient;
	RibbonScale = FMath::Max(FMath::Max3(Max.R, Max.G, Max.B), 0.01f);

	// The materials.
	for (int32 L = 0; L < LayerMaterials.Num(); ++L)
	{
		UMaterialInstanceDynamic* Mid = LayerMaterials[L];
		if (Mid == nullptr)
		{
			continue;
		}
		const ESimFxLayer Layer = static_cast<ESimFxLayer>(L);
		if (Layer == ESimFxLayer::Smoke || Layer == ESimFxLayer::Debris || Layer == ESimFxLayer::Fire)
		{
			// One material for smoke, debris and fire: the sprites' tints are albedos it lights; the fire's glow
			// (heat) is the material's, in the same brightness scale.
			Mid->SetVectorParameterValue(TEXT("SunColor"), Sun.SmokeSunWhite);
			Mid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(FVector3f(Sun.ToSun)));
			Mid->SetVectorParameterValue(TEXT("AmbientColor"), Sun.SmokeAmbient);
			Mid->SetScalarParameterValue(TEXT("SkyLow"), static_cast<float>(T.SmokeSkyLow));
			Mid->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
			Mid->SetScalarParameterValue(TEXT("EmissiveScale"), W.Brightness(W.Tune.Explosion.FireBrightness));
			Mid->SetScalarParameterValue(TEXT("ColourGlow"), static_cast<float>(W.Tune.Explosion.ColourFlipbookGlow));
		}
		else
		{
			// The additive sprites' tints carry their brightness (FSimFxWorld::Brightness, the cvar in it).
			Mid->SetScalarParameterValue(TEXT("Intensity"), 1.0f);
		}
	}
	if (RibbonMaterial != nullptr)
	{
		RibbonMaterial->SetScalarParameterValue(TEXT("Brightness"), RibbonScale);
	}
}

// ------------------------------------------------------------------ sprites

void USimFxSubsystem::DrawSprites()
{
	SCOPE_CYCLE_COUNTER(STAT_SimFxSubSprites);
	// The frame's lights are sorted brightest first (Tick): the first few light the smoke (T9).
	for (int32 L = 0; L < SimFxLayerCount; ++L)
	{
		UploadLayer(L);
	}
}

void USimFxSubsystem::UploadLayer(int32 Layer)
{
	UInstancedStaticMeshComponent* Ism = Layers.IsValidIndex(Layer) ? Layers[Layer].Get() : nullptr;
	if (Ism == nullptr || !Fx.IsValid())
	{
		return;
	}
	FSimFxWorld& W = *Fx;
	// What this component draws: its own layer's sprites and those of the layers drawn with it (the fire with the
	// smoke: one sorted, premultiplied draw, T5).
	const ESimFxLayer LayerE = static_cast<ESimFxLayer>(Layer);
	ItemScratch.Reset();
	for (int32 From = 0; From < SimFxLayerCount; ++From)
	{
		if (SimFxLogic::DrawLayerOf(static_cast<ESimFxLayer>(From)) == LayerE)
		{
			for (const FSimFxSprite& S : W.Sprites[From])
			{
				ItemScratch.Add(&S);
			}
		}
	}
	const int32 N = ItemScratch.Num();
	const int32 Shown = LayerShown[Layer];
	LayerEmptyFrames[Layer] = N > 0 ? 0 : LayerEmptyFrames[Layer] + 1;
	if (N == 0 && Shown == 0)
	{
		// Hidden only once empty for a while: a layer shown and hidden burst by burst rebuilt its render state each time
		// (0.8 ms frames for 16 sprites: art-polish-2); its spares meanwhile sit at the hidden scale.
		if (Ism->IsVisible() && LayerEmptyFrames[Layer] > SimFxSubHideAfterFrames)
		{
			Ism->SetVisibility(false);
		}
		return;
	}

	// The camera and the hidden spares' place (near what is drawn, so the bounds stay tight).
	const FVector Cam = W.CamPos;
	const FSimFxView* Main = W.Views.Num() > 0 ? &W.Views[0] : nullptr;
	const FVector ViewRight = Main != nullptr ? Main->Right : FVector::RightVector;
	const FVector ViewUp = Main != nullptr ? Main->Up : FVector::UpVector;
	const FVector ViewFwd = Main != nullptr ? Main->Forward : FVector::ForwardVector;
	const FVector HidePos = N > 0 ? ItemScratch[0]->Pos : Cam;
	const FTransform Hidden(FQuat::Identity, HidePos, FVector(SimFxSubHiddenScale));

	// Grow: always one spare past the frame's sprites (the custom data's batch call may write one past its end). At least
	// doubling, so a burst growing to 500 sprites adds instances 3 times, not 8 (each add a spike frame: art-polish-2).
	if (N + 1 > LayerCapacity[Layer])
	{
		const int32 NewCap = FMath::Max(Align(N + 1, SimFxSubInstanceChunk), 2 * LayerCapacity[Layer]);
		XfScratch.Reset();
		XfScratch.Init(Hidden, NewCap - LayerCapacity[Layer]);
		Ism->AddInstances(XfScratch, false, false, false);
		LayerCapacity[Layer] = NewCap;
	}

	// Back to front for the translucent layers (an ISM draws its instances in order).
	const bool bSorted = LayerE == ESimFxLayer::Smoke || LayerE == ESimFxLayer::Debris;
	const bool bLitLayer = LayerE == ESimFxLayer::Smoke || LayerE == ESimFxLayer::Debris;
	OrderScratch.SetNumUninitialized(N, EAllowShrinking::No);
	for (int32 I = 0; I < N; ++I)
	{
		OrderScratch[I] = I;
	}
	if (bSorted && N > 1)
	{
		DistScratch.SetNumUninitialized(N, EAllowShrinking::No);
		for (int32 I = 0; I < N; ++I)
		{
			DistScratch[I] = static_cast<float>(FVector::DistSquared(ItemScratch[I]->Pos, Cam));
		}
		const TArray<float>& Dist = DistScratch;
		OrderScratch.Sort([&Dist](const int32 A, const int32 B) { return Dist[A] > Dist[B]; });
	}

	const int32 Count = FMath::Max(N, Shown);
	XfScratch.SetNumUninitialized(Count, EAllowShrinking::No);
	DataScratch.SetNumUninitialized((N + 1) * SimFxSubFloats, EAllowShrinking::No);
	const double Hand = FVector::DotProduct(FVector::CrossProduct(PlaneU, PlaneV), FVector::UpVector) >= 0.0 ? 1.0 : -1.0;
	const FVector AbsU = PlaneU.GetAbs();
	const FVector AbsV = PlaneV.GetAbs();
	const FVector ToSun = W.Sun.ToSun;
	const double Backlit = W.Tune.Lighting.Backlit;
	const double LightScale = W.Budgets.EmissiveScale;
	// The flashes and fires that light the smoke this frame (T9): the frame's brightest lights (DrawSprites sorted them).
	const int32 LitBy = bLitLayer ? FMath::Min(W.Lights.Num(), SimFxSubSmokeLights) : 0;
	const FLinearColor SunMax = W.Sun.SunWhite;
	// By day held under a share of the sun's white; at night under the night's cap (the second art review's item 4).
	const double LocalCap = W.Sun.bNight ? W.Tune.Explosion.NightLightCap : W.Tune.Explosion.DayLightCap * FMath::Max3(SunMax.R, SunMax.G, SunMax.B);
	// The drawn sprites' box in the instances' space (the transforms below are the component's own), for its bounds.
	FBox DrawnBox(ForceInit);

	for (int32 Slot = 0; Slot < N; ++Slot)
	{
		const FSimFxSprite& S = *ItemScratch[OrderScratch[Slot]];
		FVector ToCam = Cam - S.Pos;
		const double Dist = ToCam.Size();
		ToCam = Dist > 1.0 ? ToCam / Dist : -ViewFwd;

		double Len = S.Length;
		double Wid = S.Width;
		float Opacity = S.Opacity;
		if (S.MinPx > 0.0f && Main != nullptr)
		{
			const double Ppc = Main->PixelsPerCm(Dist);
			if (Ppc > 0.0)
			{
				const double MinCm = S.MinPx / Ppc;
				Len = FMath::Max(Len, MinCm);
				Wid = FMath::Max(Wid, MinCm);
				// Widened past its true size: dimmed by its true area over the drawn (D8), a night glint never under its keep.
				Opacity *= SimFxLogic::GlintAlpha(S.Width, S.Length, Wid, Len, S.KeepAlpha);
			}
		}

		FVector Uw, Nw;
		switch (S.Facing)
		{
		case ESimFxFacing::Axis:
		{
			Uw = S.Axis.GetSafeNormal();
			if (Uw.IsZero())
			{
				Uw = ViewRight;
			}
			Nw = ToCam - Uw * FVector::DotProduct(ToCam, Uw);
			if (!Nw.Normalize())
			{
				Nw = FVector::CrossProduct(Uw, FMath::Abs(Uw.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			}
			break;
		}
		case ESimFxFacing::Flat:
		{
			Nw = FVector::UpVector;
			Uw = FVector(FMath::Cos(S.Roll), FMath::Sin(S.Roll), 0.0);
			break;
		}
		case ESimFxFacing::Camera:
		default:
		{
			Nw = ToCam;
			const double C = FMath::Cos(S.Roll), Sn = FMath::Sin(S.Roll);
			const FVector R0 = ViewRight * C + ViewUp * Sn;
			Uw = R0 - Nw * FVector::DotProduct(R0, Nw);
			if (!Uw.Normalize())
			{
				Uw = FVector::CrossProduct(FMath::Abs(Nw.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector, Nw).GetSafeNormal();
			}
			break;
		}
		}
		const FVector Vw = FVector::CrossProduct(Nw, Uw) * Hand;
		// The rotation takes the plane's U, V and normal to the sprite's: its rows are the images of the plane's X, Y, Z.
		const FVector AX = Uw * PlaneU.X + Vw * PlaneV.X;
		const FVector AY = Uw * PlaneU.Y + Vw * PlaneV.Y;
		const FMatrix M(AX, AY, Nw, FVector::ZeroVector);
		const FVector Scale((AbsU.X * Len + AbsV.X * Wid) / 100.0, (AbsU.Y * Len + AbsV.Y * Wid) / 100.0, 1.0);
		XfScratch[Slot] = FTransform(FQuat(M), S.Pos, Scale);
		// The quad's half diagonal holds it in any facing.
		const double HalfDiag = 0.5 * FMath::Sqrt(Len * Len + Wid * Wid);
		DrawnBox += FBox(S.Pos - FVector(HalfDiag), S.Pos + FVector(HalfDiag));

		// Its picture: the colour flipbook's frames on the generated flipbook when the colour one is missing.
		ESimFxKind Kind = S.Kind;
		float Frame = S.Frame;
		if (Kind == ESimFxKind::ColourFlipbook && !bColourFlipbook)
		{
			Kind = ESimFxKind::HeatFlipbook;
			Frame = Frame * (63.0f / 24.0f);
		}
		// The flashes' and fires' light on it (T9), its own size keeping it finite inside the puff.
		FLinearColor Local(0.0f, 0.0f, 0.0f, 0.0f);
		for (int32 L = 0; L < LitBy; ++L)
		{
			const FSimFxLightDraw& Ld = W.Lights[L];
			Local += SimFxLogic::LightOnSurface(Ld.Colour, Ld.Lumens * LightScale, FVector::Dist(Ld.Pos, S.Pos), Ld.RadiusCm, 0.5 * Wid);
		}
		// By day a burst's light must not turn its own dust white (the art review's section 2): held under a share of
		// the sun's white.
		Local = SimFxLogic::CapLight(Local, LocalCap);
		// A ring lying on the ground fades out at grazing angles (the art review's section 2: a hard white line).
		if (LayerE == ESimFxLayer::Ring && S.Facing == ESimFxFacing::Flat)
		{
			Opacity *= SimFxLogic::RingAlpha(ToCam.Z);
		}
		// Seen from inside it, a puff clears round the camera instead of filling the screen (LOOK 9.3, T2: the player
		// stands in his launch's smoke); far from the camera it changes nothing. Flashes and flames (additive) stay.
		if (LayerE == ESimFxLayer::Smoke || LayerE == ESimFxLayer::Debris)
		{
			Opacity *= SimFxLogic::CameraFade(Dist, FMath::Max(Len, Wid));
		}

		float* D = DataScratch.GetData() + Slot * SimFxSubFloats;
		D[0] = Frame;
		D[1] = Opacity;
		D[2] = S.Heat;
		D[3] = S.Tint.R;
		D[4] = S.Tint.G;
		D[5] = S.Tint.B;
		D[6] = S.Erosion;
		D[7] = bLitLayer ? SimFxSubLit(ToSun, ToCam, Backlit) : 1.0f;
		D[8] = static_cast<float>(static_cast<uint8>(Kind));
		D[9] = Local.R;
		D[10] = Local.G;
		D[11] = Local.B;
		D[12] = FMath::Max(SimFxSubFadeShare * static_cast<float>(Wid), 10.0f);
	}
	for (int32 Slot = N; Slot < Count; ++Slot)
	{
		XfScratch[Slot] = Hidden;
	}
	FMemory::Memzero(DataScratch.GetData() + N * SimFxSubFloats, SimFxSubFloats * sizeof(float));

	Ism->BatchUpdateInstancesTransforms(0, XfScratch, false, false, false);
	if (N > 0)
	{
		const TConstArrayView<float> Data(DataScratch.GetData(), N * SimFxSubFloats);
		// [0, N - 1]: the batch call's end is inclusive (UE 5.8 ensures (End - Start + 1) * floats == the data's length;
		// an exclusive end failed that ensure, a 0.7 s stack walk, on the first burst of a run).
		bool bOk = CustomDataMode != 3 && Ism->SetCustomData(0, N - 1, Data, false);
		if (CustomDataMode == 0)
		{
			CustomDataMode = bOk ? 2 : 3;
		}
		if (!bOk)
		{
			for (int32 Slot = 0; Slot < N; ++Slot)
			{
				Ism->SetCustomData(Slot, TArrayView<const float>(DataScratch.GetData() + Slot * SimFxSubFloats, SimFxSubFloats), false);
			}
		}
	}
	// The layer's bounds from what it draws, once a frame (the instance data itself goes up with the component's instance
	// update): tight, and no pass over every instance the layer holds.
	if (USimFxSpriteLayerComponent* SpriteLayer = Cast<USimFxSpriteLayerComponent>(Ism))
	{
		SpriteLayer->SetFxBounds(DrawnBox);  // the instances' own space, as their transforms above
	}
	Ism->UpdateBounds();
	Ism->MarkRenderTransformDirty();
	if (N > 0 && !Ism->IsVisible())
	{
		Ism->SetVisibility(true);
	}
	LayerShown[Layer] = N;
}

// ------------------------------------------------------------------ ribbons

void USimFxSubsystem::DrawRibbons()
{
	SCOPE_CYCLE_COUNTER(STAT_SimFxSubRibbons);
	FSimFxWorld& W = *Fx;
	W.StatRibbons = 0;
	W.StatRibbonPoints = 0;
	if (RibbonMesh == nullptr)
	{
		return;
	}
	const FVector Cam = W.CamPos;
	const float RT = W.RibbonTime();
	const double FarCm = W.Tune.Lod.FarRibbonM * 100.0;
	const FVector ToSun = W.Sun.ToSun;
	const double Backlit = W.Tune.Lighting.Backlit;
	const double SkyLow = W.Tune.Lighting.SmokeSkyLow;
	const float InvScale = 1.0f / FMath::Max(RibbonScale, 0.01f);
	const int32 Cap = FMath::Max(W.Budgets.MaxRibbons, 0);
	const FSimFxView* Main = W.Views.Num() > 0 ? &W.Views[0] : nullptr;
	// The flashes and fires that light the smoke this frame light the trails too (T9; DrawSprites sorted them).
	const int32 LitBy = FMath::Min(W.Lights.Num(), SimFxSubSmokeLights);
	const double LightScale = W.Budgets.EmissiveScale;
	const FLinearColor SunMax = W.Sun.SunWhite;
	// By day held under a share of the sun's white; at night under the night's cap (the second art review's item 4).
	const double LocalCap = W.Sun.bNight ? W.Tune.Explosion.NightLightCap : W.Tune.Explosion.DayLightCap * FMath::Max3(SunMax.R, SunMax.G, SunMax.B);

	RibbonVerts.Reset();
	RibbonUvs.Reset();
	RibbonUvs1.Reset();
	RibbonUvs2.Reset();
	RibbonUvs3.Reset();
	RibbonColours.Reset();
	int32 Drawn = 0;
	for (FSimFxRibbon& R : W.Ribbons)
	{
		if (!R.bUsed || R.Points.Num() < 2)
		{
			continue;
		}
		if (Drawn >= Cap)
		{
			break;
		}
		const int32 NP = R.Points.Num();
		const FSimFxRibbonStyle& St = R.Style;
		// Far by its nearest point, not its head: a trail whose missile is long gone downrange may still pass by the camera
		// (our own launch seen from the vehicle), and reused there its strip faced an old camera and its near puffs
		// (AddTrailPuffs, only on a rebuild) were drawn one frame in four.
		double NearestSq = TNumericLimits<double>::Max();
		for (const FSimFxRibbonPoint& P : R.Points)
		{
			NearestSq = FMath::Min(NearestSq, FVector::DistSquared(P.Pos, Cam));
		}
		// Wholly past the trails' range or the weather's visibility: not moved, meshed or drawn (docs/fx/README.md 4.1).
		const double RangeCm = 100.0 * W.TrailRangeM((R.Owner & SimFxContrailKeyBit) != 0);
		if (NearestSq > RangeCm * RangeCm)
		{
			R.bVisible = false;
			R.BuiltFrame = -100;
			continue;
		}
		const bool bFar = NearestSq > FarCm * FarCm;
		const bool bReuse = bFar && W.Frame - R.BuiltFrame < 4 && R.Verts.Num() == 2 * NP && R.Uvs3.Num() == 2 * NP;
		if (!bReuse)
		{
			// Where each point is now: carried by the wind at its height and its eddies, a contrail's plume rolled into
			// its vortex and sinking with it, pinched where the Crow rings form (LOOK 9.1, 12).
			RibbonAtScratch.SetNumUninitialized(NP, EAllowShrinking::No);
			RibbonPuffScratch.SetNumZeroed(NP, EAllowShrinking::No);
			R.bVisible = false;
			for (int32 J = NP - 1; J >= 0; --J)
			{
				const FSimFxRibbonPoint& P = R.Points[J];
				const float Age = FMath::Max(RT - P.BornT, 0.0f);
				const float Crow = SimFxLogic::CrowAt(P.DistM, St.CrowPeriodM, Age, St.CrowFromS, St.CrowFullS);
				RibbonAtScratch[J] = SimFxLogic::RibbonPointAt(P.Pos, R.Drift + P.Drift, P.Side * (1.0f - Crow), St.RollS, St.SinkCmps, St.SinkMaxCm, Age);
				if (!R.bVisible && W.Seen(RibbonAtScratch[J], (P.Width0 + R.SpreadCmps * P.SpreadK * Age) * 2.0f))
				{
					R.bVisible = true;
				}
			}
			R.BuiltFrame = W.Frame;
			R.Verts.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			R.Colours.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			R.Uvs.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			R.Uvs1.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			R.Uvs2.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			R.Uvs3.SetNumUninitialized(2 * NP, EAllowShrinking::No);
			if (R.bVisible)
			{
				const float TileM = FMath::Max(St.TileM, 1.0f);
				FVector PrevSide = FVector::ZeroVector;
				for (int32 J = 0; J < NP; ++J)
				{
					const FSimFxRibbonPoint& P = R.Points[J];
					const float Age = FMath::Max(RT - P.BornT, 0.0f);
					const FVector At = RibbonAtScratch[J];
					const float Crow = SimFxLogic::CrowAt(P.DistM, St.CrowPeriodM, Age, St.CrowFromS, St.CrowFullS);
					const float CrowGrown = SimFxLogic::CrowAt(0.0f, St.CrowPeriodM, Age, St.CrowFromS, St.CrowFullS);
					// Never under the pixel floor on the camera, its alpha then by its true width (T10, D8): a far line
					// stays a line instead of breaking into dashes. Each point widens at its own rate (LOOK 9.3: puffs).
					float WidthAlpha = 1.0f;
					const float TrueWidth = (P.Width0 + R.SpreadCmps * P.SpreadK * Age) * (1.0f + 0.8f * Crow);
					const float Width = Main != nullptr
						? SimFxLogic::RibbonWidthFloor(TrueWidth, Main->PixelsPerCm(FVector::Dist(At, Cam)), SimFxSubRibbonMinPx, WidthAlpha)
						: TrueWidth;
					// Thinning with age (LOOK 9.3: 0.8 to 0.5 by 15 s) and fading at the end of its life; thin between the
					// Crow rings.
					float Opacity = P.Opacity0 * SimFxLogic::TrailOpacityAt(Age, R.LifeS, St.ThinS, St.ThinShare) * WidthAlpha *
						(1.0f - 0.6f * (CrowGrown - Crow));
					if (J == 0 && NP > 2)
					{
						Opacity = 0.0f;  // the old end tapers out
					}
					const FVector Prev = RibbonAtScratch[FMath::Max(J - 1, 0)];
					const FVector Next = RibbonAtScratch[FMath::Min(J + 1, NP - 1)];
					const FVector Tangent = Next - Prev;
					const FVector ViewLine = At - Cam;
					FVector Side = FVector::CrossProduct(Tangent, ViewLine);
					// How face-on the strip is to the camera: for a strip turned to the camera, the sine of the angle between
					// the trail and the line of sight.
					const double SinView = Side.Size() / FMath::Max(Tangent.Size() * ViewLine.Size(), 1e-6);
					if (!Side.Normalize())
					{
						Side = PrevSide.IsZero() ? FVector::CrossProduct(ViewLine, FVector::UpVector).GetSafeNormal() : PrevSide;
						if (Side.IsZero())
						{
							Side = FVector::RightVector;
						}
					}
					// Seen nearly end-on the side turns fast and may flip: kept turning the same way as the point before, so
					// the strip never folds into sheets. Each point still faces the camera (held fixed, a trail going away
					// from the camera lay flat like a road and drew a wedge).
					if (!PrevSide.IsZero() && FVector::DotProduct(Side, PrevSide) < 0.0)
					{
						Side = -Side;
					}
					PrevSide = Side;
					const FVector SideN = Side;
					Side *= 0.5 * Width;
					// Seen along its length a strip is a card edge-on (a sheet or a wedge from the launcher): faded there,
					// and near the camera drawn as puffs instead (T10), which then take the opacity it had.
					const float PuffShare = R.Style.bPuffsNear ? SimFxLogic::TrailPuffShare(SinView, ViewLine.Size()) : 0.0f;
					RibbonPuffScratch[J] = FVector4f(TrueWidth, Opacity * PuffShare, static_cast<float>(SinView), Age);
					Opacity *= SimFxLogic::EndOnAlpha(SinView) * (1.0f - PuffShare);
					R.Verts[2 * J] = At - Side;
					R.Verts[2 * J + 1] = At + Side;
					// Lit per point: the sun on the side we see of it here (a trail turning across the sky changes from
					// lit to backlit along its length), the sky, then the frame's flashes and fires near it (T9).
					const float Dist = static_cast<float>(ViewLine.Size());
					const FVector ToCam = Dist > 1.0f ? -ViewLine / Dist : FVector::UpVector;
					// Each edge lit as the side of a tube there (the second art review's item 1: lit as one flat strip, a trail
					// was white with no shaded side): the edge towards the sun lit, the other in shade.
					const float Lit = SimFxSubLit(ToSun, ToCam, Backlit);
					for (int32 Edge = 0; Edge < 2; ++Edge)
					{
						const FVector Out = SideN * (Edge == 0 ? -1.0 : 1.0);
						const float Share = SimFxLogic::SunShare(Out, ToCam, ToSun, Lit);
						const float Sky = SimFxLogic::SkyShare(SimFxLogic::PuffNormal(Out, ToCam).Z, SkyLow);
						const FLinearColor Light = W.Sun.SmokeSunWhite * Share + W.Sun.SmokeAmbient * Sky;
						FLinearColor C = R.Colour * Light * InvScale;
						C.R = FMath::Min(C.R, 1.0f);
						C.G = FMath::Min(C.G, 1.0f);
						C.B = FMath::Min(C.B, 1.0f);
						C.A = FMath::Clamp(Opacity, 0.0f, 1.0f);
						R.Colours[2 * J + Edge] = C;
					}
					FLinearColor Local(0.0f, 0.0f, 0.0f, 0.0f);
					for (int32 L = 0; L < LitBy; ++L)
					{
						const FSimFxLightDraw& Ld = W.Lights[L];
						const double D2 = FVector::DistSquared(Ld.Pos, At);
						if (D2 < Ld.RadiusCm * Ld.RadiusCm)
						{
							Local += SimFxLogic::LightOnSurface(Ld.Colour, Ld.Lumens * LightScale, FMath::Sqrt(D2), Ld.RadiusCm, 0.5 * Width);
						}
					}
					Local = SimFxLogic::CapLight(Local, LocalCap) * R.Colour;
					R.Uvs2[2 * J] = R.Uvs2[2 * J + 1] = FVector2D(Local.R, Local.G);
					// Its true width in metres for the material's camera fade (LOOK 9.3: the player stands in his launch's
					// smoke, which clears round him instead of filling the screen).
					R.Uvs3[2 * J] = R.Uvs3[2 * J + 1] = FVector2D(Local.B, TrueWidth / 100.0f);
					// The texture by distance along the trail, so its billows stay in the air (T10), each trail its own
					// stretch; the erosion by age (LOOK 9.3: the trail breaks into puffs); the far hairline unbroken (C3).
					const float U = P.DistM / TileM + R.UOffset;
					R.Uvs[2 * J] = FVector2D(U, 0.0);
					R.Uvs[2 * J + 1] = FVector2D(U, 1.0);
					const FVector2D E(SimFxLogic::TrailErosionAt(Age, R.LifeS, St.ErodeFrom) * WidthAlpha, 1.0f - WidthAlpha);
					R.Uvs1[2 * J] = R.Uvs1[2 * J + 1] = E;
				}
				AddTrailPuffs(R);
			}
		}
		if (!R.bVisible)
		{
			continue;
		}
		++Drawn;
		W.StatRibbonPoints += NP;
		// Its segments as quads (four vertices each: every quad of the mesh the same shape, so the index buffer is fixed).
		// Each quad's U starts in [0, 1): the texture tiles, and the mesh's half-precision UVs keep their precision.
		for (int32 J = 0; J + 1 < NP; ++J)
		{
			const double Base = FMath::FloorToDouble(R.Uvs[2 * J].X);
			for (int32 K = 2 * J; K < 2 * J + 4; ++K)
			{
				RibbonVerts.Add(R.Verts[K]);
				RibbonColours.Add(R.Colours[K]);
				RibbonUvs.Add(FVector2D(R.Uvs[K].X - Base, R.Uvs[K].Y));
				RibbonUvs1.Add(R.Uvs1[K]);
				RibbonUvs2.Add(R.Uvs2[K]);
				RibbonUvs3.Add(R.Uvs3[K]);
			}
		}
	}
	W.StatRibbons = Drawn;

	const int32 Quads = RibbonVerts.Num() / 4;
	if (Quads == 0)
	{
		if (RibbonOrderCount > 0)
		{
			RibbonMesh->ClearAllMeshSections();
			RibbonOrderCount = 0;
		}
		return;
	}
	// The section holds a whole number of chunks of quads; the unused ones collapse to a point, unseen.
	// Not pre-sized or doubled: every quad of the section, used or not, is uploaded every frame (UpdateMeshSection), and
	// pre-sized to 1024 quads the trails took about 0.45 ms every frame, the effects' mean 0.40 ms against 0.21 (art-polish-2).
	// For the same reason it shrinks again once a busy moment has passed (SimFxLogic::RibbonSectionQuads).
	bool bRebuild = false;
	const int32 QuadCap = SimFxLogic::RibbonSectionQuads(Quads, RibbonOrderCount, SimFxSubQuadChunk, SimFxSubRibbonShrinkFrames, RibbonLowFrames, bRebuild);
	const FVector Collapse = RibbonVerts[0];
	for (int32 Q = Quads; Q < QuadCap; ++Q)
	{
		for (int32 K = 0; K < 4; ++K)
		{
			RibbonVerts.Add(Collapse);
			RibbonColours.Add(FLinearColor::Transparent);
			RibbonUvs.Add(FVector2D::ZeroVector);
			RibbonUvs1.Add(FVector2D::ZeroVector);
			RibbonUvs2.Add(FVector2D::ZeroVector);
			RibbonUvs3.Add(FVector2D::ZeroVector);
		}
	}
	const TArray<FProcMeshTangent> NoTangents;
	RibbonNormals.Reset();
	if (bRebuild || RibbonMesh->GetNumSections() == 0)
	{
		RibbonTris.Reset(QuadCap * 6);
		for (int32 Q = 0; Q < QuadCap; ++Q)
		{
			const int32 B = Q * 4;
			RibbonTris.Add(B);
			RibbonTris.Add(B + 2);
			RibbonTris.Add(B + 1);
			RibbonTris.Add(B + 1);
			RibbonTris.Add(B + 2);
			RibbonTris.Add(B + 3);
		}
		RibbonMesh->CreateMeshSection_LinearColor(0, RibbonVerts, RibbonTris, RibbonNormals, RibbonUvs, RibbonUvs1, RibbonUvs2, RibbonUvs3, RibbonColours,
			NoTangents, false, false);
		RibbonOrderCount = QuadCap;
	}
	else
	{
		RibbonMesh->UpdateMeshSection_LinearColor(0, RibbonVerts, RibbonNormals, RibbonUvs, RibbonUvs1, RibbonUvs2, RibbonUvs3, RibbonColours, NoTangents, false);
	}
}

void USimFxSubsystem::AddTrailPuffs(const FSimFxRibbon& R)
{
	FSimFxWorld& W = *Fx;
	const int32 NP = FMath::Min(R.Points.Num(), RibbonPuffScratch.Num());
	int32 Budget = 256;  // puffs a trail adds at most
	for (int32 J = 1; J + 1 < NP && Budget > 0; ++J)
	{
		const FVector4f& A = RibbonPuffScratch[J];
		const FVector4f& B = RibbonPuffScratch[J + 1];
		if (A.Y <= 0.002f && B.Y <= 0.002f)
		{
			continue;
		}
		const FVector PA = RibbonAtScratch[J];
		const FVector PB = RibbonAtScratch[J + 1];
		const double SegCm = FVector::Dist(PA, PB);
		const double WidthCm = FMath::Max(0.5 * (A.X + B.X), 50.0);
		// Close enough to overlap into one body of smoke; each puff the thinner the more of them the eye looks through.
		// The gap as seen (along the line of sight the puffs stand behind one another).
		const float SinView = 0.5f * (A.Z + B.Z);
		// And along the line of sight no more than a sixth of the distance apart: nearly end-on each puff is drawn
		// round the ones behind it, and spaced wider they stacked into rings (a bullseye up the player's own climb).
		const double DepthCm = FMath::Max(FMath::Min(FVector::Dist(PA, W.CamPos), FVector::Dist(PB, W.CamPos)), 1000.0);
		const int32 Steps = FMath::Clamp(FMath::Max(FMath::CeilToInt32(SegCm * FMath::Max(SinView, 0.1f) / (0.5 * WidthCm)),
			FMath::CeilToInt32(SegCm / (DepthCm / 6.0))), 1, 8);
		// Seen end-on the puffs stand one behind another all the way up the climb: each the thinner (0.06 straight along it,
		// 0.15 from a sine of 0.2) and scattered wider off the line, so the stack reads as an uneven, see-through cloud
		// round the missile's line, not a round white disc (the coordinator's look of 1 Nov: our launch from the vehicle).
		const float EndOn = 1.0f - FMath::Clamp(SinView / 0.2f, 0.0f, 1.0f);
		const float Overlap = FMath::Clamp(static_cast<float>(SegCm / Steps * SinView / WidthCm), FMath::Lerp(0.15f, 0.06f, EndOn), 1.0f);
		const double Scatter = 0.5 + 0.9 * EndOn;
		// Across the trail, for each puff's place off its line.
		const FVector Along = SegCm > 1.0 ? (PB - PA) / SegCm : FVector::UpVector;
		FVector Across1 = FVector::CrossProduct(Along, FVector::UpVector);
		if (!Across1.Normalize())
		{
			Across1 = FVector::RightVector;
		}
		const FVector Across2 = FVector::CrossProduct(Along, Across1);
		for (int32 K = 0; K < Steps && Budget > 0; ++K, --Budget)
		{
			const float F = static_cast<float>(K) / static_cast<float>(Steps);
			// Each puff's picture, turn, size and place from where it is along the trail, so it keeps them frame to frame;
			// a little off the line and each its own size, so seen along the trail they do not stack into rings.
			// Keyed by the point's distance along the trail, which it keeps for life (its index shifts each time the
			// oldest point expires, and every puff then jumped to a new picture, size and place).
			const uint32 PointKey = static_cast<uint32>(FMath::RoundToInt32(R.Points[J].DistM * 8.0f));
			const uint32 Seed = HashCombine(GetTypeHash(R.Owner), HashCombine(GetTypeHash(PointKey), GetTypeHash(K)));
			const double H1 = static_cast<double>((Seed >> 8) & 255u) / 255.0 - 0.5;
			const double H2 = static_cast<double>((Seed >> 16) & 255u) / 255.0 - 0.5;
			const double H3 = static_cast<double>((Seed >> 24) & 255u) / 255.0;
			const double Here = FMath::Lerp(static_cast<double>(A.X), static_cast<double>(B.X), static_cast<double>(F));
			FSimFxSprite S;
			S.Pos = FMath::Lerp(PA, PB, static_cast<double>(F)) + (Across1 * H1 + Across2 * H2) * (Scatter * Here);
			S.Width = S.Length = static_cast<float>((1.0 + 0.6 * H3) * Here);
			S.Roll = static_cast<float>(Seed % 628u) * 0.01f;
			S.Frame = static_cast<float>(Seed % 16u);
			S.Opacity = FMath::Lerp(A.Y, B.Y, F) * Overlap;
			S.Tint = R.Colour;
			const float Age = FMath::Lerp(A.W, B.W, F);
			S.Erosion = 0.6f * SimFxLogic::TrailErosionAt(Age, R.LifeS, R.Style.ErodeFrom);
			S.Layer = ESimFxLayer::Smoke;
			S.Kind = ESimFxKind::Atlas;
			S.Facing = ESimFxFacing::Camera;
			W.AddSprite(S);
		}
	}
}

// ------------------------------------------------------------------ lights, decals, munitions

void USimFxSubsystem::DrawLights()
{
	FSimFxWorld& W = *Fx;
	if (Host != nullptr && Host->GetRootComponent() != nullptr)
	{
		while (LightPool.Num() < W.Budgets.MaxLights)
		{
			LightPool.Add(SimFxSubMakeLight(Host, Host->GetRootComponent(), LightPool.Num()));
		}
	}
	// The lights come sorted brightest first (Tick sorted them before the sprites and the trails took the first few).
	const int32 Use = FMath::Min3(W.Lights.Num(), LightPool.Num(), W.Budgets.MaxLights);
	const float Scale = static_cast<float>(W.Budgets.EmissiveScale);
	for (int32 I = 0; I < LightPool.Num(); ++I)
	{
		UPointLightComponent* L = LightPool[I];
		if (L == nullptr)
		{
			continue;
		}
		if (I >= Use)
		{
			if (L->IsVisible())
			{
				L->SetVisibility(false);
			}
			continue;
		}
		const FSimFxLightDraw& D = W.Lights[I];
		L->SetWorldLocation(D.Pos);
		L->SetIntensity(static_cast<float>(D.Lumens) * Scale);
		L->SetLightColor(D.Colour);
		const float Radius = static_cast<float>(D.RadiusCm);
		if (FMath::Abs(L->AttenuationRadius - Radius) > 0.05f * FMath::Max(Radius, 1.0f))
		{
			L->SetAttenuationRadius(Radius);
		}
		if (!L->IsVisible())
		{
			L->SetVisibility(true);
		}
	}
	W.StatLights = Use;
}

void USimFxSubsystem::AddDecals(int32 Count)
{
	USceneComponent* Root = Host != nullptr ? Host->GetRootComponent() : nullptr;
	if (Root == nullptr)
	{
		return;
	}
	while (DecalPool.Num() < Count)
	{
		UDecalComponent* D = NewObject<UDecalComponent>(Host, *FString::Printf(TEXT("FxScorch%d"), DecalPool.Num()), RF_Transient);
		UMaterialInstanceDynamic* Mid = ScorchMaterial != nullptr ? UMaterialInstanceDynamic::Create(ScorchMaterial, Host) : nullptr;
		D->SetDecalMaterial(Mid);
		D->SetMobility(EComponentMobility::Movable);
		D->SetVisibility(false);
		D->SetupAttachment(Root);
		D->RegisterComponent();
		DecalPool.Add(D);
		DecalMaterials.Add(Mid);
		DecalBornT.Add(-1.0);
		DecalOpacity.Add(1.0f);
	}
}

void USimFxSubsystem::DrawDecals()
{
	FSimFxWorld& W = *Fx;
	// sam3d.fx.MaxDecals raised at run time: the pool grows as the lights' and the munitions' do (lowered, the rest wait).
	AddDecals(W.Budgets.MaxDecals);
	const int32 Pool =FMath::Min(DecalPool.Num(), FMath::Max(W.Budgets.MaxDecals, 0));
	if (Pool > 0)
	{
		for (const FSimFxDecalWant& Want : W.NewDecals)
		{
			NextDecal = NextDecal % Pool;
			const int32 I = NextDecal++;
			UDecalComponent* D = DecalPool[I];
			if (D == nullptr)
			{
				continue;
			}
			const double R = FMath::Max(Want.RadiusCm, 10.0);
			D->DecalSize = FVector(FMath::Max(R, 300.0), R, R);
			D->SetWorldLocationAndRotation(Want.Pos, FRotator(-90.0, Want.Yaw, 0.0));
			D->MarkRenderStateDirty();
			if (DecalMaterials[I] != nullptr)
			{
				DecalMaterials[I]->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
			}
			DecalOpacity[I] = 1.0f;
			DecalBornT[I] = W.Now;
			if (!D->IsVisible())
			{
				D->SetVisibility(true);
			}
		}
	}
	// The scorches fade over their time.
	const double FadeS = FMath::Max(W.Tune.Explosion.ScorchS, 1.0);
	for (int32 I = 0; I < DecalPool.Num(); ++I)
	{
		if (DecalBornT[I] < 0.0 || DecalPool[I] == nullptr)
		{
			continue;
		}
		const double Age = W.Now - DecalBornT[I];
		const float Opacity = static_cast<float>(FMath::Clamp(1.0 - Age / FadeS, 0.0, 1.0));
		if (Opacity <= 0.0f || Age < -1.0)
		{
			DecalPool[I]->SetVisibility(false);
			DecalBornT[I] = -1.0;
			continue;
		}
		if (FMath::Abs(Opacity - DecalOpacity[I]) > 0.02f && DecalMaterials[I] != nullptr)
		{
			DecalMaterials[I]->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			DecalOpacity[I] = Opacity;
		}
	}
}

void USimFxSubsystem::DrawMunitions()
{
	FSimFxWorld& W = *Fx;
	if (Host != nullptr && Host->GetRootComponent() != nullptr)
	{
		while (MunitionPool.Num() < W.Budgets.MaxMunitions)
		{
			MunitionPool.Add(SimFxSubMakeMunition(Host, Host->GetRootComponent(), MunitionPool.Num()));
		}
	}
	const int32 Use = FMath::Min3(W.Meshes.Num(), MunitionPool.Num(), W.Budgets.MaxMunitions);
	for (int32 I = 0; I < MunitionPool.Num(); ++I)
	{
		UStaticMeshComponent* M = MunitionPool[I];
		if (M == nullptr)
		{
			continue;
		}
		if (I >= Use || W.Meshes[I].Mesh == nullptr)
		{
			if (M->IsVisible())
			{
				M->SetVisibility(false);
			}
			continue;
		}
		const FSimFxMeshDraw& D = W.Meshes[I];
		if (M->GetStaticMesh() != D.Mesh)
		{
			M->SetStaticMesh(D.Mesh);
			// A munition built from its shape is coloured by its vertices (FSimMunitionModels::Material); a real asset
			// drawing it (docs/scenery/ASSETS.md) keeps its own materials, its textures.
			M->SetMaterial(0, D.Mesh->IsAsset() ? nullptr : FSimMunitionModels::Material());
		}
		M->SetWorldLocationAndRotation(D.Pos, D.Rot);
		if (!M->IsVisible())
		{
			M->SetVisibility(true);
		}
	}
	W.StatMunitions = Use;
}

void USimFxSubsystem::HideAll()
{
	if (bHidden)
	{
		return;
	}
	bHidden = true;
	if (Fx.IsValid())
	{
		for (int32 L = 0; L < SimFxLayerCount; ++L)
		{
			Fx->Sprites[L].Reset();
		}
		Fx->SpriteCount = 0;
		Fx->StatRibbons = 0;
		Fx->StatRibbonPoints = 0;
		Fx->StatLights = 0;
		Fx->StatMunitions = 0;
		for (int32 L = 0; L < Layers.Num(); ++L)
		{
			UploadLayer(L);
		}
	}
	if (RibbonMesh != nullptr && RibbonOrderCount > 0)
	{
		RibbonMesh->ClearAllMeshSections();
		RibbonOrderCount = 0;
	}
	for (UPointLightComponent* L : LightPool)
	{
		if (L != nullptr && L->IsVisible())
		{
			L->SetVisibility(false);
		}
	}
	for (UStaticMeshComponent* M : MunitionPool)
	{
		if (M != nullptr && M->IsVisible())
		{
			M->SetVisibility(false);
		}
	}
}

// ------------------------------------------------------------------ queries

void USimFxSubsystem::ClearEffects()
{
	if (!Fx.IsValid())
	{
		return;
	}
	Fx->ClearAll();
	for (int32 I = 0; I < DecalPool.Num(); ++I)
	{
		if (DecalPool[I] != nullptr)
		{
			DecalPool[I]->SetVisibility(false);
		}
		if (DecalBornT.IsValidIndex(I))
		{
			DecalBornT[I] = -1.0;
		}
	}
}

bool USimFxSubsystem::GetWreckPose(int32 EntityId, FVector& OutLocation, FRotator& OutRotation, bool& bOutVisible) const
{
	if (!Fx.IsValid())
	{
		return false;
	}
	simcore::Vec3 Pos;
	simcore::fx::WreckRotation Rot;
	if (!Fx->Wrecks.PoseEnu(EntityId, Fx->Now, Pos, Rot, bOutVisible))
	{
		return false;
	}
	// The attitude in ENU (yaw clockwise from north, pitch up, roll right wing down), then its axes through the map's
	// conversion: whatever the georeference, the wreck's nose and top point where they do in the simulation.
	const double Sy = FMath::Sin(Rot.yaw_rad), Cy = FMath::Cos(Rot.yaw_rad);
	const double Sp = FMath::Sin(Rot.pitch_rad), Cp = FMath::Cos(Rot.pitch_rad);
	const double Sr = FMath::Sin(Rot.roll_rad), Cr = FMath::Cos(Rot.roll_rad);
	const simcore::Vec3 Fwd{Sy * Cp, Cy * Cp, Sp};
	const simcore::Vec3 Right0{Cy, -Sy, 0.0};
	const simcore::Vec3 Up0{Right0.y * Fwd.z - Right0.z * Fwd.y, Right0.z * Fwd.x - Right0.x * Fwd.z, Right0.x * Fwd.y - Right0.y * Fwd.x};
	const simcore::Vec3 Up{Up0.x * Cr + Right0.x * Sr, Up0.y * Cr + Right0.y * Sr, Up0.z * Cr + Right0.z * Sr};
	OutLocation = Fx->ToU(Pos);
	const FVector FwdU = Fx->ToU(simcore::Vec3{Pos.x + Fwd.x, Pos.y + Fwd.y, Pos.z + Fwd.z}) - OutLocation;
	const FVector UpU = Fx->ToU(simcore::Vec3{Pos.x + Up.x, Pos.y + Up.y, Pos.z + Up.z}) - OutLocation;
	OutRotation = FRotationMatrix::MakeFromXZ(FwdU, UpU).Rotator();
	return true;
}

void USimFxSubsystem::SetAuxView(FName Key, const FVector& Location, const FRotator& Rotation, float FovDeg, float WidthPx, bool bActive)
{
	if (!Fx.IsValid())
	{
		return;
	}
	if (!bActive)
	{
		Fx->Aux.Remove(Key);
		return;
	}
	FSimFxWorld::FAux& A = Fx->Aux.FindOrAdd(Key);
	A.View.Set(Location, Rotation, FovDeg, WidthPx, 0.75);
	A.Frame = GFrameCounter;
}

int32 USimFxSubsystem::GetSpriteCount() const
{
	return Fx.IsValid() ? Fx->SpriteCount : 0;
}

int32 USimFxSubsystem::GetRibbonCount() const
{
	return Fx.IsValid() ? Fx->StatRibbons : 0;
}

int32 USimFxSubsystem::GetLightCount() const
{
	return Fx.IsValid() ? Fx->StatLights : 0;
}

bool USimFxSubsystem::IsNight() const
{
	return Fx.IsValid() && Fx->Sun.bNight;
}
