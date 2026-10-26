#include "SimModelGallery.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimBasicModels.h"
#include "SimEntityActor.h"
#include "SimOsaVehicle.h"
#include "SimWorldSubsystem.h"
#include "UnrealClient.h"

namespace
{
	/** The gallery floats this high above the level's ground, clear of any terrain. */
	constexpr double kGalleryHeightCm = 300000.0;

	bool IsAircraftArchetype(const std::string& A)
	{
		return A == "jet" || A == "prop" || A == "helicopter" || A == "drone" || A == "missile";
	}
	bool IsVehicleArchetype(const std::string& A)
	{
		return A == "wheeled" || A == "tracked" || A == "truck" || A == "launcher" || A == "gun" || A == "radar";
	}
}

ASimModelGallery::ASimModelGallery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASimModelGallery::StartIfAsked(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}
	FString Folder;
	const bool bGallery = FParse::Value(FCommandLine::Get(), TEXT("SimModelGallery="), Folder);
	FString ShotsFolder;
	const bool bShots = FParse::Value(FCommandLine::Get(), TEXT("SimShots="), ShotsFolder);
	if (!bGallery && !bShots)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimModelGallery* G = World->SpawnActor<ASimModelGallery>(ASimModelGallery::StaticClass(), FTransform::Identity, Params);
	if (G == nullptr)
	{
		return;
	}
	G->bScenarioShots = !bGallery;
	G->Folder = bGallery ? Folder : ShotsFolder;
	FString Times = TEXT("20,60,120");
	FParse::Value(FCommandLine::Get(), TEXT("SimShotTimes="), Times, false);  // the whole list, commas and all
	FParse::Value(FCommandLine::Get(), TEXT("SimTimeScale="), G->TimeScale);
	TArray<FString> Parts;
	Times.ParseIntoArray(Parts, TEXT(","));
	for (const FString& P : Parts)
	{
		G->ShotTimes.Add(FCString::Atof(*P));
	}
	G->ShotTimes.Sort();
	UE_LOG(LogSAM3D, Log, TEXT("Model check: %s into %s"), bGallery ? TEXT("the gallery") : TEXT("scenario photographs"), *G->Folder);
}

