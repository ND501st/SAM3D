#pragma once

// USimFxSubsystem's state (docs/fx/README.md 4.1, 5): the effects alive, the particles and ribbons, the views and the
// sun, and the frame's draw lists. SimFxSubsystem.cpp runs the frame and draws; SimFxEffects.cpp turns the feed's events
// into effects and moves them. Plain C++: the UObjects it draws with belong to the subsystem.

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"
#include "SimFxLogic.h"
#include "SimFxTuning.h"
#include "Stats/Stats.h"

#include <random>

DECLARE_STATS_GROUP(TEXT("SAM3DFx"), STATGROUP_SAM3DFx, STATCAT_Advanced);

class UWorld;
class USimWorldSubsystem;
class UStaticMesh;
struct FSimEntityState;

namespace SimFxIds
{
	constexpr int32 MissileBase = 100000;  // our engagement missiles (100000 + number)
	constexpr int32 ArmBase = 150000;      // EM anti-radiation missiles
	constexpr int32 GroundBase = 200000;   // battery vehicles and up: not aircraft
}

/** A ribbon owner's key with this bit is an aircraft's contrail (the rest: the effects' own NextKey counter). */
constexpr uint64 SimFxContrailKeyBit = 1ull << 62;

/** What the effects need of a munition (data/scenery/munitions.json through FSimMunitionModels). */
struct FSimFxMunition
{
	FString Id;            // the munition's own id; "" when the library has none for the asked id
	FString Kind;          // bomb, retarded, ballute, parachute, cluster, guided, glide, rocket, missile, sam, arm, cruise, shell, gun
	double LengthM = 3.0;
	double DiameterM = 0.2;
	double SpanM = 0.0;    // the widest fin span (tip to tip): a trail starts about this wide (LOOK 9.1)
	double ChargeKg = 0.0;
	bool bMotor = false;
	double BoostS = 0.0;
	double SustainS = 0.0;
	FString Smoke = TEXT("none");         // dense, reduced, minimal, none (the boost's)
	FString SustainSmoke = TEXT("none");
	FLinearColor PlumeColour = FLinearColor(1.0f, 0.7f, 0.4f);
	bool bPlumeColour = false;
	bool bTailTracer = false;
	double SmokeS = 0.0;
	bool bRetarder = false;
	FString RetarderType;
	double RetarderOpenS = 0.0;
	double RetarderDescentMps = 0.0;  // a chute's or ballute's steady descent (0: the path's own 47 m/s)
	bool bCluster = false;
	double OpensAglM = 0.0;
	double OpensS = 0.0;
	int32 Submunitions = 0;
	double PatternWM = 0.0;
	double PatternLM = 0.0;
	double CalibreMm = 0.0;
	double MuzzleMps = 0.0;
	double RateRpm = 0.0;
	int32 Barrels = 1;
	double TracerS = -1.0;
	double SdLoS = 0.0;
	double SdHiS = 0.0;
	FLinearColor TracerColour = FLinearColor(1.0f, 0.45f, 0.15f);
	bool bTracerColour = false;
	bool bTimeFuze = false;
	bool bHasMesh = false;
	bool IsRound() const { return Kind == TEXT("gun") || Kind == TEXT("shell"); }
};