void ASimModelGallery::BeginPlay()
{
	Super::BeginPlay();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	if (Camera != nullptr)
	{
		Camera->GetCameraComponent()->SetFieldOfView(60.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
}

void ASimModelGallery::LayOut()
{
	const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
	if (Lib == nullptr)
	{
		return;
	}
	// A fixed exposure bias: on its bright slab high above the terrain the auto exposure washed every view out white
	// (22 October 2026; -9 EV found by trying r.ExposureOffset).
	if (Camera != nullptr)
	{
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias = true;
		Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias = -9.0f;
		Camera->GetCameraComponent()->PostProcessBlendWeight = 1.0f;
	}
	// Three rows: aircraft, vehicles and sites, buildings and nature; each model after the last with room to spare.
	TArray<const simcore::scenery::ModelDef*> Rows[3];
	for (const simcore::scenery::ModelDef& M : Lib->models)
	{
		const int32 R = IsAircraftArchetype(M.archetype) ? 0 : (IsVehicleArchetype(M.archetype) ? 1 : 2);
		Rows[R].Add(&M);
	}
	const double RowY[3] = {0.0, 6000.0, 12000.0};  // cm
	double MaxX = 0.0;
	for (int32 R = 0; R < 3; ++R)
	{
		double X = 0.0;
		double Last = 0.0;
		for (const simcore::scenery::ModelDef* M : Rows[R])
		{
			const double Len = FMath::Max(2.0, FMath::Max(M->length_m, M->archetype == "tower" ? M->span_m : 0.0)) * 100.0;
			X += Last * 0.5 + Len * 0.5 + 800.0;
			Last = Len;
			const bool bAir = IsAircraftArchetype(M->archetype);
			const double Z = kGalleryHeightCm + (bAir && M->archetype != "missile" ? M->height_m * 50.0 + 100.0 : (bAir ? 100.0 : 0.0));
			USceneComponent* Holder = NewObject<USceneComponent>(this);
			Holder->SetupAttachment(Root);
			Holder->SetRelativeLocation(FVector(X, RowY[R], Z));
			Holder->RegisterComponent();
			FSimBasicModels::Build(UTF8_TO_TCHAR(M->id.c_str()), Holder);
			UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
			Label->SetupAttachment(Root);
			Label->SetRelativeLocation(FVector(X, RowY[R] - 1500.0, kGalleryHeightCm + 50.0));
			Label->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));  // readable from the south, where the cameras stand
			Label->SetText(FText::FromString(UTF8_TO_TCHAR(M->id.c_str())));
			Label->SetWorldSize(150.0f);
			Label->SetTextRenderColor(FColor::White);
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->RegisterComponent();
		}
		MaxX = FMath::Max(MaxX, X + Last);
	}
	// The ground: one wide grey-green slab under everything.
	if (UStaticMesh* Cube = FSimBasicModels::ShapeMesh(simcore::scenery::Shape::Box))
	{
		UStaticMeshComponent* Ground = NewObject<UStaticMeshComponent>(this);
		Ground->SetStaticMesh(Cube);
		Ground->SetupAttachment(Root);
		Ground->SetRelativeLocation(FVector(MaxX * 0.5, 6000.0, kGalleryHeightCm - 50.0));
		Ground->SetRelativeScale3D(FVector(MaxX / 100.0 + 200.0, 260.0, 1.0));
		Ground->SetMaterial(0, FSimBasicModels::ColourMaterial(FString(TEXT("#7d8a55"))));
		Ground->RegisterComponent();
	}
	// The views: along each row, from the south and above, every 90 m.
	for (int32 R = 0; R < 3; ++R)
	{
		double X = 0.0;
		double End = 0.0;
		for (const simcore::scenery::ModelDef* M : Rows[R])
		{
			End += FMath::Max(2.0, M->length_m) * 100.0 + 800.0;
		}
		const double Span = R == 2 ? 14000.0 : 9000.0;
		for (X = Span * 0.4; X < End + Span * 0.4; X += Span)
		{
			const FVector At(X, RowY[R], kGalleryHeightCm + 300.0);
			Views.Add({At + FVector(0.0, -Span * 0.75, Span * 0.28), At});
		}
	}
	UE_LOG(LogSAM3D, Log, TEXT("Model gallery: %d aircraft, %d vehicles and sites, %d buildings and nature; %d views"), Rows[0].Num(), Rows[1].Num(),
		Rows[2].Num(), Views.Num());
}

void ASimModelGallery::Aim(const FVector& From, const FVector& At)
{
	if (Camera == nullptr)
	{
		return;
	}
	Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Camera);
		// Clean pictures: no menu, no HUD, no debug text over them.
		if (PC->MyHUD != nullptr)
		{
			PC->MyHUD->bShowHUD = false;
		}
	}
	if (GEngine != nullptr)
	{
		GEngine->bEnableOnScreenDebugMessagesDisplay = false;
	}
}

bool ASimModelGallery::AimAtScenario(bool bAir)
{
	if (bAir)
	{
		// The nearest flying entity drawn as an aircraft, from behind and above.
		const ASimOsaVehicle* Osa = nullptr;
		for (TActorIterator<ASimOsaVehicle> It(GetWorld()); It; ++It)
		{
			Osa = *It;
			break;
		}
		const FVector Ref = Osa != nullptr ? Osa->GetActorLocation() : FVector::ZeroVector;
		ASimEntityActor* Best = nullptr;
		double BestD = 1e30;
		const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
		for (TActorIterator<ASimEntityActor> It(GetWorld()); It; ++It)
		{
			ASimEntityActor* A = *It;
			const FString Model = A->GetBasicModel();
			if (A->IsHidden() || Model.IsEmpty() || Lib == nullptr)
			{
				continue;
			}
			const simcore::scenery::ModelDef* M = Lib->Find(std::string(TCHAR_TO_UTF8(*Model)));
			if (M == nullptr || !IsAircraftArchetype(M->archetype) || M->archetype == "missile")
			{
				continue;
			}
			const double D = FVector::Dist(A->GetActorLocation(), Ref);
			if (D < BestD)
			{
				BestD = D;
				Best = A;
			}
		}
		if (Best == nullptr)
		{
			return false;
		}
		const FVector Fwd = Best->GetActorForwardVector();
		const FVector At = Best->GetActorLocation();
		const simcore::scenery::ModelDef* BestModel = Lib->Find(std::string(TCHAR_TO_UTF8(*Best->GetBasicModel())));
		const double Length = BestModel != nullptr ? BestModel->length_m : 15.0;
		const double Back = FMath::Max(2500.0, 300.0 * Length);  // about three lengths behind, a third of that above
		Followed = Best;
		FollowOffset = -Fwd * Back + FVector(0.0, 0.0, Back * 0.33);
		Aim(At + FollowOffset, At);
		const FBox Drawn = Best->GetComponentsBoundingBox(true);
		UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: following %s (%s), %.1f km from the battery; actor at %s scale %s, drawn %s size %s; camera at %s"),
			*Best->GetActorLabel(false), *Best->GetBasicModel(), BestD / 100000.0, *At.ToString(), *Best->GetActorScale3D().ToString(),
			*Drawn.GetCenter().ToString(), *Drawn.GetSize().ToString(), Camera != nullptr ? *Camera->GetActorLocation().ToString() : TEXT("-"));
		return true;
	}
	for (TActorIterator<ASimOsaVehicle> It(GetWorld()); It; ++It)
	{
		const FVector At = It->GetActorLocation();
		const FVector Offset = BatteryViewOffset(At);
		Aim(At + Offset, At);
		UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: the battery from %s"), *Offset.ToString());
		// Anything drawn floating over the vehicle, above its masts (a part placed outside it), is named in the log.
		for (TActorIterator<AActor> A(GetWorld()); A; ++A)
		{
			TArray<UPrimitiveComponent*> Prims;
			A->GetComponents<UPrimitiveComponent>(Prims);
			for (const UPrimitiveComponent* P : Prims)
			{
				if (P == nullptr || !P->IsVisible() || A->IsHidden())
				{
					continue;
				}
				const FVector C = P->Bounds.Origin;
				const double Above = C.Z - At.Z;
				if (FVector::Dist2D(C, At) < 2500.0 && Above > 700.0 && Above < 3000.0 && P->Bounds.SphereRadius < 300.0)
				{
					UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: %s.%s floats %.1f m over the battery (radius %.2f m)"),
						*A->GetName(), *P->GetName(), Above / 100.0, P->Bounds.SphereRadius / 100.0);
				}
			}
		}
		return true;
	}
	return false;
}