/** An explosion: played when it is seen while its smoke lasts (an unseen one spawns nothing). */
struct FSimFxBurst
{
	enum class EStyle : uint8
	{
		Warhead,   // a missile's warhead in the air: orange flash, dark grey-black puff, fragments
		Ground,    // on the ground: flash, fireball, brown-grey dust column, debris, ring, scorch, light
		Flak,      // a large-calibre shell in the air: dark puff with a flash
		SdPuff,    // a small shell destroying itself: a small grey puff
		Kill,      // an aircraft destroyed: fuel fireball, black smoke, debris
		Small,     // a submunition or a gun's round on the ground: a small flash and dust
		Arm,       // an anti-radiation missile bursting just over a radar (LOOK 8): fragments sprayed along its path
		Haze,      // a cluster carpet's puffs merged into a low haze over its pattern (LOOK 5)
	};
	EStyle Style = EStyle::Ground;
	simcore::Vec3 Enu;
	FVector Pos = FVector::ZeroVector;
	FVector Wind = FVector::ZeroVector;  // cm/s
	double T = 0.0;
	double ChargeKg = 0.0;
	simcore::fx::ExplosionLook Look;
	bool bSpawned = false;
	bool bDecal = false;
	bool bLightDone = false;     // its light flash (and scorch) given
	bool bLaunch = false;        // a launch: the flash and the launch cloud, not a warhead
	float LaunchScale = 1.0f;    // a launch cloud's size against our own launcher's
	FVector Dir = FVector::UpVector;  // a launch: the missile's way out (the cloud rolls out behind it); a haze: its track
	FVector Vel = FVector::ZeroVector;  // cm/s: what burst was moving so (a missile's fragments fly on ahead, a fuel
	                                    // fireball is carried along the aircraft's path)
	double CalibreMm = 0.0;     // a shell's (flak by calibre, LOOK 4)
	bool bSoot = false;         // a TNT fill: a blacker column (LOOK 0.2)
	bool bCruise = false;       // a cruise missile's warhead: masonry dust, debris wider and flatter (LOOK 7)
	float Crowd = 1.0f;         // the share of its clods, sparks and skirt drawn (SimFxLogic::CrowdShare: a stick's later
	                            // bursts among its earlier ones)
	bool bWp = false;           // an ARM with a white-phosphorus marker (LOOK 8)
	float HazeW = 0.0f;         // a haze: its pattern across and along the track, m
	float HazeL = 0.0f;
	uint32 Seed = 0;
};

/** A light flash that fades (an explosion's, a launch's). */
struct FSimFxFlashLight
{
	FVector Pos = FVector::ZeroVector;
	FLinearColor Colour = FLinearColor::White;
	double Lumens = 0.0;  // at its start, before the light pool's scale
	double RadiusCm = 1000.0;
	double T = 0.0;
	double DurationS = 0.3;
};

/** A ribbon point: where it was laid and when. */
struct FSimFxRibbonPoint
{
	FVector Pos = FVector::ZeroVector;  // cm, where it was laid
	float BornT = 0.0f;                 // simulation time, relative to the session's base (float precision is enough)
	float Width0 = 100.0f;              // cm
	float Opacity0 = 1.0f;
	float DistM = 0.0f;                 // along the trail from its first point (the texture's U: it stays in the air, T10)
	float SpreadK = 1.0f;               // its widening against the ribbon's (uneven: the trail breaks into puffs, LOOK 9.3)
	FVector Drift = FVector::ZeroVector;  // cm/s: its own wind (the wind at its height: shear, LOOK 9.1) and turbulence
	FVector Side = FVector::ZeroVector;   // cm: where it rolls to (a contrail's plume into its vortex, LOOK 12)
};

/** How a ribbon looks and moves (one per kind: a missile's smoke, a contrail, a wreck's smoke). */
struct FSimFxRibbonStyle
{
	FLinearColor Colour = FLinearColor::White;  // its albedo, lit per point by the sun, the sky and the frame's lights
	float LifeS = 30.0f;          // a point's lifetime
	float SpreadCmps = 50.0f;     // how fast it widens, cm/s
	float SpreadJitter = 0.0f;    // each point's widening x (1 +- this): uneven, it breaks up
	float TurbCmps = 0.0f;        // the eddies' drift across it, cm/s, smooth along the trail
	float TileM = 40.0f;          // the texture's length along the trail, m
	float WaveM = 150.0f;         // the widening's and the eddies' wavelength along the trail (several points to one)
	float ThinS = 0.0f;           // TrailOpacityAt: thinning to ThinShare by ThinS (0: none)
	float ThinShare = 1.0f;
	float ErodeFrom = 0.6f;       // TrailErosionAt: erosion from this share of its life
	float RollS = 0.0f;           // RibbonPointAt: rolled toward a point's Side, e-folding (0: at once)
	float SinkCmps = 0.0f;        // and sunk (a contrail's vortex pair)
	float SinkMaxCm = 0.0f;
	float CrowPeriodM = 0.0f;     // CrowAt: the vortex rings' spacing (0: none)
	float CrowFromS = 0.0f;
	float CrowFullS = 0.0f;
	bool bPuffsNear = false;      // near the camera and seen along its length, drawn as puffs (TrailPuffShare)
};