FVector ASimModelGallery::BatteryViewOffset(const FVector& At) const
{
	const FVector Default(-5000.0, -7500.0, 3500.0);  // 50 m south, 75 m west, 35 m up
	const USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetScenarioRunner() : nullptr;
	const simcore::MapTerrain* Ground = Runner != nullptr ? Runner->MapGround() : nullptr;
	if (Ground == nullptr)
	{
		return Default;  // flat ground: nothing stands in the way
	}
	// The map's woods and buildings have no collision: test the line of sight against the map's own obstacle heights.
	// Eight sides, starting from the default's, then higher: the first whose line to the vehicle's top clears them all.
	const simcore::Vec3 Base = USimWorldSubsystem::UnrealToEnu(At);
	const double GroundAt = Ground->HeightAt(Base.x, Base.y);
	const double ZOff = Base.z - GroundAt;  // ENU z minus height above sea level, near the vehicle
	for (const double Up : {35.0, 60.0, 100.0})
	{
		for (int32 Side = 0; Side < 8; ++Side)
		{
			const double Az = FMath::DegreesToRadians(236.0 + 45.0 * Side);
			const double Dist = 90.0;
			const simcore::Vec3 Cam{Base.x + Dist * FMath::Sin(Az), Base.y + Dist * FMath::Cos(Az), GroundAt + Up + ZOff};
			const double TopAlt = GroundAt + 3.0;
			bool bClear = true;
			for (int32 i = 1; i <= 30 && bClear; ++i)
			{
				const double F = i / 30.0;
				const double E = Base.x + (Cam.x - Base.x) * F;
				const double N = Base.y + (Cam.y - Base.y) * F;
				const double Alt = TopAlt + (GroundAt + Up - TopAlt) * F;
				bClear = Alt > Ground->HeightAt(E, N) + Ground->ObstacleHeightAt(E, N) + 1.0;
			}
			if (bClear)
			{
				return USimWorldSubsystem::EnuToUnreal(Cam) - At;
			}
		}
	}
	return FVector(0.0, 0.0, 15000.0);  // boxed in: straight down from 150 m
}

void ASimModelGallery::LogEntities() const
{
	int32 Count = 0;
	for (TActorIterator<ASimEntityActor> It(GetWorld()); It; ++It)
	{
		++Count;
		UE_LOG(LogSAM3D, Log, TEXT("  entity %d %s: %s%s"), It->EntityId, *It->GetActorLabel(false),
			It->GetBasicModel().IsEmpty() ? TEXT("placeholder") : *It->GetBasicModel(), It->IsHidden() ? TEXT(" (hidden)") : TEXT(""));
	}
	UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: %d entity actors"), Count);
}

void ASimModelGallery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Folder.IsEmpty())
	{
		return;
	}
	Clock += DeltaSeconds;
	if (!bScenarioShots)
	{
		// The gallery: lay out, then each view: aim, wait for the picture to settle, shoot.
		if (Step == 0 && Clock > 3.0f)
		{
			LayOut();
			Step = 1;
			NextAt = Clock + 4.0f;
			return;
		}
		if (Step >= 1 && Clock >= NextAt)
		{
			const int32 V = (Step - 1) / 2;
			if (V >= Views.Num())
			{
				UE_LOG(LogSAM3D, Log, TEXT("Model gallery: done"));
				FGenericPlatformMisc::RequestExit(false);
				Folder.Reset();
				return;
			}
			if ((Step - 1) % 2 == 0)
			{
				Aim(Views[V].Key, Views[V].Value);
				NextAt = Clock + 1.5f;
			}
			else
			{
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("gallery_%02d.png"), V)), false, false);
				NextAt = Clock + 1.0f;
			}
			++Step;
		}
		return;
	}
	// The scenario's photographs, by the scenario's own clock (sped up by -SimTimeScale: the sim's own time scale). The
	// simulation is paused for each pair of shots, so what is aimed at is still there when the picture is taken.
	USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	if (Sim == nullptr || !Sim->IsScenarioRunning())
	{
		return;
	}
	if (TimeScale > 1.0f && FMath::Abs(Sim->GetTimeScale() - TimeScale) > 0.01f)
	{
		Sim->SetTimeScale(TimeScale);
	}
	const float T = static_cast<float>(Sim->GetSimTimeSeconds());
	constexpr int32 Subs = 7;
	// Keep the chase camera on the aircraft until its picture is taken (it may still move a frame after the pause).
	if (Step % Subs == 1 && Followed.IsValid())
	{
		const FVector At = Followed->GetActorLocation();
		Aim(At + FollowOffset, At);
	}
	if (Step >= ShotTimes.Num() * Subs)
	{
		if (Phase == 0)
		{
			UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: done"));
			FGenericPlatformMisc::RequestExit(false);
			Phase = 1;
		}
		return;
	}
	const int32 Shot = Step / Subs;
	const int32 Sub = Step % Subs;
	const float When = ShotTimes[Shot];
	if (Sub == 0 && T >= When)
	{
		Sim->SetSimPaused(true);
		Followed.Reset();
		LogEntities();
		if (!AimAtScenario(true))
		{
			UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: no aircraft in the air at %.0f s"), T);
			AimAtScenario(false);
		}
		NextAt = Clock + 0.6f;
		++Step;
	}
	else if (Sub == 1 && Clock >= NextAt)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("shot_%04.0f_air.png"), When)), false, false);
		NextAt = Clock + 0.5f;
		++Step;
	}
	else if (Sub == 2 && Clock >= NextAt)
	{
		AimAtScenario(false);
		NextAt = Clock + 0.6f;
		++Step;
	}
	else if (Sub == 3 && Clock >= NextAt)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("shot_%04.0f_battery.png"), When)), false, false);
		NextAt = Clock + 0.5f;
		++Step;
	}
	else if (Sub == 4 && Clock >= NextAt)
	{
		bNeighbourShot = AimAtNeighbour();
		NextAt = Clock + 0.6f;
		++Step;
	}
	else if (Sub == 5 && Clock >= NextAt)
	{
		if (bNeighbourShot)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("shot_%04.0f_neighbour.png"), When)), false, false);
		}
		NextAt = Clock + 0.5f;
		++Step;
	}
	else if (Sub == 6 && Clock >= NextAt)
	{
		Sim->SetSimPaused(false);
		++Step;
	}
}