/** A camera-facing strip: a missile's smoke, a contrail, a wreck's smoke. */
struct FSimFxRibbon
{
	bool bUsed = false;
	bool bEmitting = false;
	uint64 Owner = 0;
	TArray<FSimFxRibbonPoint> Points;
	FVector Drift = FVector::ZeroVector;  // cm/s: the wind carrying it (each point adds its own)
	FSimFxRibbonStyle Style;
	float SpreadCmps = 50.0f;             // how fast it widens, cm/s (Style's)
	float LifeS = 30.0f;                  // a point's lifetime (Style's)
	float UOffset = 0.0f;                 // where its texture starts (each trail a different stretch of it)
	FVector TurbA = FVector::ZeroVector;  // the eddies' two directions (unit, horizontal) and phases
	FVector TurbB = FVector::ZeroVector;
	float TurbPhase = 0.0f;
	FLinearColor Colour = FLinearColor::White;
	// The geometry last built (reused for a far ribbon three frames in four).
	TArray<FVector> Verts;
	TArray<FLinearColor> Colours;
	TArray<FVector2D> Uvs;
	TArray<FVector2D> Uvs1;   // (erosion, hairline share)
	TArray<FVector2D> Uvs2;   // the frame's lights on it (T9), R and G, already times its albedo
	TArray<FVector2D> Uvs3;   // B, and its true width in metres (M_FX_Ribbon's camera fade)
	int32 BuiltFrame = -100;
	bool bVisible = false;
};

/** A missile drawn with plume and smoke: an entity of the snapshot (our missiles, EM ARMs), or flown here. */
struct FSimFxMissile
{
	enum class EMode : uint8 { Entity, Flown };
	EMode Mode = EMode::Entity;
	int32 EntityId = -1;
	int32 Number = 0;          // our missile's number (0: not ours)
	uint64 Key = 0;            // the ribbon's owner key
	FString Weapon;
	FString Source;            // the event's source (a neighbour, the raid aircraft of an ARM)
	bool bArm = false;
	const FSimFxMunition* Mun = nullptr;
	double LaunchT = 0.0;
	double SeenT = 0.0;        // an entity's: when it was last in the snapshot
	// Flown: from (P0, V0) at T0 to Aim at EndT, chasing TargetId (if any) on the way.
	simcore::Vec3 P0, V0, Aim, Pos, Vel;
	double T0 = 0.0;
	double EndT = 0.0;
	int32 TargetId = -1;
	FString TargetName;
	double SpeedMps = 700.0;
	bool bBurstAtEnd = true;
	FSimFxBurst::EStyle BurstStyle = FSimFxBurst::EStyle::Warhead;
	double ChargeKg = 15.0;
	bool bWaitForEnd = false;  // a SAM bursts on its SamEnd, not by itself (unless long overdue)
	// Drawn this frame (Unreal).
	FVector PosU = FVector::ZeroVector;
	FVector FwdU = FVector::ForwardVector;
	bool bAlive = true;
	bool bDone = false;
	int32 Ribbon = -1;
	FVector LastTrailPos = FVector::ZeroVector;
	bool bHaveTrail = false;
	bool bLowSmoke = false;    // HARM and its like: a faint trail
	bool bAam = false;         // an air-to-air missile (AamLaunch): leaves at its shooter's speed, ends on its AamEnd
	bool bShowMesh = false;    // flown here: its mesh from the munition pool
	float HelixPhase = 0.0f;   // a MANPADS's corkscrew: where its turn starts (LOOK 9.2)
	FVector HelixAxisPos = FVector::ZeroVector;  // its axis where the last helix point was laid
	double HelixDistM = 0.0;   // the helix's length laid
};

/** One weapon released from an aircraft: a bomb, a rocket, a missile or a cluster dispenser, on its path. */
struct FSimFxBomb
{
	FString Weapon;
	const FSimFxMunition* Mun = nullptr;
	int32 SourceId = -1;
	FString Source;
	double ReleaseT = 0.0;
	double ImpactT = 0.0;
	simcore::Vec3 ImpactEnu;
	simcore::Vec3 FallbackPos, FallbackVel;  // the aircraft at its release instant if it is not in the snapshot
	bool bReleased = false;
	bool bDone = false;
	simcore::fx::BombPath Path;
	simcore::fx::BombPathParams::Kind PathKind = simcore::fx::BombPathParams::FreeFall;
	FString Part;              // blades_open, ballute, chute: swapped in at PartT
	double PartT = 0.0;
	double ChuteDescentMps = 0.0;  // the munition's descent under its chute (0: BombPathParams' default)
	double ClusterOpenT = -1.0;
	bool bClusterOpened = false;
	double ChargeKg = 0.0;
	uint64 Key = 0;
	int32 Ribbon = -1;
	FVector LastTrailPos = FVector::ZeroVector;
	bool bHaveTrail = false;
	bool bFire = false;
	bool bInert = false;       // jettisoned (delivery "jettison"): it lands without going off
};

/** An aircraft's gun burst at the ground: flashes at the aircraft and a line of impacts walking to the aim. */
struct FSimFxGunRun
{
	int32 SourceId = -1;
	const FSimFxMunition* Mun = nullptr;
	double StartT = 0.0;
	double FireEndT = 0.0;
	double ImpactStartT = 0.0;
	double ImpactEndT = 0.0;
	simcore::Vec3 From, To;    // the line of impacts on the ground (ENU)
	simcore::Vec3 Muzzle0, MuzzleVel;
	double NextImpactT = 0.0;
	double NextFlashT = 0.0;
	bool bDone = false;
};

/** A decoy flare (FlareDrop, roadmap B4): a bright point falling from the aircraft, burning, with its smoke. */
struct FSimFxFlare
{
	simcore::Vec3 Pos, Vel;    // ENU m, m/s
	double BornT = 0.0;        // ejected then (a salvo's flares one after another)
	double BurnS = 4.5;
	int32 SourceId = -1;       // the aircraft it leaves (its drawn place at BornT; the event's state if not drawn)
	simcore::Vec3 FallbackPos, FallbackVel;
	simcore::Vec3 Eject;       // its throw out of the dispenser (m/s)
	bool bLive = false;
	uint64 Key = 0;
	int32 Ribbon = -1;
	FVector LastTrailPos = FVector::ZeroVector;
	bool bHaveTrail = false;
	bool bDone = false;
};

/** One round of a gun pass in flight. */
struct FSimFxRound
{
	double FireT = 0.0;
	simcore::Vec3 Muzzle, Dir;  // ENU
	double SdT = 0.0;           // when it destroys itself (0: never)
	double BurstT = 0.0;        // a flak shell's fuze time (0: none)
	bool bTracer = true;
	bool bEnded = false;
	bool bExact = false;        // the simulation's shell (a FlakBurst): it bursts where it says, not scattered here
};

/** A defence's guns through a pass at an aircraft. */
struct FSimFxGunPass
{
	simcore::Vec3 Site;
	FVector SiteU = FVector::ZeroVector;
	int32 TargetId = -1;
	FString TargetName;
	simcore::Vec3 AimFallback;   // the aircraft where the pass began
	double T0 = 0.0;
	double T1 = 0.0;
	const FSimFxMunition* Mun = nullptr;
	FString Weapon;
	TArray<FSimFxRound> Rounds;
	double NextShotT = 0.0;
	double BurstEndT = 0.0;
	double NextBurstT = 0.0;
	TArray<SimFxLogic::FGunBurst> Bursts;  // the pass's bursts (SimFxLogic::GunBursts: the sound fires the same)
	int32 BurstIndex = 0;
	int32 Barrel = 0;
	int32 RoundIndex = 0;      // the rounds fired: which of the belt's carry a tracer (LOOK 14)
	bool bBurstSmoked = false; // this burst's gun smoke given
	uint32 Rng = 1;
	bool bFlak = false;  // a time fuze (the KS-30's): bursts round the target's height
	bool bDone = false;
	// The fire control's errors through the pass (the GunPass event's, simcore/aaa.hpp): the rounds aim where the
	// simulation's do. Zero: a perfect lead.
	double AimAcrossRad = 0.0;
	double AimUpRad = 0.0;
	double RangeShare = 0.0;
	simcore::Vec3 LeadError;
	double CorrectionPerS = 0.0;
	double SpreadMrad = 0.0;   // a round's scatter (0: the tuning's)
	// A fighter's burst at an aircraft (AirGunBurst, roadmap B13): the site is the shooter, moving; no gun smoke.
	bool bAir = false;
	int32 SiteId = -1;
	simcore::Vec3 SiteVel;
};

/** A damaged aircraft flying on (AircraftDamaged): its smoke trail and its fire, until it is down or gone. */
struct FSimFxDamageFx
{
	int32 Id = -1;
	uint64 Key = 0;
	int32 Level = 0;           // simcore::DamageLevel: 2 moderate, 3 heavy
	bool bSmoke = false;
	bool bFire = false;
	int32 Ribbon = -1;
	FVector LastTrailPos = FVector::ZeroVector;
	bool bHaveTrail = false;
	double FireAccum = 0.0;
};