bool ASimModelGallery::AimAtNeighbour()
{
	const USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>();
	const simcore::ScenarioRunner* Runner = Sim != nullptr ? Sim->GetScenarioRunner() : nullptr;
	if (Runner == nullptr || Runner->Neighbours().empty())
	{
		return false;
	}
	const simcore::Vec3 Own = Runner->UnitPosition();
	const simcore::ScenarioNeighbour* Best = nullptr;
	for (const simcore::ScenarioNeighbour& Nb : Runner->Neighbours())
	{
		if (Best == nullptr || (Nb.position - Own).HorizontalLength() < (Best->position - Own).HorizontalLength())
		{
			Best = &Nb;
		}
	}
	// Its first vehicle: a fighting Osa battery's first launcher, else the first of its system's site (a ring of guns has
	// none at the centre).
	simcore::Vec3 At = Best->position;
	const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
	const std::vector<std::pair<std::string, simcore::Vec3>> Site =
		Lib != nullptr ? Lib->SiteLayout(Best->system) : std::vector<std::pair<std::string, simcore::Vec3>>();
	if (!Site.empty())
	{
		At = Best->position + Site.front().second;
	}
	for (const std::pair<std::string, simcore::Vec3>& L : Runner->NeighbourLaunchers())
	{
		if (L.first == Best->id)
		{
			At = L.second;
			break;
		}
	}
	const FVector Target = Sim->ToUnreal(Runner->GroundEnu(At.x, At.y));
	const FVector From = Sim->ToUnreal(Runner->GroundEnu(At.x - 110.0, At.y - 110.0)) + FVector(0.0, 0.0, 7000.0);
	Aim(From, Target);
	UE_LOG(LogSAM3D, Log, TEXT("Scenario photographs: the neighbour %s (%s), %.1f km from the battery, from %s at %s (camera %s)"),
		UTF8_TO_TCHAR(Best->name.c_str()), UTF8_TO_TCHAR(Best->system.c_str()), (Best->position - Own).HorizontalLength() / 1000.0,
		*From.ToString(), *Target.ToString(), Camera != nullptr ? *Camera->GetActorLocation().ToString() : TEXT("-"));
	return true;
}