/** A wreck's effects: its smoke and fire while it falls, its impact. */
struct FSimFxWreckFx
{
	int32 Id = -1;
	uint64 Key = 0;
	int32 Ribbon = -1;
	FVector LastTrailPos = FVector::ZeroVector;
	bool bHaveTrail = false;
	double FireAccum = 0.0;
	bool bLanded = false;
};

/** A fire burning on the ground and the smoke column rising from it, minutes long. */
struct FSimFxFire
{
	FVector Pos = FVector::ZeroVector;
	FVector Wind = FVector::ZeroVector;
	double StartT = 0.0;
	double FireEndT = 0.0;
	double SmokeEndT = 0.0;
	double SizeM = 5.0;
	double FireAccum = 0.0;
	double SmokeAccum = 0.0;
};

/** An aircraft's contrails and afterburner. */
struct FSimFxAircraftFx
{
	int32 Id = -1;
	FString Model;
	TArray<FVector> Nozzles;      // model frame, cm (X forward, Y right, Z up)
	double NextEvalT = -1.0;
	simcore::fx::ContrailState State;
	int32 Ribbons[4] = {-1, -1, -1, -1};
	FVector LastPoint[4];
	double SpanM = 10.0;          // its wingspan: the vortex pair 0.78 span apart, the Crow rings every 8.6 spans (LOOK 12)
	bool bBlueFlame = false;      // an engine whose afterburner shows blue at night (LOOK 13)
	double LastPointT = -1.0;
	bool bSeen = false;
};

/** A munition mesh drawn this frame (from the pool). */
struct FSimFxMeshDraw
{
	UStaticMesh* Mesh = nullptr;
	FVector Pos = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
};

/** A light wanted this frame. */
struct FSimFxLightDraw
{
	FVector Pos = FVector::ZeroVector;
	FLinearColor Colour = FLinearColor::White;
	double Lumens = 0.0;
	double RadiusCm = 1000.0;
	double Score = 0.0;
};

/** A decal wanted (a scorch where a bomb burst). */
struct FSimFxDecalWant
{
	FVector Pos = FVector::ZeroVector;
	double RadiusCm = 300.0;
	float Yaw = 0.0f;
};

/** The per-frame budgets and switches (the sam3d.fx.* cvars). */
struct FSimFxBudgets
{
	int32 MaxSprites = 4000;
	int32 MaxRibbons = 48;
	int32 MaxLights = 8;
	int32 MaxDecals = 32;
	int32 MaxMunitions = 48;
	double EmissiveScale = 1.0;
};

/** The sun and the sky's light, as the effects' materials need them. */
struct FSimFxSun
{
	FVector ToSun = FVector(0.3, 0.2, 0.93).GetSafeNormal();
	double ElevationDeg = 40.0;
	double Lux = 10.0;
	FLinearColor Colour = FLinearColor::White;  // the light's colour, reddened low
	FLinearColor SunWhite = FLinearColor(3.18f, 3.18f, 3.18f);   // a white surface lit by it: lux * colour / pi
	FLinearColor SmokeSunWhite = FLinearColor(1.0f, 1.0f, 1.0f); // and smoke (Lighting.SmokeSun of it)
	FLinearColor SmokeAmbient = FLinearColor(0.3f, 0.3f, 0.3f);  // the sky's light on smoke (Lighting.SmokeSky of Ambient)
	FLinearColor Ambient = FLinearColor(0.7f, 0.8f, 1.0f);      // a white surface in the shade
	bool bNight = false;
	bool bFound = false;
};

class FSimFxWorld
{
public:
	FSimFxTuning Tune;
	FSimFxBudgets Budgets;
	FSimFxLod Lod;
	FSimFxSun Sun;

	// The clock: the simulation's render time, and the base the ribbon points' float times count from.
	double Now = 0.0;
	double LastNow = -1.0;
	float Dt = 0.0f;
	double TimeBase = 0.0;
	int32 Frame = 0;

	USimWorldSubsystem* Sim = nullptr;
	simcore::ScenarioRunner* Runner = nullptr;
	FSimFxFeedCursor Cursor;

	// The snapshot this frame, by id.
	TMap<int32, int32> StateIndex;
	const TArray<FSimEntityState>* States = nullptr;

	// Views: [0] the player's camera, then the aux views.
	TArray<FSimFxView> Views;
	struct FAux
	{
		FSimFxView View;
		uint64 Frame = 0;
	};
	TMap<FName, FAux> Aux;
	FVector CamPos = FVector::ZeroVector;

	// Effects.
	FSimFxParticles Particles;
	TArray<FSimFxBurst> Bursts;
	TArray<FVector4> RecentGround;  // the ground bursts played in full lately: where (cm) and when (s), for CrowdShare
	TArray<FSimFxFlashLight> FlashLights;
	TArray<FSimFxMissile> Missiles;
	TArray<FSimFxBomb> Bombs;
	TArray<FSimFxGunRun> GunRuns;
	TArray<FSimFxGunPass> GunPasses;
	TArray<FSimFxFlare> Flares;
	FSimFxWrecks Wrecks;
	TArray<FSimFxWreckFx> WreckFx;
	TArray<FSimFxDamageFx> DamageFx;
	TArray<FSimFxFire> Fires;
	TMap<int32, FSimFxAircraftFx> Aircraft;
	TArray<FSimFxRibbon> Ribbons;
	TSet<int32> EndedMissiles;  // our missiles' entity ids whose MissileEnd has come (never picked up again)
	// Each munition held by pointer: the effects keep pointers to them while the cache grows.
	TMap<FString, TUniquePtr<FSimFxMunition>> MunitionCache;
	TMap<FString, TArray<FVector>> NozzleCache;
	uint64 NextKey = 1;
	std::mt19937 Rng{0x5A3D};

	// The frame's draw lists.
	TArray<FSimFxSprite> Sprites[SimFxLayerCount];
	int32 SpriteCount = 0;
	TArray<FSimFxMeshDraw> Meshes;
	TArray<FSimFxLightDraw> Lights;
	TArray<FSimFxDecalWant> NewDecals;

	// Stats of the last frame.
	int32 StatRibbons = 0;
	int32 StatRibbonPoints = 0;
	int32 StatLights = 0;
	int32 StatMunitions = 0;

	// ------------------------------------------------------------------ SimFxEffects.cpp

	/** Every effect gone (a new scenario, the feed gone backwards). */
	void ClearAll();
	/** One event of the feed. */
	void OnEvent(const simcore::fx::Event& E);
	/** Move every effect to Now and put its sprites, ribbon points, meshes and lights in the frame's lists. */
	void UpdateEffects();

	const FSimFxMunition& Munition(const FString& AnyId);
	const FSimEntityState* Entity(int32 Id) const;
	const FSimEntityState* EntityByName(const FString& Name) const;
	/** Where an entity is drawn now (its state carried to the render time). */
	FVector EntityPos(const FSimEntityState& S) const;

	FVector ToU(const simcore::Vec3& Enu) const;
	simcore::Vec3 ToE(const FVector& U) const;
	/** The wind at a place (ENU m), as a drift in Unreal cm/s. */
	FVector WindAt(const simcore::Vec3& Enu) const;
	double AltitudeOf(const simcore::Vec3& Enu) const;
	simcore::Vec3 GroundUnder(const simcore::Vec3& Enu) const;

	/** Add a sprite to this frame (false when the budget is spent or it is outside every view). */
	bool AddSprite(const FSimFxSprite& S);
	bool Spawn(const FSimFxParticle& P) { return Particles.Spawn(P); }
	/** A ribbon from the pool for this owner, whose trail is laid at At (-1 if none free or none it may take over). */
	int32 AcquireRibbon(uint64 Owner, const FSimFxRibbonStyle& Style, const FVector& Drift, const FVector& At);
	/** A point laid at Pos: its own drift (the wind there) and, for a contrail, where it rolls to (Side). */
	void AddRibbonPoint(int32 Ribbon, const FVector& Pos, float WidthCm, float Opacity, const FVector& Drift = FVector::ZeroVector,
		const FVector& Side = FVector::ZeroVector);
	/** Stop laying a ribbon: it fades and is freed. Only when Owner still holds it (it may have been taken over). */
	void ReleaseRibbon(int32 Ribbon, uint64 Owner);
	float RibbonTime() const { return static_cast<float>(Now - TimeBase); }

	/** Add an explosion at Now (or at T, later); VelEnu what burst was moving at (m/s, ENU). Returns it (valid until
	 *  the next burst is added). */
	FSimFxBurst& AddBurst(FSimFxBurst::EStyle Style, const simcore::Vec3& Enu, double ChargeKg, double T,
		const simcore::Vec3& VelEnu = simcore::Vec3());
	/** True if the munition is one of the ids listed (a prefix match: "fab" holds fab250 and fab500). */
	static bool MunitionListed(const TArray<FString>& List, const FString& Id);
	void PlayBurst(FSimFxBurst& B, ESimFxTier Tier);
	void AddFlashLight(const FVector& Pos, const FLinearColor& Colour, double Lumens, double RadiusCm, double DurationS, double StartT);
	void AddLight(const FVector& Pos, const FLinearColor& Colour, double Lumens, double RadiusCm);

	/** Effect helpers. */
	float Brightness(double B) const { return static_cast<float>(B * Budgets.EmissiveScale * (Sun.Lux / FMath::Max(Tune.Lighting.SunReferenceLux, 1e-3))); }
	double Rand01() { return std::uniform_real_distribution<double>(0.0, 1.0)(Rng); }
	double RandRange(double A, double B) { return A + (B - A) * Rand01(); }
	FVector RandUnit();
	ESimFxTier TierAt(const FVector& Pos, double SizeM, double* OutPx = nullptr) const
	{
		return SimFxLogic::TierFor(Views, Pos, SizeM, Lod, OutPx);
	}
	bool Seen(const FVector& Pos, double RadiusCm) const { return SimFxLogic::InAnyView(Views, Pos, RadiusCm, Lod.Margin); }
	double DistToCamM(const FVector& Pos) const { return FVector::Dist(Pos, CamPos) / 100.0; }
	/** How far a trail is laid (m): a contrail's (bContrail) or a smoke trail's (SimFxLogic::TrailRangeM). */
	double TrailRangeM(bool bContrail) const
	{
		return SimFxLogic::TrailRangeM(Lod, bContrail ? Tune.Lod.ContrailRangeScale : 1.0, Tune.Lod.TrailMarginM);
	}
	/** An aircraft's contrail ribbon's owner key, engine E. */
	static uint64 ContrailKey(int32 Id, int32 E)
	{
		return (static_cast<uint64>(static_cast<uint32>(Id)) << 8) | static_cast<uint64>(E) | SimFxContrailKeyBit;
	}

	void UpdateMissiles();
	void UpdateBombs();
	void UpdateGunRuns();
	void UpdateGunPasses();
	void UpdateFlares();
	void UpdateWrecks();
	void UpdateDamaged();
	void UpdateFires();
	void UpdateAircraft();
	void UpdateBursts();
	void DrawMissile(FSimFxMissile& M, const FVector& Pos, const FVector& Fwd, double SinceLaunch);
	/** Lay a trail: the head follows At each frame, a point is left every PointM; PointDrift the wind there (cm/s), Side
	 *  where the point rolls to. */
	void TrailPoint(int32& Ribbon, uint64 Key, FVector& Last, bool& bHave, const FVector& At, float WidthCm, float Opacity, double PointM,
		const FSimFxRibbonStyle& Style, const FVector& PointDrift, const FVector& Side = FVector::ZeroVector);
	/** The styles of the trails (effects.json): a missile's smoke, a rocket's, a contrail, a wreck's smoke. */
	FSimFxRibbonStyle MissileTrailStyle(double LifeS) const;
	FSimFxRibbonStyle ContrailStyle(double LifeS, double SpreadMps, double SpanM) const;
	FSimFxRibbonStyle WreckTrailStyle() const;
	void LaunchMissileFx(const simcore::fx::Event& E);
	void FlownMissile(const simcore::fx::Event& E, bool bArm);
	void Release(const simcore::fx::Event& E);
	void StartGunPass(const simcore::fx::Event& E);
	void FlakShell(const simcore::fx::Event& E);
	void AircraftDamaged(const simcore::fx::Event& E);
	void AircraftDown(const simcore::fx::Event& E);
	/** The air battle (roadmap B13): a chaff salvo's cloud, a salvo of flares, an air-to-air missile's launch and end, a
	 *  fighter's gun burst. */
	void ChaffDrop(const simcore::fx::Event& E);
	void FlareDrop(const simcore::fx::Event& E);
	void AamLaunch(const simcore::fx::Event& E);
	void AamEnd(const simcore::fx::Event& E);
	void AirGunBurst(const simcore::fx::Event& E);
	const TArray<FVector>& NozzlesOf(const FString& Model);
};
