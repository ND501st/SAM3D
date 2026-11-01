// The weapons' effects (docs/fx/README.md 4.1): the feed's events turned into effects, and each effect moved on the
// simulation's clock and put into the frame's draw lists. USimFxSubsystem (SimFxSubsystem.cpp) runs the frame and draws.

#include "SimFxImpl.h"

#include "SimBasicModels.h"
#include "SimMunitionModels.h"
#include "SimWorldSubsystem.h"

namespace
{
	using simcore::Vec3;
	using EFxBurstStyle = FSimFxBurst::EStyle;

	FLinearColor SimFxEffHex(const std::string& Hex, const FLinearColor& Default, bool& bOutFound)
	{
		bOutFound = false;
		if (Hex.size() < 7 || Hex[0] != '#')
		{
			return Default;
		}
		const std::array<float, 3> C = simcore::scenery::LinearColour(Hex);
		bOutFound = true;
		return FLinearColor(C[0], C[1], C[2]);
	}

	/** A cubic Hermite between two states over H seconds, at U (0..1), and its rate. */
	Vec3 SimFxEffHermite(const Vec3& P0, const Vec3& V0, const Vec3& P1, const Vec3& V1, double H, double U)
	{
		const double U2 = U * U, U3 = U2 * U;
		return P0 * (2.0 * U3 - 3.0 * U2 + 1.0) + V0 * (H * (U3 - 2.0 * U2 + U)) + P1 * (-2.0 * U3 + 3.0 * U2) + V1 * (H * (U3 - U2));
	}
	Vec3 SimFxEffHermiteRate(const Vec3& P0, const Vec3& V0, const Vec3& P1, const Vec3& V1, double H, double U)
	{
		const double U2 = U * U;
		return (P0 * (6.0 * U2 - 6.0 * U) + P1 * (-6.0 * U2 + 6.0 * U)) / FMath::Max(H, 1e-6) + V0 * (3.0 * U2 - 4.0 * U + 1.0) +
			V1 * (3.0 * U2 - 2.0 * U);
	}

	/** The trail's width and opacity for a motor's smoke ("dense", "reduced", "minimal"; anything else none). */
	bool SimFxEffSmokeLevel(const FString& Smoke, const FSimFxTuning::FMissile& T, double& OutWidthM, double& OutOpacity)
	{
		if (Smoke == TEXT("dense"))
		{
			OutWidthM = T.TrailWidthDenseM;
			OutOpacity = T.TrailOpacityDense;
			return true;
		}
		if (Smoke == TEXT("reduced"))
		{
			OutWidthM = T.TrailWidthReducedM;
			OutOpacity = T.TrailOpacityReduced;
			return true;
		}
		if (Smoke == TEXT("minimal"))
		{
			OutWidthM = T.TrailWidthMinimalM;
			OutOpacity = T.TrailOpacityMinimal;
			return true;
		}
		return false;
	}

	float SimFxEffFlicker(double Now, double Phase)
	{
		return static_cast<float>(0.85 + 0.15 * FMath::Sin(Now * 47.0 + Phase) * FMath::Sin(Now * 31.0 + Phase * 1.7));
	}

	uint32 SimFxEffHash(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352du;
		X ^= X >> 15;
		X *= 0x846ca68bu;
		X ^= X >> 16;
		return X;
	}

	/** A uniform 0..1 from a small state (deterministic per effect). */
	float SimFxEffRand(uint32& State)
	{
		State = SimFxEffHash(State + 0x9E3779B9u);
		return static_cast<float>(State & 0xFFFFFF) / 16777216.0f;
	}
}

// ------------------------------------------------------------------ helpers

const FSimFxMunition& FSimFxWorld::Munition(const FString& AnyId)
{
	if (const TUniquePtr<FSimFxMunition>* Found = MunitionCache.Find(AnyId))
	{
		return **Found;
	}
	TUniquePtr<FSimFxMunition> Made = MakeUnique<FSimFxMunition>();
	FSimFxMunition& M = *Made;
	const simcore::scenery::MunitionSpec* S = AnyId.IsEmpty() ? nullptr : FSimMunitionModels::Resolve(AnyId);
	if (S != nullptr)
	{
		M.Id = UTF8_TO_TCHAR(S->id.c_str());
		M.Kind = UTF8_TO_TCHAR(S->kind.c_str());
		M.LengthM = S->length_m > 0.0 ? S->length_m : M.LengthM;
		M.DiameterM = S->diameter_m > 0.0 ? S->diameter_m : M.DiameterM;
		M.SpanM = 2.0 * simcore::scenery::MaxRadius(*S);
		M.ChargeKg = S->charge_kg;
		M.bMotor = S->has_motor;
		if (S->has_motor)
		{
			M.BoostS = S->motor.boost_s;
			M.SustainS = S->motor.sustain_s;
			M.Smoke = UTF8_TO_TCHAR(S->motor.smoke.c_str());
			M.SustainSmoke = UTF8_TO_TCHAR(S->motor.SustainSmoke().c_str());
			M.PlumeColour = SimFxEffHex(S->motor.plume_colour, M.PlumeColour, M.bPlumeColour);
			M.bTailTracer = S->motor.tracer;
		}
		M.SmokeS = S->smoke_s;
		M.bRetarder = S->has_retarder;
		if (S->has_retarder)
		{
			M.RetarderType = UTF8_TO_TCHAR(S->retarder.type.c_str());
			M.RetarderOpenS = S->retarder.open_s;
			M.RetarderDescentMps = S->retarder.descent_mps;
		}
		M.bCluster = S->has_cluster;
		if (S->has_cluster)
		{
			M.OpensAglM = S->cluster.opens_agl_m;
			M.OpensS = S->cluster.opens_s;
			M.Submunitions = S->cluster.submunitions;
			M.PatternWM = S->cluster.pattern_w_m;
			M.PatternLM = S->cluster.pattern_l_m;
		}
		M.CalibreMm = S->calibre_mm;
		M.MuzzleMps = S->muzzle_mps;
		M.RateRpm = S->rate_rpm;
		M.Barrels = FMath::Max(S->barrels, 1);
		M.TracerS = S->tracer_s;
		M.SdLoS = S->sd_lo_s;
		M.SdHiS = S->sd_hi_s;
		M.TracerColour = SimFxEffHex(S->tracer_colour, M.TracerColour, M.bTracerColour);
		M.bTimeFuze = S->burst == "time";
		M.bHasMesh = S->HasMesh();
	}
	return *MunitionCache.Add(AnyId, MoveTemp(Made));
}

const FSimEntityState* FSimFxWorld::Entity(int32 Id) const
{
	if (States == nullptr || Id < 0)
	{
		return nullptr;
	}
	const int32* Index = StateIndex.Find(Id);
	return Index != nullptr ? &(*States)[*Index] : nullptr;
}

const FSimEntityState* FSimFxWorld::EntityByName(const FString& Name) const
{
	if (States == nullptr || Name.IsEmpty())
	{
		return nullptr;
	}
	for (const FSimEntityState& S : *States)
	{
		if (!S.bIsMissile && !S.bIsGround && S.Name == Name)
		{
			return &S;
		}
	}
	return nullptr;
}

FVector FSimFxWorld::EntityPos(const FSimEntityState& S) const
{
	return Sim != nullptr ? Sim->GetRenderLocation(S) : S.Location;
}

FVector FSimFxWorld::ToU(const Vec3& Enu) const
{
	return Sim != nullptr ? Sim->ToUnreal(Enu) : USimWorldSubsystem::EnuToUnreal(Enu);
}

Vec3 FSimFxWorld::ToE(const FVector& U) const
{
	return Sim != nullptr ? Sim->ToEnu(U) : USimWorldSubsystem::UnrealToEnu(U);
}

FVector FSimFxWorld::WindAt(const Vec3& Enu) const
{
	if (Runner == nullptr)
	{
		return FVector::ZeroVector;
	}
	const double Alt = AltitudeOf(Enu);
	const double Agl = FMath::Max(Alt - Runner->GroundHeightAt(Enu.x, Enu.y), 0.0);
	const Vec3 W = Runner->GetWeather().WindAt(Alt, Agl);
	return ToU(Enu + W) - ToU(Enu);
}

double FSimFxWorld::AltitudeOf(const Vec3& Enu) const
{
	return Runner != nullptr ? Runner->GetWorld().Frame().AltitudeOf(Enu) : Enu.z;
}

Vec3 FSimFxWorld::GroundUnder(const Vec3& Enu) const
{
	return Runner != nullptr ? Runner->GroundEnu(Enu.x, Enu.y) : Vec3{Enu.x, Enu.y, 0.0};
}

FVector FSimFxWorld::RandUnit()
{
	FVector V;
	do
	{
		V = FVector(RandRange(-1.0, 1.0), RandRange(-1.0, 1.0), RandRange(-1.0, 1.0));
	} while (V.SizeSquared() > 1.0 || V.SizeSquared() < 1e-4);
	return V.GetUnsafeNormal();
}

bool FSimFxWorld::AddSprite(const FSimFxSprite& S)
{
	if (SpriteCount >= Budgets.MaxSprites)
	{
		return false;
	}
	const double Radius = 0.5 * FMath::Max(S.Width, S.Length);
	// Past the effects' range or the weather's visibility nothing is drawn (docs/fx/README.md 4.1): an aircraft's
	// afterburner glow or a missile's motor 60 km off, held at its pixel floor, was a dot past what the eye could see.
	const double RangeCm = 100.0 * FMath::Min(Lod.MaxRangeM, Lod.VisibilityM) + Radius;
	if (FVector::DistSquared(S.Pos, CamPos) > RangeCm * RangeCm)
	{
		return false;
	}
	if (!SimFxLogic::InAnyView(Views, S.Pos, Radius, Lod.Margin))
	{
		return false;
	}
	Sprites[static_cast<int32>(S.Layer)].Add(S);
	++SpriteCount;
	return true;
}

int32 FSimFxWorld::AcquireRibbon(uint64 Owner, const FSimFxRibbonStyle& Style, const FVector& Drift, const FVector& At)
{
	int32 Pick = -1;
	const int32 Limit = FMath::Max(Budgets.MaxRibbons, 0);
	for (int32 I = 0; I < Ribbons.Num() && I < Limit; ++I)
	{
		if (!Ribbons[I].bUsed)
		{
			Pick = I;
			break;
		}
	}
	if (Pick < 0 && Ribbons.Num() < Limit)
	{
		Pick = Ribbons.AddDefaulted();
	}
	if (Pick < 0)
	{
		// Every ribbon in use: take over the one finished longest ago (not one still being laid).
		float Oldest = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Ribbons.Num() && I < Limit; ++I)
		{
			const FSimFxRibbon& R = Ribbons[I];
			const float Newest = R.Points.Num() > 0 ? R.Points.Last().BornT : -1e9f;
			if (!R.bEmitting && Newest < Oldest)
			{
				Oldest = Newest;
				Pick = I;
			}
		}
	}
	if (Pick < 0)
	{
		// Every ribbon still being laid: a missile's, a rocket's or a wreck's smoke takes the contrail farthest from the
		// camera (a raid of forty aircraft in contrail air held them all and our own missile flew without a trail), and so
		// does a contrail much nearer than that one (a far raid left the aircraft overhead without one: SimFxLogic::
		// RibbonTakeOver). That contrail stops there; its aircraft asks again next frame.
		const bool bContrail = (Owner & SimFxContrailKeyBit) != 0;
		const double MyDistM = DistToCamM(At);
		double FarthestM = -1.0;
		int32 Farthest = -1;
		for (int32 I = 0; I < Ribbons.Num() && I < Limit; ++I)
		{
			const FSimFxRibbon& R = Ribbons[I];
			if ((R.Owner & SimFxContrailKeyBit) == 0 || R.Points.Num() == 0)
			{
				continue;
			}
			const double D = DistToCamM(R.Points.Last().Pos);
			if (D > FarthestM)
			{
				FarthestM = D;
				Farthest = I;
			}
		}
		if (Farthest >= 0 && SimFxLogic::RibbonTakeOver(bContrail, MyDistM, true, FarthestM, 1.5))
		{
			Pick = Farthest;
		}
	}
	if (Pick < 0)
	{
		return -1;
	}
	FSimFxRibbon& R = Ribbons[Pick];
	R.bUsed = true;
	R.bEmitting = true;
	R.Owner = Owner;
	R.Points.Reset();
	R.Style = Style;
	R.Colour = Style.Colour;
	R.LifeS = FMath::Max(Style.LifeS, 0.5f);
	R.SpreadCmps = Style.SpreadCmps;
	R.Drift = Drift;
	// Each trail its own stretch of the texture and its own eddies (two horizontal directions, smooth along it).
	R.UOffset = static_cast<float>(Rand01());
	const double A1 = RandRange(0.0, 2.0 * UE_DOUBLE_PI), A2 = RandRange(0.0, 2.0 * UE_DOUBLE_PI);
	R.TurbA = FVector(FMath::Cos(A1), FMath::Sin(A1), 0.15 * RandRange(-1.0, 1.0));
	R.TurbB = FVector(FMath::Cos(A2), FMath::Sin(A2), 0.15 * RandRange(-1.0, 1.0));
	R.TurbPhase = static_cast<float>(RandRange(0.0, 2.0 * UE_DOUBLE_PI));
	R.BuiltFrame = -100;
	return Pick;
}

void FSimFxWorld::AddRibbonPoint(int32 Ribbon, const FVector& Pos, float WidthCm, float Opacity, const FVector& Drift, const FVector& Side)
{
	if (!Ribbons.IsValidIndex(Ribbon))
	{
		return;
	}
	FSimFxRibbon& R = Ribbons[Ribbon];
	FSimFxRibbonPoint P;
	P.Pos = Pos;
	P.BornT = RibbonTime();
	P.Width0 = WidthCm;
	P.Opacity0 = Opacity;
	P.Side = Side;
	if (R.Points.Num() > 0)
	{
		const FSimFxRibbonPoint& Prev = R.Points.Last();
		P.DistM = Prev.DistM + static_cast<float>(FVector::Dist(Pos, Prev.Pos) / 100.0);
	}
	const FSimFxRibbonStyle& St = R.Style;
	// Uneven widening, smooth along the trail (two waves of its own phase): it swells and pinches into puffs (LOOK 9.3)
	// rather than stepping from point to point.
	const float Wave = FMath::Max(St.WaveM, 1.0f);
	P.SpreadK = 1.0f + St.SpreadJitter * (0.6f * FMath::Sin(2.0f * UE_PI * P.DistM / Wave + 2.3f * R.TurbPhase) +
		0.4f * FMath::Sin(2.0f * UE_PI * P.DistM / (0.55f * Wave) + 4.1f * R.TurbPhase));
	// Eddies: a drift across the trail that changes smoothly along it, so an old trail meanders and breaks instead of
	// staying a ruled line (LOOK 9.1, 9.3).
	const float Turb1 = FMath::Sin(2.0f * UE_PI * P.DistM / Wave + R.TurbPhase);
	const float Turb2 = FMath::Sin(2.0f * UE_PI * P.DistM / (0.6f * Wave) + 1.7f * R.TurbPhase);
	P.Drift = Drift + (R.TurbA * Turb1 + R.TurbB * Turb2) * (0.7f * St.TurbCmps);
	R.Points.Add(P);
	if (R.Points.Num() > 256)
	{
		SimFxLogic::DecimateKeepingEnds(R.Points, 256);
	}
}

void FSimFxWorld::ReleaseRibbon(int32 Ribbon, uint64 Owner)
{
	// Only the owner's: a contrail's ribbon taken over by a missile's or a wreck's smoke (AcquireRibbon) is still named by
	// its aircraft's stale index, and that aircraft's release must not stop the new owner's trail.
	if (Ribbons.IsValidIndex(Ribbon) && Ribbons[Ribbon].bUsed && Ribbons[Ribbon].Owner == Owner)
	{
		Ribbons[Ribbon].bEmitting = false;
	}
}

void FSimFxWorld::TrailPoint(int32& Ribbon, uint64 Key, FVector& Last, bool& bHave, const FVector& At, float WidthCm, float Opacity, double PointM,
	const FSimFxRibbonStyle& Style, const FVector& PointDrift, const FVector& Side)
{
	if (Ribbon >= 0 && (!Ribbons.IsValidIndex(Ribbon) || !Ribbons[Ribbon].bUsed || Ribbons[Ribbon].Owner != Key))
	{
		Ribbon = -1;  // taken over by another
		bHave = false;
	}
	if (Ribbon < 0)
	{
		Ribbon = AcquireRibbon(Key, Style, FVector::ZeroVector, At);
		bHave = false;
		if (Ribbon < 0)
		{
			return;
		}
	}
	FSimFxRibbon& R = Ribbons[Ribbon];
	R.bEmitting = true;
	if (!bHave || R.Points.Num() == 0)
	{
		AddRibbonPoint(Ribbon, At, WidthCm, Opacity, PointDrift, Side);
		AddRibbonPoint(Ribbon, At, WidthCm, Opacity, PointDrift, Side);  // the head, which follows the source
		Last = At;
		bHave = true;
		return;
	}
	FSimFxRibbonPoint& Head = R.Points.Last();
	Head.Pos = At;
	Head.Width0 = WidthCm;
	Head.Opacity0 = Opacity;
	Head.BornT = RibbonTime();
	Head.Side = Side;
	if (R.Points.Num() >= 2)
	{
		const FSimFxRibbonPoint& Prev = R.Points[R.Points.Num() - 2];
		Head.DistM = Prev.DistM + static_cast<float>(FVector::Dist(At, Prev.Pos) / 100.0);
	}
	if (FVector::DistSquared(At, Last) >= FMath::Square(PointM * 100.0))
	{
		AddRibbonPoint(Ribbon, At, WidthCm, Opacity, PointDrift, Side);
		Last = At;
	}
}

FSimFxRibbonStyle FSimFxWorld::MissileTrailStyle(double LifeS) const
{
	const FSimFxTuning::FMissile& T = Tune.Missile;
	FSimFxRibbonStyle S;
	S.Colour = T.TrailColour;
	S.LifeS = static_cast<float>(LifeS);
	S.SpreadCmps = static_cast<float>(T.TrailSpreadMps * 100.0);
	S.SpreadJitter = static_cast<float>(T.TrailSpreadJitter);
	S.TurbCmps = static_cast<float>(T.TrailTurbMps * 100.0);
	S.TileM = static_cast<float>(T.TrailTileM);
	S.WaveM = static_cast<float>(T.TrailWaveM);
	S.ThinS = static_cast<float>(T.TrailThinS);
	S.ThinShare = static_cast<float>(T.TrailThinShare);
	S.ErodeFrom = static_cast<float>(T.TrailErodeFrom);
	S.bPuffsNear = true;
	return S;
}

FSimFxRibbonStyle FSimFxWorld::ContrailStyle(double LifeS, double SpreadMps, double SpanM) const
{
	const FSimFxTuning::FContrail& C = Tune.Contrail;
	FSimFxRibbonStyle S;
	S.Colour = C.Colour;
	S.LifeS = static_cast<float>(LifeS);
	S.SpreadCmps = static_cast<float>(SpreadMps * 100.0);
	S.SpreadJitter = 0.25f;
	S.TurbCmps = static_cast<float>(C.TurbMps * 100.0);
	S.TileM = static_cast<float>(C.TileM);
	S.WaveM = static_cast<float>(C.WaveM);
	S.ErodeFrom = static_cast<float>(C.ErodeFrom);
	S.RollS = static_cast<float>(C.RollS);
	S.SinkCmps = static_cast<float>(C.SinkMps * 100.0);
	S.SinkMaxCm = static_cast<float>(C.SinkMaxM * 100.0);
	S.CrowPeriodM = static_cast<float>(C.CrowSpans * SpanM);
	S.CrowFromS = static_cast<float>(C.CrowFromS);
	S.CrowFullS = static_cast<float>(C.CrowFullS);
	return S;
}

FSimFxRibbonStyle FSimFxWorld::WreckTrailStyle() const
{
	const FSimFxTuning::FWreck& T = Tune.Wreck;
	FSimFxRibbonStyle S;
	S.Colour = T.SmokeColour;
	S.LifeS = static_cast<float>(T.SmokeS);
	S.SpreadCmps = static_cast<float>(T.SmokeSpreadMps * 100.0);
	S.SpreadJitter = 0.4f;
	S.TurbCmps = 50.0f;
	S.TileM = 80.0f;
	S.WaveM = 240.0f;
	S.ErodeFrom = 0.5f;
	return S;
}

void FSimFxWorld::AddFlashLight(const FVector& Pos, const FLinearColor& Colour, double Lumens, double RadiusCm, double DurationS, double StartT)
{
	if (Lumens * Tune.Lighting.LightScale < Tune.Lighting.MinLightLumens)
	{
		return;
	}
	FSimFxFlashLight L;
	L.Pos = Pos;
	L.Colour = Colour;
	L.Lumens = Lumens;
	L.RadiusCm = RadiusCm;
	L.T = StartT;
	L.DurationS = FMath::Max(DurationS, 0.05);
	FlashLights.Add(L);
}

void FSimFxWorld::AddLight(const FVector& Pos, const FLinearColor& Colour, double Lumens, double RadiusCm)
{
	const double Scaled = Lumens * Tune.Lighting.LightScale;
	if (Scaled < Tune.Lighting.MinLightLumens)
	{
		return;
	}
	FSimFxLightDraw D;
	D.Pos = Pos;
	D.Colour = Colour;
	D.Lumens = Scaled;
	D.RadiusCm = RadiusCm;
	D.Score = Scaled / FMath::Max(FVector::DistSquared(Pos, CamPos), 1.0e4);
	Lights.Add(D);
}

const TArray<FVector>& FSimFxWorld::NozzlesOf(const FString& Model)
{
	if (const TArray<FVector>* Found = NozzleCache.Find(Model))
	{
		return *Found;
	}
	TArray<FVector> Out;
	const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
	const simcore::scenery::ModelDef* M = Lib != nullptr ? Lib->Find(std::string(TCHAR_TO_UTF8(*Model))) : nullptr;
	if (M != nullptr && M->archetype == "jet")
	{
		// The engines are the cylinders along the body after the fuselage (simcore/scenery/basic_models.hpp: the jet's
		// nozzles at the tail, or an airliner's pods under the wings); the nozzle is each one's back end.
		for (size_t I = 1; I < M->parts.size(); ++I)
		{
			const simcore::scenery::Part& P = M->parts[I];
			if (P.shape == simcore::scenery::Shape::Cylinder && P.axis == 'x' && P.size.z < 0.2 * M->length_m)
			{
				Out.Add(FVector((P.at.x - 0.5 * P.size.z) * 100.0, P.at.y * 100.0, P.at.z * 100.0));
			}
		}
	}
	else if (M != nullptr && M->archetype == "prop")
	{
		// Turboprops: the nacelles on the wings, at a sixth of the span out each side.
		for (const double Side : {-1.0, 1.0})
		{
			Out.Add(FVector(-0.1 * M->length_m * 100.0, Side * M->span_m * 0.17 * 100.0, 0.0));
		}
	}
	return NozzleCache.Add(Model, Out);
}

// ------------------------------------------------------------------ the frame

void FSimFxWorld::ClearAll()
{
	Particles.Clear();
	Bursts.Reset();
	RecentGround.Reset();
	FlashLights.Reset();
	Missiles.Reset();
	Bombs.Reset();
	GunRuns.Reset();
	GunPasses.Reset();
	Wrecks.Clear();
	WreckFx.Reset();
	Fires.Reset();
	Aircraft.Reset();
	EndedMissiles.Reset();
	for (FSimFxRibbon& R : Ribbons)
	{
		R.bUsed = false;
		R.bEmitting = false;
		R.Points.Reset();
	}
	NewDecals.Reset();
}

void FSimFxWorld::UpdateEffects()
{
	// Ribbons: points past their life go; a finished ribbon with nothing left is free again.
	const float RT = RibbonTime();
	for (FSimFxRibbon& R : Ribbons)
	{
		if (!R.bUsed)
		{
			continue;
		}
		int32 Drop = 0;
		while (Drop < R.Points.Num() - 1 && RT - R.Points[Drop].BornT > R.LifeS)
		{
			++Drop;
		}
		if (Drop > 0)
		{
			R.Points.RemoveAt(0, Drop, EAllowShrinking::No);
		}
		if (!R.bEmitting && (R.Points.Num() < 2 || RT - R.Points.Last().BornT > R.LifeS))
		{
			R.bUsed = false;
			R.Points.Reset();
		}
	}

	UpdateBursts();
	UpdateMissiles();
	UpdateBombs();
	UpdateGunRuns();
	UpdateGunPasses();
	UpdateWrecks();
	UpdateFires();
	UpdateAircraft();

	// Flash lights fade as the square of their remaining time.
	for (int32 I = FlashLights.Num() - 1; I >= 0; --I)
	{
		const FSimFxFlashLight& L = FlashLights[I];
		const double U = (Now - L.T) / L.DurationS;
		if (U >= 1.0 || U < -1.0)
		{
			FlashLights.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		const double K = FMath::Square(1.0 - FMath::Clamp(U, 0.0, 1.0));
		// White for its first frame or two, then cooling to the afterglow's orange (Gross's curve, LOOK 0.1; the second
		// art review's item 4: one colour for its whole life).
		const FLinearColor Colour = SimFxLogic::FlashLightColour(L.Colour, Tune.Explosion.LightEmberColour, Now - L.T, L.DurationS, Tune.Explosion.LightWhiteS);
		AddLight(L.Pos, Colour, L.Lumens * K, L.RadiusCm);
	}

	// The particles: moved, then drawn after the effects' own sprites, as the budget allows.
	Particles.SetMax(Budgets.MaxSprites);
	{
		// Under time compression a frame's step can be seconds: integrated in slices of at most a quarter second (debris
		// and shells fall on their arcs, not one long straight step).
		const int32 Slices = SimFxLogic::SubSteps(Dt, 0.25, 8);
		for (int32 I = 0; I < Slices; ++I)
		{
			Particles.Update(Dt / static_cast<float>(Slices));
		}
	}
	for (int32 Layer = 0; Layer < SimFxLayerCount; ++Layer)
	{
		for (const FSimFxParticle& P : Particles.Layer(static_cast<ESimFxLayer>(Layer)))
		{
			if (SpriteCount >= Budgets.MaxSprites)
			{
				break;
			}
			if (!P.IsWaiting())
			{
				AddSprite(P.ToSprite());
			}
		}
	}
}

// ------------------------------------------------------------------ events

void FSimFxWorld::OnEvent(const simcore::fx::Event& E)
{
	using simcore::fx::EventKind;
	using simcore::fx::BurstEnd;
	switch (E.kind)
	{
	case EventKind::MissileLaunch:
		LaunchMissileFx(E);
		break;
	case EventKind::MissileEnd:
	{
		const int32 EntityId = SimFxIds::MissileBase + E.number;
		EndedMissiles.Add(EntityId);
		FSimFxMissile* M = nullptr;
		for (FSimFxMissile& Each : Missiles)
		{
			if (Each.EntityId == EntityId && !Each.bDone)
			{
				M = &Each;
				break;
			}
		}
		const FSimFxMunition& Mun = Munition(E.weapon.empty() ? FString(TEXT("9m33")) : FString(UTF8_TO_TCHAR(E.weapon.c_str())));
		// The event's charge when it gives one (fx/events.hpp: 0 is munitions.json's), as the audio takes it.
		const double Charge = E.charge_kg > 0.0 ? E.charge_kg : (Mun.ChargeKg > 0.0 ? Mun.ChargeKg : Tune.Missile.DefaultChargeKg);
		const bool bFliesOn = (E.end == BurstEnd::Wide || E.end == BurstEnd::Early) && E.end_t_s > E.t_s + 0.05;
		if (!bFliesOn)
		{
			AddBurst(EFxBurstStyle::Warhead, E.aim, Charge, E.t_s, M != nullptr && M->Vel.LengthSquared() > 1.0 ? M->Vel : E.velocity);
			if (M != nullptr)
			{
				M->bDone = true;
				ReleaseRibbon(M->Ribbon, M->Key);
			}
			break;
		}
		// It flies on to destroy itself: the effects fly it on from where it is drawn (or the event's state) to where it
		// bursts.
		bool bFromDrawn = false;
		if (M == nullptr)
		{
			FSimFxMissile& New = Missiles.AddDefaulted_GetRef();
			New.Number = E.number;
			New.EntityId = EntityId;
			New.Key = NextKey++;
			New.Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
			New.Mun = &Mun;
			New.LaunchT = E.t_s;
			M = &New;
		}
		else
		{
			bFromDrawn = M->Mode == FSimFxMissile::EMode::Entity && Now - M->SeenT < 0.5 && M->Vel.LengthSquared() > 1.0;
		}
		M->Mode = FSimFxMissile::EMode::Flown;
		M->P0 = bFromDrawn ? M->Pos : E.position;
		M->V0 = bFromDrawn ? M->Vel : E.velocity;
		M->Pos = M->P0;
		M->Vel = M->V0;
		M->Aim = E.aim;
		M->T0 = bFromDrawn ? FMath::Min(Now, E.end_t_s - 0.1) : E.t_s;
		M->EndT = E.end_t_s;
		M->TargetId = -1;
		M->TargetName.Reset();
		M->bWaitForEnd = false;
		M->bBurstAtEnd = true;
		M->BurstStyle = EFxBurstStyle::Warhead;
		M->ChargeKg = Charge;
		M->bShowMesh = true;
		M->SpeedMps = FMath::Max(E.velocity.Length(), 300.0);
		break;
	}
	case EventKind::SamLaunch:
		FlownMissile(E, false);
		break;
	case EventKind::ArmLaunch:
		FlownMissile(E, true);
		break;
	case EventKind::SamEnd:
	case EventKind::ArmBurst:
	{
		const bool bArm = E.kind == EventKind::ArmBurst;
		const FString Source = UTF8_TO_TCHAR(E.source.c_str());
		const FString Target = UTF8_TO_TCHAR(E.target.c_str());
		// The missile this ends: the same kind from the same source (at the same target, the same round), the oldest
		// in flight first.
		FSimFxMissile* Best = nullptr;
		for (FSimFxMissile& M : Missiles)
		{
			if (M.bDone || M.Mode != FSimFxMissile::EMode::Flown || !M.bWaitForEnd || M.bArm != bArm)
			{
				continue;
			}
			if (!Source.IsEmpty() && !M.Source.IsEmpty() && M.Source != Source)
			{
				continue;
			}
			if (!bArm && !Target.IsEmpty() && !M.TargetName.IsEmpty() && M.TargetName != Target)
			{
				continue;
			}
			if (!bArm && E.number > 0 && M.Number > 0 && M.Number != E.number)
			{
				continue;
			}
			if (Best == nullptr || M.LaunchT < Best->LaunchT)
			{
				Best = &M;
			}
		}
		const FSimFxMunition& Mun = Munition(UTF8_TO_TCHAR(E.weapon.c_str()));
		const double Charge = E.charge_kg > 0.0 ? E.charge_kg : (Mun.ChargeKg > 0.0 ? Mun.ChargeKg : Tune.Missile.DefaultChargeKg);
		const Vec3 Aim = bArm ? E.position : E.aim;
		Vec3 At = Aim;
		Vec3 Vel = E.velocity;
		FString Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
		if (Best != nullptr)
		{
			if ((Best->Pos - Aim).Length() < Tune.Missile.BurstAtMissileM)
			{
				At = Best->Pos;
			}
			if (Best->Vel.LengthSquared() > 1.0)
			{
				Vel = Best->Vel;
			}
			if (Best->Mun != nullptr && !Best->Mun->Id.IsEmpty())
			{
				Weapon = Best->Mun->Id;
			}
			Best->bDone = true;
			ReleaseRibbon(Best->Ribbon, Best->Key);
		}
		if (bArm)
		{
			// Several metres over the antenna (LOOK 8), its fragments flying on along its path.
			const Vec3 Ground = GroundUnder(At);
			At.z = FMath::Max(At.z, Ground.z + Tune.Explosion.ArmBurstUpM);
			FSimFxBurst& B = AddBurst(EFxBurstStyle::Arm, At, Charge, E.t_s, Vel);
			B.bWp = MunitionListed(Tune.Explosion.WpMunitions, Mun.Id.IsEmpty() ? Weapon : Mun.Id) || MunitionListed(Tune.Explosion.WpMunitions, Weapon);
			break;
		}
		const bool bGround = (At - GroundUnder(At)).z < 30.0;  // height over the ground under it, both ENU
		AddBurst(bGround ? EFxBurstStyle::Ground : EFxBurstStyle::Warhead, At, Charge, E.t_s, Vel);
		break;
	}
	case EventKind::WeaponRelease:
		Release(E);
		break;
	case EventKind::WeaponImpact:
	{
		// The bombs this effects layer flies burst by themselves; an impact with none in flight bursts here.
		const FString Source = UTF8_TO_TCHAR(E.source.c_str());
		const FString Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
		bool bFlown = false;
		for (const FSimFxBomb& B : Bombs)
		{
			if (B.Source == Source && B.Weapon == Weapon && FMath::Abs(B.ImpactT - E.t_s) < 10.0)
			{
				bFlown = true;
				break;
			}
		}
		for (const FSimFxGunRun& G : GunRuns)
		{
			if (FMath::Abs(G.ImpactStartT - E.t_s) < 10.0 && G.SourceId == E.world_id)
			{
				bFlown = true;
			}
		}
		if (!bFlown)
		{
			const FSimFxMunition& Mun = Munition(Weapon);
			const double Charge = E.charge_kg > 0.0 ? E.charge_kg : FMath::Max(Mun.ChargeKg, 50.0);
			const int32 Count = FMath::Clamp(E.number, 1, Tune.Bombs.MaxPerRelease);
			for (int32 I = 0; I < Count; ++I)
			{
				const Vec3 Off{RandRange(-1.0, 1.0) * 15.0 * I, RandRange(-1.0, 1.0) * 15.0 * I, 0.0};
				FSimFxBurst& B = AddBurst(Mun.IsRound() ? EFxBurstStyle::Small : EFxBurstStyle::Ground, GroundUnder(E.position + Off), Charge, E.t_s + 0.08 * I);
				B.bSoot = MunitionListed(Tune.Explosion.SootMunitions, Mun.Id.IsEmpty() ? Weapon : Mun.Id);
			}
			if (E.fire)
			{
				FSimFxFire& F = Fires.AddDefaulted_GetRef();
				F.Pos = ToU(GroundUnder(E.position));
				F.Wind = WindAt(E.position);
				F.StartT = E.t_s;
				F.FireEndT = E.t_s + Tune.Wreck.GroundFireS;
				F.SmokeEndT = E.t_s + Tune.Wreck.ColumnS;
				F.SizeM = Tune.Wreck.FireM;
			}
		}
		break;
	}
	case EventKind::CruiseImpact:
	{
		const FSimFxMunition& Mun = Munition(UTF8_TO_TCHAR(E.weapon.c_str()));
		const double Charge = E.charge_kg > 0.0 ? E.charge_kg : FMath::Max(Mun.ChargeKg, 300.0);
		const Vec3 At = GroundUnder(E.aim.LengthSquared() > 0.0 ? E.aim : E.position);
		FSimFxBurst& B = AddBurst(EFxBurstStyle::Ground, At, Charge, E.t_s, E.velocity);
		B.bCruise = true;
		B.bSoot = MunitionListed(Tune.Explosion.SootMunitions, Mun.Id.IsEmpty() ? FString(UTF8_TO_TCHAR(E.weapon.c_str())) : Mun.Id);
		FSimFxFire& F = Fires.AddDefaulted_GetRef();
		F.Pos = ToU(At);
		F.Wind = WindAt(At);
		F.StartT = E.t_s;
		F.FireEndT = E.t_s + Tune.Wreck.GroundFireS;
		F.SmokeEndT = E.t_s + Tune.Wreck.ColumnS;
		F.SizeM = Tune.Wreck.FireM * 1.5;
		break;
	}
	case EventKind::AircraftDown:
		AircraftDown(E);
		break;
	case EventKind::GunPass:
		StartGunPass(E);
		break;
	}
}

void FSimFxWorld::LaunchMissileFx(const simcore::fx::Event& E)
{
	const FString Weapon = E.weapon.empty() ? FString(TEXT("9m33")) : FString(UTF8_TO_TCHAR(E.weapon.c_str()));
	const int32 EntityId = SimFxIds::MissileBase + E.number;
	FSimFxMissile* M = nullptr;
	for (FSimFxMissile& Each : Missiles)
	{
		if (Each.EntityId == EntityId && Each.Mode == FSimFxMissile::EMode::Entity && !Each.bDone)
		{
			M = &Each;
		}
	}
	if (M == nullptr)
	{
		M = &Missiles.AddDefaulted_GetRef();
		M->Key = NextKey++;
	}
	EndedMissiles.Remove(EntityId);  // a number used again (a new engagement)
	M->Mode = FSimFxMissile::EMode::Entity;
	M->Number = E.number;
	M->EntityId = EntityId;
	M->Source = TEXT("own");
	M->Weapon = Weapon;
	M->Mun = &Munition(Weapon);
	M->LaunchT = E.t_s;
	M->SeenT = Now;
	// The launch's flash and cloud at the launcher (PlayBurst's launch branch; the style only sets its flash's colour).
	FSimFxBurst& B = Bursts.AddDefaulted_GetRef();
	B.Style = EFxBurstStyle::Ground;
	B.Enu = E.position;
	B.Pos = ToU(E.position);
	B.Wind = WindAt(E.position);
	B.T = E.t_s;
	B.ChargeKg = 0.0;
	B.Look = simcore::fx::LookFor(1.0, true, true);
	B.Look.smoke_m = Tune.Missile.LaunchCloudM * Tune.Missile.LaunchCloudGrow * 0.5;
	B.Look.smoke_s = Tune.Missile.LaunchCloudS;
	B.Look.light_lumens = 2.0e7;
	B.Look.fireball_m = Tune.Missile.LaunchFlashM * 0.5;
	B.Look.fireball_s = Tune.Missile.LaunchFlashS;
	B.Look.crater_m = 0.0;
	B.Seed = static_cast<uint32>(E.number) * 7919u + 13u;
	B.bLaunch = true;
	const Vec3 Dir = E.velocity.Normalized();
	B.Dir = (ToU(E.position + Dir) - B.Pos).GetSafeNormal();
}

void FSimFxWorld::FlownMissile(const simcore::fx::Event& E, bool bArm)
{
	FSimFxMissile& M = Missiles.AddDefaulted_GetRef();
	M.Mode = FSimFxMissile::EMode::Flown;
	M.Key = NextKey++;
	M.Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
	M.Mun = &Munition(M.Weapon.IsEmpty() ? FString(bArm ? TEXT("missile_arm") : TEXT("9m33")) : M.Weapon);
	M.Source = UTF8_TO_TCHAR(E.source.c_str());
	M.Number = E.number;
	M.bArm = bArm;
	M.LaunchT = E.t_s;
	M.T0 = E.t_s;
	M.P0 = E.position;
	M.Pos = E.position;
	M.Aim = E.aim;
	M.TargetId = bArm ? -1 : E.world_id;
	M.TargetName = bArm ? FString() : FString(UTF8_TO_TCHAR(E.target.c_str()));
	M.SpeedMps = bArm ? Tune.Missile.ArmSpeedMps : Tune.Missile.SamSpeedMps;
	const double Dist = (E.aim - E.position).Length();
	M.EndT = E.end_t_s > E.t_s + 0.5 ? E.end_t_s : E.t_s + Dist / M.SpeedMps;
	Vec3 Dir = E.velocity.Normalized();
	if (Dir.LengthSquared() < 0.5)
	{
		Dir = (E.aim - E.position + Vec3{0.0, 0.0, 0.3 * Dist}).Normalized();
	}
	M.V0 = Dir * (bArm ? FMath::Max(E.velocity.Length(), 200.0) : 30.0);
	M.Vel = M.V0;
	M.bWaitForEnd = true;
	M.bShowMesh = true;
	M.bLowSmoke = bArm;
	M.ChargeKg = M.Mun->ChargeKg > 0.0 ? M.Mun->ChargeKg : Tune.Missile.DefaultChargeKg;
	if (!bArm)
	{
		// A neighbour's launch: a flash and a smaller cloud at the site.
		FSimFxBurst& B = Bursts.AddDefaulted_GetRef();
		B.Enu = E.position;
		B.Pos = ToU(E.position);
		B.Wind = WindAt(E.position);
		B.T = E.t_s;
		B.Look = simcore::fx::LookFor(1.0, true, true);
		B.Look.smoke_m = Tune.Missile.LaunchCloudM * Tune.Missile.LaunchCloudGrow * 0.35;
		B.Look.smoke_s = Tune.Missile.LaunchCloudS * 0.7;
		B.Look.light_lumens = 1.5e7;
		B.Look.fireball_m = Tune.Missile.LaunchFlashM * 0.4;
		B.Look.fireball_s = Tune.Missile.LaunchFlashS;
		B.Look.crater_m = 0.0;
		B.Seed = static_cast<uint32>(M.Key) * 31u + 7u;
		B.bLaunch = true;
		// A shoulder-fired missile's ejection charge and first metres of motor: a small puff, not a launcher's cloud.
		const bool bManpads = M.Mun->Kind == TEXT("sam") && M.Mun->DiameterM > 0.0 && M.Mun->DiameterM <= Tune.Missile.HelixMaxDiameterM;
		B.LaunchScale = bManpads ? 0.15f : 0.6f;
		B.Dir = (ToU(E.position + Dir) - B.Pos).GetSafeNormal();
	}
}

void FSimFxWorld::Release(const simcore::fx::Event& E)
{
	const FString Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
	const FSimFxMunition& Mun = Munition(Weapon);
	const FString Source = UTF8_TO_TCHAR(E.source.c_str());
	if (Mun.Kind == TEXT("gun") || Mun.Kind == TEXT("shell"))
	{
		// A gun run at the ground: flashes at the aircraft, impacts walking along the ground to the aim.
		FSimFxGunRun& G = GunRuns.AddDefaulted_GetRef();
		G.SourceId = E.world_id;
		G.Mun = &Mun;
		const double Rate = FMath::Max(Mun.RateRpm > 0.0 ? Mun.RateRpm : Tune.Guns.DefaultRateRpm, 60.0) / 60.0;
		const double Rounds = FMath::Max(E.number, 1);
		const double Duration = FMath::Clamp(Rounds / Rate, 0.5, 4.0);
		G.StartT = E.t_s;
		G.FireEndT = E.t_s + Duration;
		G.ImpactStartT = FMath::Max(E.end_t_s, E.t_s + 0.3);
		G.ImpactEndT = G.ImpactStartT + Duration;
		Vec3 Track = simcore::fx::trajectory_detail::Horizontal(E.velocity).Normalized();
		if (Track.LengthSquared() < 0.5)
		{
			Track = simcore::fx::trajectory_detail::Horizontal(E.aim - E.position).Normalized();
		}
		const double Walk = Tune.Bombs.GunWalkMps > 0.0 ? Tune.Bombs.GunWalkMps * Duration
			: FMath::Clamp(E.velocity.HorizontalLength() * Duration * 0.4, 30.0, 400.0);
		G.From = GroundUnder(E.aim - Track * (0.5 * Walk));
		G.To = GroundUnder(E.aim + Track * (0.5 * Walk));
		G.Muzzle0 = E.position;
		G.MuzzleVel = E.velocity;
		G.NextImpactT = G.ImpactStartT;
		G.NextFlashT = G.StartT;
		return;
	}
	const int32 Count = FMath::Clamp(E.number, 1, FMath::Max(Tune.Bombs.MaxPerRelease, 1));
	const bool bRocket = Mun.Kind == TEXT("rocket") || Mun.Kind == TEXT("missile");
	const double Interval = bRocket ? Tune.Bombs.RocketRippleS : Tune.Bombs.RippleS;
	Vec3 Track = simcore::fx::trajectory_detail::Horizontal(E.velocity).Normalized();
	if (Track.LengthSquared() < 0.5)
	{
		Track = simcore::fx::trajectory_detail::Horizontal(E.aim - E.position).Normalized();
	}
	const Vec3 Across{Track.y, -Track.x, 0.0};
	const double Spacing = Interval * E.velocity.HorizontalLength();
	const double ImpactT = E.end_t_s > E.t_s + 0.3 ? E.end_t_s : E.t_s + 8.0;
	for (int32 I = 0; I < Count; ++I)
	{
		FSimFxBomb& B = Bombs.AddDefaulted_GetRef();
		B.Weapon = Weapon;
		B.Mun = &Mun;
		B.SourceId = I == 0 ? -1 : E.world_id;  // the first leaves from the event's own state
		B.Source = Source;
		B.ReleaseT = E.t_s + I * Interval;
		const double Along = (I - 0.5 * (Count - 1)) * Spacing;
		const double Scatter = Tune.Bombs.ImpactScatterM;
		const Vec3 Impact = E.aim + Track * Along + Track * RandRange(-Scatter, Scatter) + Across * RandRange(-Scatter, Scatter);
		B.ImpactEnu = GroundUnder(Impact);
		B.ImpactT = FMath::Max(ImpactT + (I - 0.5 * (Count - 1)) * Interval, B.ReleaseT + 0.5);
		B.FallbackPos = E.position + E.velocity * (I * Interval);
		B.FallbackVel = E.velocity;
		B.ChargeKg = E.charge_kg > 0.0 ? E.charge_kg : Mun.ChargeKg;
		B.bFire = E.fire;
		B.Key = NextKey++;
		using EKind = simcore::fx::BombPathParams;
		const bool bBlades = Mun.Kind == TEXT("retarded") || (Mun.bRetarder && Mun.RetarderType == TEXT("blades"));
		const bool bChute = Mun.Kind == TEXT("ballute") || Mun.Kind == TEXT("parachute") ||
			(Mun.bRetarder && (Mun.RetarderType == TEXT("ballute") || Mun.RetarderType == TEXT("chute")));
		if (bBlades)
		{
			B.PathKind = EKind::Retarded;
			B.Part = TEXT("blades_open");
			B.PartT = B.ReleaseT + (Mun.RetarderOpenS > 0.0 ? Mun.RetarderOpenS : Tune.Bombs.RetarderOpenS);
		}
		else if (bChute)
		{
			B.PathKind = EKind::Parachute;
			const bool bBallute = Mun.RetarderType == TEXT("ballute") || (Mun.RetarderType.IsEmpty() && Mun.Kind == TEXT("ballute"));
			B.Part = bBallute ? TEXT("ballute") : TEXT("chute");
			B.PartT = B.ReleaseT + (Mun.RetarderOpenS > 0.0 ? Mun.RetarderOpenS : 1.2);
			B.ChuteDescentMps = Mun.RetarderDescentMps;
		}
		else if (Mun.Kind == TEXT("glide"))
		{
			B.PathKind = EKind::Glide;
		}
		else if (Mun.Kind == TEXT("rocket"))
		{
			B.PathKind = EKind::Rocket;
		}
		else if (Mun.Kind == TEXT("missile") || Mun.Kind == TEXT("arm") || Mun.Kind == TEXT("cruise"))
		{
			B.PathKind = EKind::Missile;
		}
		else
		{
			B.PathKind = EKind::FreeFall;
		}
	}
}

void FSimFxWorld::StartGunPass(const simcore::fx::Event& E)
{
	FSimFxGunPass& G = GunPasses.AddDefaulted_GetRef();
	G.Site = E.position + Vec3{0.0, 0.0, 2.0};
	G.SiteU = ToU(G.Site);
	G.TargetId = E.world_id;
	G.TargetName = UTF8_TO_TCHAR(E.target.c_str());
	G.AimFallback = E.aim;
	G.T0 = E.t_s;
	G.T1 = E.end_t_s > E.t_s ? E.end_t_s : E.t_s + 6.0;
	G.Weapon = UTF8_TO_TCHAR(E.weapon.c_str());
	G.Mun = &Munition(G.Weapon);
	G.bFlak = G.Mun->CalibreMm >= 50.0;
	G.Rng = SimFxEffHash(static_cast<uint32>(E.seq) * 2654435761u + 17u);
	// The bursts by the event's seq alone, so the sound (USimFxAudioSubsystem) fires the same ones.
	// A slow gun's bursts hold a few rounds (its clips), not the Shilka's fraction of a second.
	const double RateHz = FMath::Max(G.Mun->RateRpm > 0.0 ? G.Mun->RateRpm : Tune.Guns.DefaultRateRpm, 30.0) / 60.0;
	double BurstMinS = 0.0, BurstMaxS = 0.0;
	SimFxLogic::GunBurstSpan(Tune.Guns.BurstMinS, Tune.Guns.BurstMaxS,
		SimFxLogic::NamedIntFor(Tune.Guns.BurstMinRounds, G.Mun->Id, G.Weapon, 1), RateHz, BurstMinS, BurstMaxS);
	SimFxLogic::GunBursts(static_cast<uint32>(E.seq), G.T0, G.T1, BurstMinS, BurstMaxS, Tune.Guns.PauseMinS,
		Tune.Guns.PauseMaxS, G.Bursts);
	G.BurstIndex = 0;
	G.NextShotT = G.Bursts.Num() > 0 ? G.Bursts[0].StartS : G.T1;
	G.NextBurstT = G.NextShotT;  // the burst under way began then
	G.BurstEndT = G.Bursts.Num() > 0 ? G.Bursts[0].EndS : G.T1;
}

void FSimFxWorld::AircraftDown(const simcore::fx::Event& E)
{
	AddBurst(EFxBurstStyle::Kill, E.position, Tune.Wreck.KillChargeKg, E.t_s, E.velocity);
	if (E.world_id < 0)
	{
		return;
	}
	simcore::fx::WreckFallParams P;
	P.pos = E.position;
	P.vel = E.velocity;
	P.ground_z_m = GroundUnder(E.position).z;
	P.ground_z_at = [this](double X, double Y) { return GroundUnder(simcore::Vec3{X, Y, 0.0}).z; };  // used while the fall is built
	P.seed = FSimFxWrecks::SeedFor(E.world_id);
	P.falls = E.falls;
	Wrecks.Add(E.world_id, E.t_s, P, E.fire, UTF8_TO_TCHAR(E.model.c_str()));
	FSimFxWreckFx* Fx = nullptr;
	for (FSimFxWreckFx& W : WreckFx)
	{
		if (W.Id == E.world_id)
		{
			Fx = &W;
		}
	}
	if (Fx == nullptr)
	{
		Fx = &WreckFx.AddDefaulted_GetRef();
	}
	else
	{
		// The same aircraft down again (the photographer's kill and then the sim's, or two hits in one step): the first
		// fall's smoke stops and fades rather than holding its ribbon to the next clear.
		ReleaseRibbon(Fx->Ribbon, Fx->Key);
	}
	*Fx = FSimFxWreckFx();
	Fx->Id = E.world_id;
	Fx->Key = NextKey++;
}

// ------------------------------------------------------------------ explosions

bool FSimFxWorld::MunitionListed(const TArray<FString>& List, const FString& Id)
{
	if (Id.IsEmpty())
	{
		return false;
	}
	for (const FString& Each : List)
	{
		if (!Each.IsEmpty() && Id.StartsWith(Each, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FSimFxBurst& FSimFxWorld::AddBurst(FSimFxBurst::EStyle Style, const Vec3& Enu, double ChargeKg, double T, const Vec3& VelEnu)
{
	FSimFxBurst& B = Bursts.AddDefaulted_GetRef();
	B.Style = Style;
	B.Enu = Enu;
	B.Pos = ToU(Enu);
	B.Wind = WindAt(Enu);
	B.T = T;
	B.ChargeKg = FMath::Max(ChargeKg, 0.001);
	B.Vel = VelEnu.LengthSquared() > 1e-6 ? ToU(Enu + VelEnu) - B.Pos : FVector::ZeroVector;  // the flat mapping: m/s to cm/s
	const bool bGround = Style == EFxBurstStyle::Ground || Style == EFxBurstStyle::Small || Style == EFxBurstStyle::Haze;
	B.Look = simcore::fx::LookFor(B.ChargeKg, bGround, !bGround);
	if (Style == EFxBurstStyle::Warhead || Style == EFxBurstStyle::Flak || Style == EFxBurstStyle::SdPuff)
	{
		// Thinner air, a bigger puff (LOOK 3, Sachs scaling).
		B.Look.smoke_m *= SimFxLogic::SachsScale(AltitudeOf(Enu));
	}
	else if (Style == EFxBurstStyle::Kill)
	{
		// The fuel's fireball, not the warhead's (LOOK 6): it sets the burst's size and its tier.
		const double Fuel = Tune.Wreck.KillFuelKg;
		B.Look.fireball_m = 0.5 * SimFxLogic::FuelFireballM(Fuel);
		B.Look.fireball_s = SimFxLogic::FuelFireballS(Fuel);
		B.Look.smoke_m = FMath::Max(B.Look.smoke_m, B.Look.fireball_m);
		B.Look.flash_m = FMath::Max(B.Look.flash_m, B.Look.fireball_m);
	}
	B.Seed = SimFxEffHash(static_cast<uint32>(Bursts.Num()) * 7u + static_cast<uint32>(T * 1000.0));
	return B;
}

void FSimFxWorld::UpdateBursts()
{
	const FSimFxTuning::FExplosion& X = Tune.Explosion;
	for (int32 I = Bursts.Num() - 1; I >= 0; --I)
	{
		FSimFxBurst& B = Bursts[I];
		if (B.T > Now)
		{
			continue;  // a burst still to come (a cluster's bomblets)
		}
		const double Age = Now - B.T;
		const simcore::fx::ExplosionLook& L = B.Look;
		const bool bColumn = B.Style == EFxBurstStyle::Ground && !B.bLaunch;
		if (!B.bLightDone)
		{
			B.bLightDone = true;
			const double LightS = X.LightS * FMath::Max(1.0, L.fireball_s / 0.3);
			if (Age < LightS && DistToCamM(B.Pos) < Lod.MaxRangeM && B.Style != EFxBurstStyle::Haze)
			{
				AddFlashLight(B.Pos + FVector(0.0, 0.0, 100.0 * L.fireball_m), X.LightColour, L.light_lumens,
					100.0 * L.fireball_m * X.LightRadiusPerFireballM, LightS, B.T);
			}
			if (bColumn && L.crater_m > 0.0 && DistToCamM(B.Pos) < Lod.MaxRangeM)
			{
				FSimFxDecalWant D;
				D.Pos = B.Pos;
				D.RadiusCm = 100.0 * FMath::Max(X.ScorchMinM, L.crater_m * X.ScorchPerCrater);
				D.Yaw = static_cast<float>(RandRange(0.0, 360.0));
				NewDecals.Add(D);
			}
		}
		// How long an unseen burst waits to be seen: while its smoke hangs.
		double Keep = FMath::Max(L.smoke_s, 2.0 * L.fireball_s + 0.5);
		if (bColumn)
		{
			Keep = FMath::Max(Keep, 0.5 * X.ColumnLifeS);
		}
		else if (B.Style == EFxBurstStyle::Warhead || B.Style == EFxBurstStyle::Kill)
		{
			Keep = FMath::Max(Keep, 0.5 * X.AirPuffS);
		}
		else if (B.Style == EFxBurstStyle::Flak || B.Style == EFxBurstStyle::SdPuff)
		{
			Keep = FMath::Max(Keep, 0.5 * SimFxLogic::FlakFor(X.FlakByCalibre, B.CalibreMm).HangS);
		}
		else if (B.Style == EFxBurstStyle::Haze)
		{
			Keep = Tune.Bombs.ClusterHazeS * 0.5;
		}
		if (!B.bSpawned)
		{
			// Its size for the tier: a ground burst by its column at 10 s (LOOK 2.1 "judge the tier by the column's
			// height"), a haze by its pattern, the rest by their fireball and smoke.
			double Size = 2.0 * FMath::Max(L.fireball_m, L.smoke_m);
			if (bColumn)
			{
				Size = FMath::Max(Size, SimFxLogic::ColumnTopM(B.ChargeKg, 10.0, X.ColumnScale));
			}
			else if (B.Style == EFxBurstStyle::Haze)
			{
				Size = FMath::Max(B.HazeW, B.HazeL);
			}
			else if (B.Style == EFxBurstStyle::Flak || B.Style == EFxBurstStyle::SdPuff)
			{
				Size = FMath::Max(Size, SimFxLogic::FlakFor(X.FlakByCalibre, B.CalibreMm).PuffM);
			}
			const ESimFxTier Tier = TierAt(B.Pos + FVector(0.0, 0.0, bColumn ? 50.0 * Size : 0.0), Size);
			if (Tier == ESimFxTier::Glint)
			{
				if (Age < X.FlashS + L.fireball_s)
				{
					PlayBurst(B, Tier);
				}
				B.bSpawned = true;
			}
			else if (Tier != ESimFxTier::None)
			{
				if (bColumn && Tier == ESimFxTier::Full)
				{
					// Among the ground bursts played in full lately and near it, its short-lived detail shared out (a
					// stick of bombs: the clods and dust of six are not six times one's).
					const double NearCm = 100.0 * X.CrowdRadiusM;
					int32 Neighbours = 0;
					for (int32 K = RecentGround.Num() - 1; K >= 0; --K)
					{
						const FVector4& Rg = RecentGround[K];
						if (Now - Rg.W > X.CrowdS)
						{
							RecentGround.RemoveAtSwap(K, EAllowShrinking::No);
						}
						else if (FVector::DistSquared(FVector(Rg), B.Pos) < NearCm * NearCm)
						{
							++Neighbours;
						}
					}
					B.Crowd = SimFxLogic::CrowdShare(Neighbours);
					RecentGround.Add(FVector4(B.Pos, B.T));
				}
				PlayBurst(B, Tier);
				B.bSpawned = true;
			}
		}
		if (B.bSpawned || Age > Keep)
		{
			Bursts.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
}

void FSimFxWorld::PlayBurst(FSimFxBurst& B, ESimFxTier Tier)
{
	const FSimFxTuning::FExplosion& X = Tune.Explosion;
	const simcore::fx::ExplosionLook& L = B.Look;
	const float Age = static_cast<float>(FMath::Max(Now - B.T, 0.0));
	const bool bFull = Tier == ESimFxTier::Full;
	uint32 R = B.Seed ^ 0xA511E9B3u;
	auto U = [&R]() { return SimFxEffRand(R); };
	auto PM = [&R]() { return 2.0f * SimFxEffRand(R) - 1.0f; };
	const float FireD = static_cast<float>(200.0 * L.fireball_m);
	const float SmokeD = static_cast<float>(200.0 * L.smoke_m);
	const float DustD = static_cast<float>(200.0 * L.dust_m);
	const float FlashD = static_cast<float>(200.0 * L.flash_m);
	const float Cube = static_cast<float>(FMath::Pow(B.ChargeKg / 100.0, 1.0 / 3.0));  // W^(1/3) against 100 kg
	// The fireball cools by the charge's Gross curve (LOOK 0.1, the art review's D2): held hot 0.05 s, then e-folding.
	const float FireTauS = static_cast<float>(SimFxLogic::FireballTauS(B.ChargeKg));
	const bool bAirStyle = B.Style == EFxBurstStyle::Warhead || B.Style == EFxBurstStyle::Flak || B.Style == EFxBurstStyle::SdPuff;
	const FLinearColor Soil = X.DustColour;

	// ---- the pieces every style draws from
	auto Flash = [&](const FVector& At, float Size, float Life, const FLinearColor& Tint, bool bSpiky, float MinPx)
	{
		FSimFxParticle P;
		P.Layer = ESimFxLayer::Glow;
		P.Kind = bSpiky ? ESimFxKind::Flash : ESimFxKind::Atlas;
		P.Pos = At;
		P.Age = Age;
		P.Life = Life;
		P.Size0 = Size * 0.8f;
		P.Size1 = Size;
		P.GrowS = 0.01f;
		P.FadeFrom = 0.15f;
		P.Roll = U() * 6.2832f;
		P.Tint = Tint;
		P.MinPx = MinPx;
		Spawn(P);
	};
	auto Smoke = [&](const FVector& At, float Size0, float Size1, float Life, const FLinearColor& Colour, float Opacity)
	{
		FSimFxParticle P;
		P.Layer = ESimFxLayer::Smoke;
		P.Pos = At;
		P.Wind = B.Wind;
		P.Age = Age;
		P.Life = Life;
		P.Size0 = Size0;
		P.Size1 = Size1;
		P.Opacity = Opacity;
		P.FadeFrom = 0.45f;
		P.Tint = Colour;
		P.Frame0 = FMath::Floor(U() * 15.99f);
		P.Drag = 1.0f;
		P.Roll = U() * 6.2832f;
		P.Spin = PM() * 0.15f;
		// Most of its growth in the blast's first moments, the rest slow (D7).
		P.GrowS = B.bLaunch ? 1.5f : 0.4f;
		return P;
	};
	auto FireBall = [&](const FVector& At, const FVector& Vel, float Size, float Life)
	{
		FSimFxParticle P;
		P.Layer = ESimFxLayer::Fire;
		P.Pos = At;
		P.Vel = Vel;
		P.Wind = B.Wind;
		P.Age = Age;
		P.Life = Life;
		P.Size0 = Size * 0.6f;
		P.Size1 = Size * 1.4f;
		// The recorded colour flipbook (its own soot and flame); its heat by the Gross curve.
		P.Kind = ESimFxKind::ColourFlipbook;
		P.Heat0 = 1.0f;
		P.Heat1 = 0.0f;
		P.HeatHoldS = 0.05f;
		P.HeatTauS = FireTauS;
		P.Frame0 = 0.0f;
		P.Frames = 24.99f;
		P.FadeFrom = 0.6f;
		P.GrowS = 0.05f;  // at its largest by 50 ms (LOOK 2.1)
		// The soot's albedo: the material lights it by the sun, the sky and the flashes like the smoke.
		P.Tint = FLinearColor(0.06f, 0.05f, 0.045f);
		P.Drag = 2.0f;
		P.Roll = U() * 6.2832f;
		return P;
	};
	auto Spark = [&](const FVector& At, const FVector& Vel, float Life, float Drag, float Gravity)
	{
		FSimFxParticle P;
		P.Layer = ESimFxLayer::Spark;
		P.Pos = At;
		P.Vel = Vel;
		P.Age = Age;
		P.Life = Life;
		P.Size0 = P.Size1 = 60.0f;
		P.FadeFrom = 0.3f;
		P.Tint = X.SparkColour * Brightness(X.SparkBrightness);
		P.Gravity = Gravity;
		P.Drag = Drag;
		P.MinPx = 1.0f;
		Spawn(P);
	};
	auto Sparks = [&](int32 Count, float SpeedCm, float Life, float Up)
	{
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector D(PM(), PM(), PM() + Up);
			Spark(B.Pos, D.GetSafeNormal() * SpeedCm * (0.5f + 0.5f * U()), Life * (0.5f + 0.5f * U()), 1.5f, 980.0f);
		}
	};
	// Fragments in a cone ahead along the burst's velocity (LOOK 3, 8), reaching ReachM at drag 3/s.
	auto ConeSparks = [&](const FVector& At, int32 Count, double ConeDeg, double ReachM)
	{
		const bool bHasDir = B.Vel.SizeSquared() > 1.0;
		const float Drag = 3.0f;
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector Dir = bHasDir ? SimFxLogic::DirInCone(B.Vel, FMath::DegreesToRadians(ConeDeg), U(), U()) : FVector(PM(), PM(), PM()).GetSafeNormal();
			const float Speed = static_cast<float>(ReachM * 100.0 * Drag) * (0.5f + 0.6f * U());
			Spark(At, Dir * Speed, 0.25f + 0.2f * U(), Drag, 300.0f);
		}
	};
	// A thrown piece's dust or smoke trail: puffs laid later along its path (they wait unseen until their time).
	auto Trail = [&](const FVector& P0, const FVector& V0, float Drag, float Gravity, int32 Count, float StepS, float Size, float Life,
		const FLinearColor& Colour, float Opacity)
	{
		for (int32 K = 1; K <= Count; ++K)
		{
			const float Delay = StepS * static_cast<float>(K);
			FSimFxParticle P = Smoke(SimFxLogic::BallisticAt(P0, V0, FVector::ZeroVector, Drag, Gravity, Delay), Size * 0.4f, Size, Life, Colour, Opacity);
			P.Age = Age - Delay;
			P.GrowS = 0.3f;
			P.Drag = 0.5f;
			P.FadeFrom = 0.3f;
			Spawn(P);
		}
	};

	// ---- far tiers
	if (Tier == ESimFxTier::Glint)
	{
		// At night: the flash alone, long enough to be seen as a flicker (LOOK 0.6); a haze has none.
		if (B.Style != EFxBurstStyle::Haze)
		{
			Flash(B.Pos, FlashD, static_cast<float>(X.FlashS * FMath::Max(1.0, L.fireball_s / 0.3)), X.FlashColour * Brightness(X.FlashBrightness),
				false, static_cast<float>(Tune.Lod.GlintPx));
		}
		return;
	}
	const bool bColumn = B.Style == EFxBurstStyle::Ground && !B.bLaunch;
	const double Scale = X.ColumnScale;
	const float RiseK = static_cast<float>(100.0 * SimFxLogic::ColumnRiseK(B.ChargeKg, Scale));  // cm/s^(1/2)
	const float StabS = static_cast<float>(SimFxLogic::ColumnStabS(B.ChargeKg));
	const float ColumnW = static_cast<float>(100.0 * SimFxLogic::ColumnWidthM(B.ChargeKg));
	const float Soot = B.bSoot ? static_cast<float>(X.SootDarken) : 1.0f;
	FLinearColor Low = X.ColumnLowColour * Soot;
	FLinearColor High = X.ColumnHighColour * FMath::Lerp(1.0f, Soot, 0.5f);
	if (B.bCruise)
	{
		High = FMath::Lerp(High, X.MasonryColour, 0.5f);  // a lower, paler cloud of masonry dust (LOOK 7)
	}
	// A column's member at a share F of its height: up at F K sqrt(t) until it stabilises, drifting with the wind
	// (more the higher it is), its width growing from about the fireball's to the column's (LOOK 0.3).
	auto Member = [&](float F, float WidthShare, float Life, float MinPx)
	{
		const float H = F * RiseK * FMath::Sqrt(FMath::Min(Age, StabS));
		const FVector Drift = B.Wind * (1.0f + 0.5f * F);
		const FVector At = B.Pos + FVector(PM(), PM(), 0.0f) * (0.12f * ColumnW * F) + FVector(0.0f, 0.0f, H + 0.15f * FireD) + Drift * FMath::Min(Age, 20.0f);
		const float S1 = ColumnW * WidthShare * FMath::Lerp(0.8f, 1.15f, F) * (0.9f + 0.25f * U());
		FSimFxParticle P = Smoke(At, FMath::Min(0.6f * FireD, S1), S1, Life, FMath::Lerp(Low, High, FMath::Pow(F, 0.8f)),
			static_cast<float>(FMath::Lerp(X.ColumnLowOpacity, X.ColumnHighOpacity, F)));
		P.RiseK = F * RiseK;
		P.RiseStopS = StabS;
		P.GrowS = 6.0f;  // the width: about the fireball's at 1 s, twice it by 3 s, the column's by 10-20 s (LOOK 0.3)
		P.Drag = 0.3f;
		P.Wind = Drift;
		P.FadeFrom = 0.4f;
		P.FadeInS = 0.05f;
		P.Spin = PM() * 0.02f;
		P.MinPx = MinPx;
		Spawn(P);
	};

	if (Tier == ESimFxTier::Speck)
	{
		// Far by day: grey specks at the cloud's true size, never under the speck's pixels (dimmed by their true size
		// instead, D8), staying as long as the smoke (LOOK T12, 3). A ground burst: its column as two.
		const float Px = static_cast<float>(Tune.Lod.SpeckPx);
		if (bColumn)
		{
			Member(0.3f, 1.2f, static_cast<float>(X.ColumnLifeS), Px);
			Member(0.8f, 1.2f, static_cast<float>(X.ColumnLifeS), Px);
			return;
		}
		const bool bAir = bAirStyle || B.Style == EFxBurstStyle::Kill || B.Style == EFxBurstStyle::Arm;
		FLinearColor Colour = B.bLaunch ? Tune.Missile.LaunchCloudColour : (bAir ? X.AirSmokeColour : Soil);
		float D = B.bLaunch ? static_cast<float>(Tune.Missile.LaunchCloudM * 100.0 * Tune.Missile.LaunchCloudGrow) * B.LaunchScale
			: FMath::Max(SmokeD, bAir ? 0.0f : DustD);
		float Life = static_cast<float>(B.bLaunch ? Tune.Missile.LaunchCloudS : (bAir ? X.AirPuffS : L.smoke_s));
		if (B.Style == EFxBurstStyle::Flak || B.Style == EFxBurstStyle::SdPuff)
		{
			const SimFxLogic::FFlakRow Row = SimFxLogic::FlakFor(X.FlakByCalibre, B.CalibreMm);
			Colour = FMath::Lerp(X.FlakLightColour, X.FlakBlackColour, static_cast<float>(Row.Black));
			D = static_cast<float>(Row.PuffM * 100.0);
			Life = static_cast<float>(Row.HangS);
		}
		else if (B.Style == EFxBurstStyle::Haze)
		{
			Colour = Tune.Bombs.ClusterHazeColour;
			D = 100.0f * FMath::Max(B.HazeW, B.HazeL) * 0.5f;
			Life = static_cast<float>(Tune.Bombs.ClusterHazeS);
		}
		FSimFxParticle P = Smoke(B.Pos + FVector(0.0f, 0.0f, bAir ? 0.0f : 0.3f * D), 0.35f * D, D, Life, Colour, static_cast<float>(X.SmokeOpacity));
		P.MinPx = Px;
		Spawn(P);
		return;
	}

	if (B.bLaunch)
	{
		// The launch's flash (the style only sets its colour), and the cloud rolling out low round the launcher, mostly
		// behind it.
		const FSimFxTuning::FMissile& T = Tune.Missile;
		Flash(B.Pos, FlashD, static_cast<float>(T.LaunchFlashS), X.FlashColour * Brightness(X.FlashBrightness), false, 0.0f);
		const float LScale = B.LaunchScale;
		const int32 Puffs = bFull ? T.LaunchCloudPuffs : FMath::Max(T.LaunchCloudPuffsImpostor, 1);
		FVector Back = -FVector(B.Dir.X, B.Dir.Y, 0.0).GetSafeNormal();
		if (Back.IsNearlyZero())
		{
			Back = FVector(-1.0, 0.0, 0.0);
		}
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Back);
		const float Spread = static_cast<float>(100.0 * T.LaunchCloudSpreadM) * LScale;
		for (int32 I = 0; I < Puffs; ++I)
		{
			// Most of it blown out behind and to the sides, bunched near the launcher (F = U^0.6) and spreading as it goes;
			// a third of it rolling round the launcher itself. Each puff its own size, sitting on the ground.
			const float Size = static_cast<float>(T.LaunchCloudM * 100.0) * LScale * (bFull ? 1.0f : 2.0f) * (0.6f + 0.8f * U());
			FVector Off, Out;
			if (I % 3 == 2)
			{
				const float A = 6.2832f * U();
				Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.0f);
				Off = Out * (0.5f * Spread * U());
			}
			else
			{
				const float F = FMath::Pow(U(), 0.6f);
				Off = Back * (F * 2500.0f * LScale) + Across * (PM() * Spread * (0.4f + 0.6f * F));
				Out = Off.GetSafeNormal2D();
			}
			Off.Z = 0.4f * Size;
			const FVector Vel = Out * (300.0f + 700.0f * U()) * LScale + Back * (300.0f * U()) * LScale + FVector(0.0f, 0.0f, 40.0f * U());
			FSimFxParticle P = Smoke(B.Pos + Off, Size, Size * static_cast<float>(T.LaunchCloudGrow), static_cast<float>(T.LaunchCloudS) * (0.7f + 0.5f * U()),
				T.LaunchCloudColour, static_cast<float>(T.LaunchCloudOpacity));
			P.Vel = Vel;
			P.Buoyancy = 8.0f;
			P.Drag = 0.6f;
			P.FadeInS = 0.05f;
			P.Spin = PM() * 0.25f;  // boiling
			Spawn(P);
		}
		return;
	}

	// The detonation's flash: 1-2 frames of a spiky star 2-3 x the fireball (LOOK 2.1, 3), and for a far burst a softer
	// one that lasts long enough to be seen.
	const FLinearColor FlashTint = (bAirStyle || B.Style == EFxBurstStyle::Arm ? FLinearColor(1.0f, 0.9f, 0.75f) : X.FlashColour) *
		Brightness(X.FlashBrightness);
	if (B.Style != EFxBurstStyle::Haze)
	{
		const float Spike = FMath::Max(FireD * static_cast<float>(X.SpikeFireballs), 150.0f);
		Flash(B.Pos, Spike, static_cast<float>(X.SpikeS), FlashTint, true, 0.0f);
		if (!bFull)
		{
			Flash(B.Pos, FlashD, static_cast<float>(X.FlashS), FlashTint, false, 0.0f);
		}
	}

	switch (B.Style)
	{
	case EFxBurstStyle::Warhead:
	{
		// A SAM's warhead (LOOK 3): a small fireball, an irregular dark puff of uneven parts grown within a second and
		// hanging for a minute or two, greying; fragments in a cone ahead along the missile's way.
		const int32 FireCount = bFull ? 3 : 1;
		for (int32 I = 0; I < FireCount; ++I)
		{
			const FVector Off = bFull ? FVector(PM(), PM(), PM()) * FireD * 0.25f : FVector::ZeroVector;
			FSimFxParticle P = FireBall(B.Pos + Off, Off * 2.0f + B.Vel * 0.1f, FireD * (bFull ? 0.8f + 0.3f * U() : 1.3f),
				static_cast<float>(L.fireball_s) * (1.2f + 0.4f * U()));
			Spawn(P);
		}
		const int32 Puffs = bFull ? 3 + static_cast<int32>(U() * 2.99f) : 2;
		for (int32 I = 0; I < Puffs; ++I)
		{
			const FVector Off = FVector(PM(), PM(), PM()) * SmokeD * (bFull ? 0.25f : 0.15f);
			const float S1 = SmokeD * (0.5f + 0.7f * U()) * (bFull ? 1.0f : 1.2f);
			const float Life = static_cast<float>(X.AirPuffS) * (0.75f + 0.5f * U());
			FSimFxParticle P = Smoke(B.Pos + Off, S1 * 0.3f, S1, Life, X.AirSmokeColour, static_cast<float>(X.SmokeOpacity));
			// Carried a little on by the missile's way, then with the wind; rising a little; widening 2-3 times as it
			// thins and greys (LOOK 3).
			P.Vel = Off * 1.5f + B.Vel * 0.04f;
			P.Drag = 1.2f;
			P.Buoyancy = 15.0f;
			P.FadeInS = static_cast<float>(L.fireball_s) * 0.5f;
			P.FadeFrom = 0.35f;
			P.SpreadCmps = 1.5f * S1 / Life;
			P.Tint1 = X.AirGreyColour;
			P.bTintOverLife = true;
			Spawn(P);
		}
		if (bFull)
		{
			ConeSparks(B.Pos, X.AirSparks, X.AirSparkConeDeg, X.AirSparkReachM);
		}
		break;
	}
	case EFxBurstStyle::Ground:
	{
		const float H1 = RiseK;  // the column's top at 1 s, cm
		if (!bFull)
		{
			// Impostor: one fireball and the column as four stacked puffs at its height (LOOK 2.1, the art review's 2).
			Spawn(FireBall(B.Pos + FVector(0.0f, 0.0f, FireD * 0.4f), FVector::ZeroVector, FireD * 1.4f, static_cast<float>(L.fireball_s)));
			const float Shares[4] = {0.2f, 0.45f, 0.7f, 0.95f};
			for (float F : Shares)
			{
				Member(F, 1.1f, static_cast<float>(X.ColumnLifeS) * (0.8f + 0.4f * U()), 0.0f);
			}
			FSimFxParticle Skirt = Smoke(B.Pos + FVector(0.0f, 0.0f, 300.0f), 0.3f * ColumnW, 0.8f * ColumnW, static_cast<float>(X.SkirtS), Soil,
				static_cast<float>(X.SkirtOpacity) * 0.8f);
			Skirt.Roll = 0.0f;
			Skirt.Spin = 0.0f;
			Skirt.Stretch = 2.5f;
			Skirt.Drag = 0.3f;
			Spawn(Skirt);
			break;
		}
		// The fireball: spiky sprites round its edge so the core is no sphere (LOOK 2.1), cooling on Gross's curve and
		// sorted with the soot that swallows it.
		const int32 FireSprites = FMath::Max(FMath::RoundToInt32(static_cast<float>(X.FireSpritesFull) * FMath::Max(B.Crowd, 0.5f)), 2);
		for (int32 I = 0; I < FireSprites; ++I)
		{
			const FVector Off = FVector(PM() * 0.35f, PM() * 0.35f, 0.15f + 0.35f * U()) * FireD;
			const FVector Vel = FVector(Off.X, Off.Y, 0.0f) * 3.0f + FVector(0.0f, 0.0f, 600.0f + 900.0f * U());
			Spawn(FireBall(B.Pos + Off, Vel, FireD * (0.5f + 0.35f * U()), 0.35f + 0.25f * U()));
		}
		// Soot and the first dust boiling over it within 0.05-0.3 s (LOOK 2.1: the fireball swallowed).
		for (int32 I = 0; I < 4; ++I)
		{
			const float Delay = 0.03f + 0.08f * U();
			const FVector Off = FVector(PM() * 0.4f, PM() * 0.4f, 0.2f + 0.4f * U()) * FireD;
			// Carried up into the column and gone by about 3 s (the second art review's item 3: left at the root they read
			// as a dark round ball at its base).
			FSimFxParticle P = Smoke(B.Pos + Off, FireD * 0.8f, FireD * (1.5f + 0.5f * U()), 3.0f + 1.0f * U(), FMath::Lerp(Low, Soil * Soot, 0.15f), 0.9f);
			P.Age = Age - Delay;
			P.Vel = FVector(Off.X, Off.Y, 0.0f) * 2.0f + FVector(0.0f, 0.0f, 1.4f * H1);
			P.Drag = 1.5f;
			P.GrowS = 0.15f;
			P.FadeFrom = 0.3f;
			Spawn(P);
		}
		// The ejecta curtain: soil jets from the crater in a V, 15-45 degrees off the vertical, up to 1.0-1.2 x the
		// column's 1 s height within 0.5 s, falling back by 2 s (LOOK 2.1: Graettinger's inverted cone).
		const int32 Jets = FMath::Max(FMath::RoundToInt32(static_cast<float>(X.EjectaJets) * FMath::Max(B.Crowd, 0.5f)), 1);
		for (int32 I = 0; I < Jets; ++I)
		{
			const float Tilt = FMath::DegreesToRadians(15.0f + 30.0f * U());
			const float Az = (static_cast<float>(I) + 0.6f * U()) / static_cast<float>(Jets) * 6.2832f;
			const FVector Axis(FMath::Sin(Tilt) * FMath::Cos(Az), FMath::Sin(Tilt) * FMath::Sin(Az), FMath::Cos(Tilt));
			const float Width = static_cast<float>(X.EjectaWidthM * 100.0) * Cube * (0.5f + 0.9f * U());
			const float Reach = H1 * (0.55f + 0.6f * U());
			const float Life = static_cast<float>(X.EjectaS) * (0.7f + 0.5f * U());
			// Each jet a chain of overlapping puffs thrown up along it, the farther the faster, stopped by the air within
			// about 0.5 s (drag 4/s: 86 %) and falling back (the second art review's item 3: one stretched sprite a jet
			// was a flat petal with straight edges). Wet soil and soot, darker than the dust that rises later, opaque
			// (LOOK 2.1: 1.0); uneven jets, not a crown of equal petals.
			const FLinearColor JetColour = FMath::Lerp(Soil, Low, 0.45f) * FMath::Lerp(1.0f, Soot, 0.5f);
			constexpr int32 Links = 4;
			for (int32 K = 0; K < Links; ++K)
			{
				// Each link about as wide as the gap to the next is long: they overlap into one jet.
				const float F = (static_cast<float>(K) + 0.6f + 0.4f * U()) / static_cast<float>(Links);
				const float Size = FMath::Max(Width * (0.9f + 0.6f * F), 0.45f * Reach);
				FSimFxParticle P = Smoke(B.Pos + Axis * (0.3f * Size), Size * 0.5f, Size * (1.1f + 0.3f * U()), Life * (0.8f + 0.3f * F),
					JetColour * (0.85f + 0.3f * U()), 0.95f);
				P.Vel = Axis * (F * Reach * 4.0f);
				P.Drag = 4.0f;
				P.Gravity = 500.0f;
				P.Wind = FVector::ZeroVector;
				P.GrowS = 0.25f;
				P.FadeFrom = 0.4f;
				P.Spin = PM() * 0.4f;
				Spawn(P);
			}
		}
		// The column, rooted in the ground, rising as t^1/2, graded from dark soil low to lighter dust high (LOOK 0.3,
		// 2.1), and two stems from the ground to its top under the puffs.
		const int32 Members = FMath::Clamp(static_cast<int32>(X.ColumnMembers * (1.0 + 0.5 * FMath::Log2(FMath::Max(B.ChargeKg / 100.0, 0.25)))), 6, 18);
		for (int32 I = 0; I < Members; ++I)
		{
			const float F = FMath::Clamp((static_cast<float>(I) + 0.5f + 0.4f * PM()) / static_cast<float>(Members), 0.04f, 1.0f);
			Member(F, 1.15f, static_cast<float>(X.ColumnLifeS) * (0.8f + 0.4f * U()), 0.0f);
		}
		for (int32 I = 0; I < 2; ++I)
		{
			// Faint, light and wide (the second art review's item 3: at 0.45 and dark they were a vertical band up the
			// column): they only fill the gaps between its puffs.
			FSimFxParticle P = Smoke(B.Pos + FVector(PM(), PM(), 0.0f) * (0.1f * ColumnW), 0.5f * FireD, ColumnW * (0.6f + 0.15f * U()),
				static_cast<float>(X.ColumnLifeS) * 0.6f, FMath::Lerp(Low, High, 0.6f), 0.22f);
			P.Facing = ESimFxFacing::Axis;
			P.FixedAxis = FVector::UpVector;
			P.bRooted = true;
			P.LengthK = RiseK * 0.9f;
			P.RiseStopS = StabS;
			P.GrowS = 6.0f;
			P.Drag = 0.3f;
			P.Spin = 0.0f;
			P.FadeInS = 0.3f;
			Spawn(P);
		}
		// The low dust skirt racing out to 50-100 m on dry ground, then drifting (LOOK 2.1): r = R (1 - e^(-t/0.6)).
		{
			const float Radius = static_cast<float>(X.SkirtRadiusM * 100.0) * Cube;
			const float Height = 1000.0f * Cube;
			// Beside other bursts' skirts, fewer of its own (at least half).
			const int32 SkirtPuffs = FMath::Max(FMath::RoundToInt32(static_cast<float>(X.SkirtPuffs) * FMath::Max(B.Crowd, 0.5f)), 4);
			for (int32 I = 0; I < SkirtPuffs; ++I)
			{
				const float Az = (static_cast<float>(I) + 0.7f * U()) / static_cast<float>(SkirtPuffs) * 6.2832f;
				const FVector Out(FMath::Cos(Az), FMath::Sin(Az), 0.0f);
				// Every other puff stops short, so the skirt is a filled sheet of dust, not a ring of pillows.
				const float Reach = Radius * ((I % 2) == 0 ? 0.7f + 0.5f * U() : 0.25f + 0.35f * U());
				// Small at first and growing as it races out (r = R (1 - e^(-t/0.6))), a little flattened and turned each its
				// own way, thinning from 0.6 to 0.3 by 3 s and then drifting (LOOK 2.1; the second art review's item 3: big
				// flat pillows at once, opaque for 6 s).
				// Wide and low enough that neighbours overlap into one sheet (at 1.3-1.9 x the height they stood apart as a
				// row of pillows seen from the side).
				FSimFxParticle P = Smoke(B.Pos + Out * (0.3f * FireD) + FVector(0.0f, 0.0f, (0.25f + 0.25f * U()) * Height), Height * 0.5f, Height * (1.7f + 0.6f * U()),
					static_cast<float>(X.SkirtS) * (0.7f + 0.6f * U()), FMath::Lerp(Soil, Low, 0.25f), static_cast<float>(X.SkirtOpacity));
				P.Vel = Out * (Reach / 0.6f);
				P.Drag = 1.0f / 0.6f;
				P.Roll = PM() * 0.3f;
				P.Spin = 0.0f;
				P.Stretch = 1.8f + 0.4f * U();
				P.GrowS = 0.8f;
				P.ThinS = 3.0f;
				P.ThinTo = 0.5f;
				P.FadeFrom = 0.3f;
				P.Buoyancy = 30.0f;
				Spawn(P);
			}
			if (B.ChargeKg >= 200.0)
			{
				// A second, slower wave of dust from the fallout (LOOK 2.3).
				for (int32 I = 0; I < SkirtPuffs / 2; ++I)
				{
					const float Az = U() * 6.2832f;
					const FVector Out(FMath::Cos(Az), FMath::Sin(Az), 0.0f);
					FSimFxParticle P = Smoke(B.Pos + Out * (0.5f * Radius) + FVector(0.0f, 0.0f, 0.5f * Height), Height * 0.6f, Height * 1.4f,
						static_cast<float>(X.SkirtS), FMath::Lerp(Soil, High, 0.5f), static_cast<float>(X.SkirtOpacity) * 0.7f);
					P.Age = Age - (2.0f + 2.0f * U());
					P.Vel = Out * (0.3f * Radius / 2.0f);
					P.Drag = 0.5f;
					P.Roll = PM() * 0.35f;
					P.Spin = 0.0f;
					P.Stretch = 1.4f;
					P.GrowS = 2.0f;
					P.FadeInS = 1.0f;
					P.FadeFrom = 0.2f;
					Spawn(P);
				}
			}
		}
		// Clods in a few clumps thrown up in a 45 degree cone, the larger trailing dust, falling back (LOOK 2.1).
		{
			const int32 Debris = FMath::RoundToInt32(static_cast<float>(FMath::Min(L.debris, X.DebrisMax)) * B.Crowd);
			const int32 Clumps = FMath::Max(X.ClodClumps, 1);
			const float Flat = B.bCruise ? 0.45f : 1.0f;  // a building's debris thrown wider and flatter (LOOK 7)
			for (int32 C = 0; C < Clumps; ++C)
			{
				const float ClumpAz = U() * 6.2832f;
				const float ClumpEl = FMath::DegreesToRadians(FMath::Lerp(45.0f, 82.0f, U()) * Flat);
				for (int32 I = C; I < Debris; I += Clumps)
				{
					FSimFxParticle P;
					P.Layer = ESimFxLayer::Debris;
					P.Pos = B.Pos + FVector(0.0f, 0.0f, 100.0f);
					const float El = ClumpEl + FMath::DegreesToRadians(8.0f) * PM();
					const float Az = ClumpAz + FMath::DegreesToRadians(12.0f) * PM();
					const float Speed = static_cast<float>(X.DebrisSpeedMps * 100.0) * (0.5f + 0.6f * U()) * (B.bCruise ? 1.4f : 1.0f);
					P.Vel = FVector(FMath::Cos(El) * FMath::Cos(Az), FMath::Cos(El) * FMath::Sin(Az), FMath::Sin(El)) * Speed;
					P.Age = Age;
					P.Life = FMath::Clamp(2.0f * P.Vel.Z / 980.0f, 0.5f, 6.0f);
					P.Size0 = P.Size1 = static_cast<float>(X.DebrisSizeM * 100.0) * (0.5f + U());
					P.FadeFrom = 0.9f;
					P.Tint = X.DebrisColour;
					P.Frame0 = FMath::Floor(U() * 15.99f);
					P.Gravity = 980.0f;
					P.Drag = 0.2f;
					P.Roll = U() * 6.2832f;
					P.Spin = PM() * 8.0f;
					Spawn(P);
					// Among a stick's other bursts the clods fly bare (their dust trails, three puffs each, are most of a
					// burst's sprites, and the stick's own dust hides them).
					if ((I % 2) == 0 && B.Crowd >= 1.0f)
					{
						Trail(P.Pos, P.Vel, P.Drag, P.Gravity, 3, 0.12f, 180.0f + 120.0f * U(), 1.5f + 1.5f * U(), Soil, 0.55f);
					}
				}
			}
		}
		// The shock: a faint ring racing out over the ground (LOOK 0.4, 2.1: thin, 0.2-0.3, 0.3 s), none at night; the
		// drawing fades it at grazing angles.
		if (!Sun.bNight)
		{
			FSimFxParticle Ring;
			Ring.Layer = ESimFxLayer::Ring;
			Ring.Facing = ESimFxFacing::Flat;
			Ring.Pos = B.Pos + FVector(0.0f, 0.0f, 80.0f);
			Ring.Age = Age;
			Ring.Life = static_cast<float>(X.RingS);
			Ring.Size0 = FireD;
			Ring.Size1 = FMath::Min(static_cast<float>(X.RingSpeedMps * X.RingS * 200.0), FMath::Max(DustD, FireD) * 4.0f);
			Ring.FadeFrom = 0.2f;
			Ring.Tint = X.RingColour * Brightness(X.RingBrightness);
			Spawn(Ring);
		}
		Sparks(FMath::Max(FMath::RoundToInt32(8.0f * B.Crowd), 2), static_cast<float>(X.SparkSpeedMps * 60.0), static_cast<float>(X.SparkS), 1.0f);
		break;
	}
	case EFxBurstStyle::Small:
	{
		// A submunition, a rocket or a round on the ground (LOOK 2.2): a flash, a dust spurt of soil, a few clods.
		const float Puff = FMath::Max(SmokeD, 250.0f);
		Spawn(FireBall(B.Pos + FVector(0.0f, 0.0f, FireD * 0.4f), FVector::ZeroVector, FireD * 1.5f, static_cast<float>(L.fireball_s)));
		const int32 Puffs = bFull ? 2 : 1;
		for (int32 I = 0; I < Puffs; ++I)
		{
			FSimFxParticle P = Smoke(B.Pos + FVector(PM(), PM(), 0.0f) * (0.2f * Puff) + FVector(0.0f, 0.0f, 0.4f * Puff), 0.4f * Puff,
				Puff * (0.8f + 0.5f * U()), FMath::Max(static_cast<float>(L.smoke_s), 8.0f) * (0.8f + 0.6f * U()), FMath::Lerp(Soil, Low, 0.5f), 0.8f);
			P.Vel = FVector(0.0f, 0.0f, 200.0f + 300.0f * U());
			P.Drag = 1.0f;
			P.Buoyancy = 10.0f;
			Spawn(P);
		}
		if (bFull)
		{
			Sparks(4, static_cast<float>(X.SparkSpeedMps * 30.0), static_cast<float>(X.SparkS), 1.0f);
		}
		break;
	}
	case EFxBurstStyle::Flak:
	case EFxBurstStyle::SdPuff:
	{
		// A shell's burst by its calibre (LOOK 4): the light grey of RDX and A-IX-2 (23-57 mm) or the black of TNT
		// (85-130 mm), full size within 0.5-1 s, hanging 10 s to 3 min; black puffs grey as they spread.
		const SimFxLogic::FFlakRow Row = SimFxLogic::FlakFor(X.FlakByCalibre, B.CalibreMm > 0.0 ? B.CalibreMm : 23.0);
		const float Black = static_cast<float>(Row.Black);
		const float Puff = static_cast<float>(Row.PuffM * 100.0 * SimFxLogic::SachsScale(AltitudeOf(B.Enu)));
		const FLinearColor Colour = FMath::Lerp(X.FlakLightColour, X.FlakBlackColour, Black);
		const FLinearColor Later = FMath::Lerp(X.FlakLightColour, X.AirGreyColour, Black);
		Spawn(FireBall(B.Pos, FVector::ZeroVector, FMath::Max(FireD * 1.5f, 120.0f), FMath::Max(static_cast<float>(L.fireball_s), 0.12f)));
		const int32 Puffs = bFull ? (Black > 0.5f ? 3 : 2) : 1;
		for (int32 I = 0; I < Puffs; ++I)
		{
			const FVector Off = bFull ? FVector(PM(), PM(), PM() * 0.6f) * Puff * 0.18f : FVector::ZeroVector;
			const float S1 = Puff * (bFull ? 0.75f + 0.35f * U() : 1.1f);
			const float Life = static_cast<float>(Row.HangS) * (0.8f + 0.4f * U());
			FSimFxParticle P = Smoke(B.Pos + Off, S1 * 0.3f, S1, Life, Colour, Black > 0.5f ? 0.95f : 0.85f);
			P.Vel = Off;
			P.Drag = 1.5f;
			P.GrowS = 0.3f;
			P.Buoyancy = 5.0f;
			P.FadeInS = 0.03f;
			P.FadeFrom = 0.3f;
			// Expanding several times as it hangs (LOOK 4; the second art review's item 6: KS-30 puffs had barely grown by
			// 40 s): a black puff to about 3.5 times its size by the end of its life, half again by a third of it.
			P.SpreadCmps = (Black > 0.5f ? 2.5f : 1.5f) * S1 / Life;
			P.Tint1 = Later;
			P.bTintOverLife = true;
			Spawn(P);
		}
		break;
	}
	case EFxBurstStyle::Kill:
	{
		// An aircraft blowing up (LOOK 6): its fuel's fireball, 39-58 m, carried on along its path and slowing in the air,
		// stretched along its velocity and glowing 3-4.5 s under a black soot envelope; pieces falling, the burning ones
		// trailing smoke.
		const float D = FireD;
		const float FuelS = static_cast<float>(L.fireball_s);
		const FVector V = B.Vel;
		// Five large sprites at nearly the aircraft's speed (the second art review's item 5: eight at 0.9-1.0 of it drew
		// apart by 0.1 V t, three lobes 112 m long by 2 s against the 46 m ball).
		const int32 FireCount = bFull ? 5 : 2;
		for (int32 I = 0; I < FireCount; ++I)
		{
			const FVector Off = FVector(PM(), PM(), PM()) * D * (bFull ? 0.18f : 0.1f);
			FSimFxParticle P = FireBall(B.Pos + Off, V * (0.97f + 0.03f * U()) + Off * 0.5f, D * (bFull ? 0.6f + 0.2f * U() : 0.8f), FuelS * (0.8f + 0.3f * U()));
			P.Size0 = P.Size1 * 0.3f;
			P.GrowS = 0.25f;
			// Slowing gently in the air, so it stays round the falling aircraft through its seconds (LOOK 6).
			P.Drag = 0.2f;
			P.Facing = V.SizeSquared() > 100.0 ? ESimFxFacing::Axis : ESimFxFacing::Camera;
			P.Stretch = 1.25f;
			// Hydrocarbon flame: cooler and slower than HE's (LOOK 6), glowing most of its life.
			P.HeatHoldS = 0.3f * P.Life;
			P.HeatTauS = 0.35f * P.Life;
			P.Frames = 20.0f;
			Spawn(P);
		}
		const int32 Puffs = bFull ? 7 : 2;
		for (int32 I = 0; I < Puffs; ++I)
		{
			const FVector Off = FVector(PM(), PM(), PM()) * D * 0.3f;
			const float S1 = D * (bFull ? 0.6f + 0.4f * U() : 0.9f);
			const float Life = static_cast<float>(X.AirPuffS) * (0.5f + 0.4f * U());
			FSimFxParticle P = Smoke(B.Pos + Off, S1 * 0.4f, S1, Life, Tune.Wreck.SmokeColour * 0.5f, 0.9f);
			// Trailing the fire a little (slower along the path) so the fire shows ahead of its own soot.
			// (0.9-0.97 of the speed and a little more drag: some 40 m behind by 2 s at 250 m/s, an envelope round the fire,
			// not a separate cloud 130 m back.)
			P.Vel = V * (0.9f + 0.07f * U()) + Off * 0.5f;
			P.Drag = 0.25f;
			P.GrowS = 0.6f;
			P.FadeInS = 0.3f;
			P.FadeFrom = 0.35f;
			P.Buoyancy = 20.0f;
			P.SpreadCmps = S1 / Life;
			P.Tint1 = FLinearColor(0.10f, 0.09f, 0.08f);
			P.bTintOverLife = true;
			Spawn(P);
		}
		if (bFull)
		{
			const int32 Pieces = FMath::Max(Tune.Wreck.KillPieces, 0);
			const int32 Burning = FMath::Min(Tune.Wreck.KillBurningPieces, Pieces);
			for (int32 I = 0; I < Pieces; ++I)
			{
				FSimFxParticle P;
				P.Layer = ESimFxLayer::Debris;
				P.Pos = B.Pos + FVector(PM(), PM(), PM()) * D * 0.15f;
				P.Vel = V * (0.5f + 0.4f * U()) + FVector(PM(), PM(), PM() + 0.3f).GetSafeNormal() * 3000.0f * (0.3f + U());
				P.Age = Age;
				P.Life = 3.0f + 5.0f * U();
				P.Size0 = P.Size1 = 50.0f + 250.0f * U();
				P.FadeFrom = 0.85f;
				P.Tint = FLinearColor(0.05f, 0.05f, 0.05f);
				P.Frame0 = FMath::Floor(U() * 15.99f);
				P.Gravity = 980.0f;
				P.Drag = 0.4f;
				P.Roll = U() * 6.2832f;
				P.Spin = PM() * 6.0f;
				Spawn(P);
				if (I < Burning)
				{
					// Burning (#FF8D15): a flame moving with the piece, and a smoke streamer laid behind it.
					FSimFxParticle F = FireBall(P.Pos, P.Vel, 180.0f + 120.0f * U(), P.Life * 0.8f);
					F.Kind = ESimFxKind::HeatFlipbook;
					F.Frame0 = 8.0f;
					F.Frames = 40.0f;
					F.Size0 = F.Size1 = 200.0f + 150.0f * U();
					F.Heat0 = 1.0f;
					F.HeatHoldS = 0.5f * F.Life;
					F.HeatTauS = 0.4f * F.Life;
					F.Wind = FVector::ZeroVector;
					F.Drag = P.Drag;
					F.Gravity = P.Gravity;
					Spawn(F);
					Trail(P.Pos, P.Vel, P.Drag, P.Gravity, 5, 0.35f, 250.0f + 150.0f * U(), 8.0f + 4.0f * U(), FLinearColor(0.027f, 0.024f, 0.021f), 0.8f);
				}
			}
			Sparks(X.SparksMax, static_cast<float>(X.SparkSpeedMps * 80.0), static_cast<float>(X.SparkS) * 1.5f, 0.0f);
		}
		break;
	}
	case EFxBurstStyle::Arm:
	{
		// An ARM bursting just over a radar (LOOK 8): a sharp flash, fragments sprayed on along its path, kicking up dust
		// in a stretched ellipse, a grey-black puff; a Shrike's white phosphorus arcing out in smoking pieces that merge
		// into a rising white column.
		Spawn(FireBall(B.Pos, B.Vel * 0.05f, FireD * 1.1f, static_cast<float>(L.fireball_s)));
		const int32 Puffs = bFull ? 3 : 1;
		for (int32 I = 0; I < Puffs; ++I)
		{
			const FVector Off = FVector(PM(), PM(), PM() * 0.5f) * SmokeD * 0.2f;
			const float S1 = SmokeD * (0.6f + 0.5f * U());
			FSimFxParticle P = Smoke(B.Pos + Off, S1 * 0.3f, S1, 18.0f + 6.0f * U(), FLinearColor(0.042f, 0.038f, 0.034f), 0.9f);
			P.Vel = Off + B.Vel * 0.03f;
			P.Buoyancy = 20.0f;
			P.FadeFrom = 0.35f;
			Spawn(P);
		}
		if (!bFull)
		{
			if (B.bWp)
			{
				FSimFxParticle P = Smoke(B.Pos, 1000.0f, 3500.0f, 180.0f, X.WpColour, 0.95f);
				P.RiseK = 0.6f * RiseK;
				P.RiseStopS = StabS;
				P.GrowS = 3.0f;
				Spawn(P);
			}
			break;
		}
		ConeSparks(B.Pos, X.AirSparks, 40.0, X.AirSparkReachM);
		// The fragments' dust spurts on the ground, 20-60 m on along the flight path (LOOK 8).
		{
			FVector Along = FVector(B.Vel.X, B.Vel.Y, 0.0f).GetSafeNormal();
			if (Along.IsNearlyZero())
			{
				Along = FVector(PM(), PM(), 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			}
			const FVector Across(-Along.Y, Along.X, 0.0f);
			const FVector Ground = B.Pos - FVector(0.0f, 0.0f, static_cast<float>(X.ArmBurstUpM * 100.0));
			for (int32 I = 0; I < 14; ++I)
			{
				const float S = FMath::Lerp(500.0f, 6000.0f, U());
				const float Side = PM() * (300.0f + 0.25f * S);
				const float Size = 200.0f + 250.0f * U();
				FSimFxParticle P = Smoke(Ground + Along * S + Across * Side + FVector(0.0f, 0.0f, 0.4f * Size), Size * 0.3f, Size, 4.0f + 4.0f * U(),
					FMath::Lerp(Soil, Low, 0.2f), 0.8f);
				P.Age = Age - S / 30000.0f;  // the fragments at about 300 m/s
				P.Vel = FVector(0.0f, 0.0f, 150.0f + 250.0f * U());
				P.GrowS = 0.2f;
				P.FadeFrom = 0.3f;
				Spawn(P);
			}
		}
		if (B.bWp)
		{
			// The white-phosphorus "octopus": burning pieces arcing 20-50 m out, each trailing white smoke.
			for (int32 I = 0; I < X.ArmWpPieces; ++I)
			{
				const FVector Dir = SimFxLogic::DirInCone(FVector::UpVector, FMath::DegreesToRadians(75.0), U(), U());
				const FVector Vel = Dir * (1500.0f + 1500.0f * U());
				FSimFxParticle P;
				P.Layer = ESimFxLayer::Spark;
				P.Pos = B.Pos;
				P.Vel = Vel;
				P.Age = Age;
				P.Life = 1.5f + 1.0f * U();
				P.Size0 = P.Size1 = 120.0f;
				P.FadeFrom = 0.7f;
				P.Tint = FLinearColor(1.0f, 0.95f, 0.85f) * Brightness(X.SparkBrightness);
				P.Gravity = 980.0f;
				P.Drag = 0.6f;
				P.MinPx = 1.0f;
				Spawn(P);
				// Close-set puffs along its arc, one body of smoke (the second art review's item 9: five 0.3 s apart were a
				// string of pearls).
				Trail(B.Pos, Vel, P.Drag, P.Gravity, 13, 0.1f, 250.0f + 150.0f * U(), 60.0f + 60.0f * U(), X.WpColour, 0.85f);
			}
			// The trails merging into a dense white cloud that pillars upward (LOOK 8: 20-60 m wide).
			for (int32 I = 0; I < 4; ++I)
			{
				const float F = (static_cast<float>(I) + 0.5f) / 4.0f;
				FSimFxParticle P = Smoke(B.Pos + FVector(PM(), PM(), 0.0f) * 800.0f, 800.0f, 2500.0f + 2000.0f * U(), 180.0f + 120.0f * U(), X.WpColour, 0.95f);
				P.Age = Age - 1.5f;
				P.RiseK = 0.6f * F * RiseK;
				P.RiseStopS = StabS;
				P.GrowS = 4.0f;
				P.FadeInS = 1.5f;
				P.FadeFrom = 0.4f;
				P.Drag = 0.3f;
				Spawn(P);
			}
		}
		break;
	}
	case EFxBurstStyle::Haze:
	{
		// A cluster carpet's puffs merged into a low haze drifting over its pattern (LOOK 5: 5-10 m high, #A8A39A, 0.5).
		const FSimFxTuning::FBombs& T = Tune.Bombs;
		const FVector Track = FVector(B.Dir.X, B.Dir.Y, 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector Across(-Track.Y, Track.X, 0.0f);
		const int32 Count = bFull ? FMath::Max(T.ClusterHazeSprites, 1) : 3;
		const float Height = static_cast<float>(T.ClusterHazeM * 100.0);
		const float Long = 100.0f * B.HazeL;
		const float Wide = 100.0f * B.HazeW;
		for (int32 I = 0; I < Count; ++I)
		{
			const float Along = (static_cast<float>(I) + 0.5f) / static_cast<float>(Count) - 0.5f + 0.15f * PM() / static_cast<float>(Count);
			const FVector At = B.Pos + Track * (Along * Long) + Across * (0.3f * PM() * Wide) + FVector(0.0f, 0.0f, 0.4f * Height);
			const float Span = FMath::Max(Wide, Long / static_cast<float>(Count) * 1.8f);
			FSimFxParticle P = Smoke(At, Height, Height * 1.6f, static_cast<float>(T.ClusterHazeS) * (0.7f + 0.5f * U()), T.ClusterHazeColour, 0.5f);
			P.Roll = 0.0f;
			P.Spin = 0.0f;
			P.Stretch = FMath::Clamp(Span / (Height * 1.6f), 1.0f, 8.0f);
			P.GrowS = 2.0f;
			P.FadeInS = 1.5f;
			P.FadeFrom = 0.3f;
			P.Drag = 0.3f;
			Spawn(P);
		}
		break;
	}
	}
}

// ------------------------------------------------------------------ missiles

void FSimFxWorld::DrawMissile(FSimFxMissile& M, const FVector& Pos, const FVector& Fwd, double SinceLaunch)
{
	static const FSimFxMunition NoMunition;
	const FSimFxMunition& Mun = M.Mun != nullptr ? *M.Mun : NoMunition;
	const FSimFxTuning::FMissile& T = Tune.Missile;
	const float Scale = static_cast<float>(FMath::Clamp(Mun.DiameterM / 0.206, 0.3, 2.5));
	const FVector Tail = Pos - Fwd * (50.0 * Mun.LengthM);
	// The smoke levels by reference (every missile and rocket every frame: no string copies).
	static const FString DenseSmoke(TEXT("dense")), MinimalSmoke(TEXT("minimal"));
	const FString& NoMotorSmoke = M.bLowSmoke ? MinimalSmoke : DenseSmoke;
	const double Boost = Mun.bMotor ? Mun.BoostS : 3.0;
	const double Sustain = Mun.bMotor ? Mun.SustainS : 12.0;
	const FString& BoostSmoke = Mun.bMotor ? Mun.Smoke : NoMotorSmoke;
	const FString& SustainSmoke = Mun.bMotor ? Mun.SustainSmoke : NoMotorSmoke;
	const simcore::fx::MotorPhase Phase = simcore::fx::MotorPhaseAt(SinceLaunch, Boost, Sustain);
	const bool bBurning = Phase == simcore::fx::MotorPhase::Boost || Phase == simcore::fx::MotorPhase::Sustain;
	const bool bBoost = Phase == simcore::fx::MotorPhase::Boost;
	const FLinearColor PlumeColour = Mun.bPlumeColour ? Mun.PlumeColour : T.PlumeColour;
	if (bBurning)
	{
		const float Flick = SimFxEffFlicker(Now, static_cast<double>(M.Key));
		FSimFxSprite S;
		S.Layer = ESimFxLayer::Plume;
		S.Facing = ESimFxFacing::Axis;
		S.Axis = -Fwd;
		S.Length = static_cast<float>(100.0 * (bBoost ? T.PlumeBoostLengthM : T.PlumeSustainLengthM)) * Scale * Flick;
		S.Width = static_cast<float>(100.0 * (bBoost ? T.PlumeBoostWidthM : T.PlumeSustainWidthM)) * Scale;
		S.Pos = Tail - Fwd * (0.5 * S.Length);
		S.Tint = PlumeColour * Brightness(T.PlumeBrightness);
		AddSprite(S);
		FSimFxSprite G;
		G.Layer = ESimFxLayer::Glow;
		G.Pos = Tail - Fwd * 50.0;
		G.Width = G.Length = static_cast<float>(100.0 * (bBoost ? T.GlowBoostM : T.GlowSustainM)) * Scale * Flick;
		G.Tint = PlumeColour * Brightness(T.PlumeBrightness * 0.5);
		// At night a burning motor is the point of light the eye follows: a glint seen whole, its bright core about glint_px
		// (the second art review's item 9 and the coordinator's look of 1 Nov: about 1 px at 1.3-3 km, its true area
		// smeared over its pixel floor).
		G.MinPx = Sun.bNight ? static_cast<float>(Tune.Lod.GlintSpritePx) : 1.5f;
		G.KeepAlpha = Sun.bNight ? static_cast<float>(Tune.Lod.GlintKeep) : 0.0f;
		AddSprite(G);
		if (Sun.bNight && DistToCamM(Tail) < 6000.0)
		{
			AddLight(Tail, PlumeColour, bBoost ? 3.0e7 : 1.0e7, 4000.0 * Scale);
		}
	}
	if (Mun.bTailTracer && Phase != simcore::fx::MotorPhase::None)
	{
		FSimFxSprite G;
		G.Layer = ESimFxLayer::Glow;
		G.Pos = Tail;
		G.Width = G.Length = static_cast<float>(100.0 * T.TracerGlowM);
		G.Tint = T.TracerColour * Brightness(T.TracerBrightness);
		G.MinPx = 1.0f;
		AddSprite(G);
	}
	double WidthM = 0.0, Opacity = 0.0;
	const FString& Level = bBoost ? BoostSmoke : SustainSmoke;
	// No smoke laid past the effects' range or the weather's visibility (docs/fx/README.md 4.1): a neighbour's SAM 60 km
	// off held a ribbon of the pool and was meshed every frame unseen.
	const bool bSmoke = bBurning && SimFxEffSmokeLevel(Level, T, WidthM, Opacity) && DistToCamM(Tail) < TrailRangeM(false);
	if (bSmoke)
	{
		const double Life = Mun.SmokeS > 0.0 ? Mun.SmokeS : (Mun.Kind == TEXT("rocket") ? Tune.Bombs.RocketSmokeS : T.DefaultSmokeS);
		// It starts about as wide as the fin span (LOOK 9.1: the turbulent wake; 0.65 m for the 9M33, 2.2 m for the
		// S-75's booster), a reduced or minimal smoke motor's narrower; a munition without fins by the level's width.
		float WidthCm = static_cast<float>(WidthM * 100.0) * Scale;
		if (Mun.SpanM > 0.0)
		{
			const double Share = Level == TEXT("dense") ? T.TrailSpanShareDense : (Level == TEXT("reduced") ? T.TrailSpanShareReduced : T.TrailSpanShareMinimal);
			WidthCm = static_cast<float>(100.0 * Mun.SpanM * Share);
		}
		const float Op = static_cast<float>(Opacity * (M.bLowSmoke ? 0.5 : 1.0));
		if (Sun.bNight)
		{
			// At night the flame lights its own smoke for a few metres behind it (LOOK 9.3, 15): a short glowing wake, held to
			// a couple of pixels far off, so a climbing motor is a bright point with a lit tail, not a lone pixel (the
			// coordinator's look of 1 Nov). Its head at the motor (T_FX_Streak's bright end along its axis).
			FSimFxSprite L;
			L.Layer = ESimFxLayer::Streak;
			L.Facing = ESimFxFacing::Axis;
			L.Axis = Fwd;
			L.Length = static_cast<float>(100.0 * T.NightLitTrailM);
			L.Width = FMath::Max(WidthCm, 50.0f) * 2.0f;
			L.Pos = Tail - Fwd * (0.45 * L.Length);
			L.Tint = PlumeColour * T.TrailColour * (Brightness(T.NightLitTrailBrightness) * Op);
			L.MinPx = 2.0f;
			L.KeepAlpha = static_cast<float>(0.5 * Tune.Lod.GlintKeep);
			AddSprite(L);
		}
		const FSimFxRibbonStyle Style = MissileTrailStyle(Life);
		// Each point drifts with the wind at its own height (shear: LOOK 9.1) and rises a little, warm.
		const FVector PointDrift = WindAt(ToE(Tail)) + FVector(0.0, 0.0, 100.0 * T.TrailRiseMps);
		const bool bHelix = Mun.Kind == TEXT("sam") && Mun.DiameterM > 0.0 && Mun.DiameterM <= T.HelixMaxDiameterM;
		if (!bHelix)
		{
			TrailPoint(M.Ribbon, M.Key, M.LastTrailPos, M.bHaveTrail, Tail, WidthCm, Op, T.TrailPointM, Style, PointDrift);
		}
		else
		{
			// A MANPADS corkscrews (LOOK 9.2): its trail laid along the helix a few metres a point, the frame's flight cut
			// into steps so a turn of 21.5 m is never drawn as a triangle.
			if (!M.bHaveTrail)
			{
				M.HelixAxisPos = Tail;
				M.HelixDistM = 0.0;
			}
			const double StepCm = 100.0 * FMath::Max(T.HelixPointM, 0.5);
			const FVector From = M.HelixAxisPos;
			const double Span = FVector::Dist(From, Tail);
			const FVector Dir = Span > 1.0 ? (Tail - From) / Span : Fwd;
			const int32 Steps = FMath::Min(FMath::FloorToInt(Span / StepCm), 64);
			for (int32 K = 1; K <= Steps; ++K)
			{
				const FVector P = From + Dir * (StepCm * K);
				M.HelixDistM += StepCm / 100.0;
				// Its radius uneven turn to turn (HelixRadiusAt: the seeker's corrections, LOOK 9.2), not a regular spring.
				const double Radius = SimFxLogic::HelixRadiusAt(M.HelixDistM, T.HelixPitchM, T.HelixRadiusM, M.HelixPhase);
				TrailPoint(M.Ribbon, M.Key, M.LastTrailPos, M.bHaveTrail, P + SimFxLogic::HelixOffset(Dir, M.HelixDistM, T.HelixPitchM, Radius, M.HelixPhase),
					WidthCm, Op, 0.0, Style, PointDrift);
			}
			M.HelixAxisPos = Steps > 0 ? From + Dir * (StepCm * Steps) : M.HelixAxisPos;
			const double Ahead = FVector::Dist(M.HelixAxisPos, Tail) / 100.0;
			const double RadiusAhead = SimFxLogic::HelixRadiusAt(M.HelixDistM + Ahead, T.HelixPitchM, T.HelixRadiusM, M.HelixPhase);
			TrailPoint(M.Ribbon, M.Key, M.LastTrailPos, M.bHaveTrail, Tail + SimFxLogic::HelixOffset(Dir, M.HelixDistM + Ahead, T.HelixPitchM, RadiusAhead, M.HelixPhase),
				WidthCm, Op, 1.0e6, Style, PointDrift);
		}
	}
	else if (M.Ribbon >= 0)
	{
		ReleaseRibbon(M.Ribbon, M.Key);
		M.Ribbon = -1;
		M.bHaveTrail = false;
	}
	if (M.bShowMesh && Mun.bHasMesh)
	{
		double Px = 0.0;
		TierAt(Pos, Mun.LengthM, &Px);
		if (Px > Tune.Lod.MunitionPx)
		{
			if (UStaticMesh* Mesh = FSimMunitionModels::Mesh(Mun.Id))
			{
				FSimFxMeshDraw D;
				D.Mesh = Mesh;
				D.Pos = Pos;
				D.Rot = Fwd.Rotation();
				Meshes.Add(D);
			}
		}
	}
}

void FSimFxWorld::UpdateMissiles()
{
	// Every missile entity of the snapshot (ours, the EM ARMs) gets its plume and smoke.
	if (States != nullptr)
	{
		for (const FSimEntityState& S : *States)
		{
			if (!S.bIsMissile || !S.bAlive || S.Id < SimFxIds::MissileBase || S.Id >= SimFxIds::GroundBase)
			{
				continue;
			}
			bool bKnown = false;
			for (const FSimFxMissile& M : Missiles)
			{
				if (M.EntityId == S.Id && M.Mode == FSimFxMissile::EMode::Entity)
				{
					bKnown = true;
					break;
				}
			}
			if (bKnown)
			{
				continue;
			}
			FSimFxMissile& M = Missiles.AddDefaulted_GetRef();
			M.Mode = FSimFxMissile::EMode::Entity;
			M.EntityId = S.Id;
			M.Number = S.Id < SimFxIds::ArmBase ? S.Id - SimFxIds::MissileBase : 0;
			M.Key = NextKey++;
			M.Weapon = S.Model.IsEmpty() ? FString(M.Number > 0 ? TEXT("9m33") : TEXT("missile_arm")) : S.Model;
			M.Mun = &Munition(M.Weapon);
			M.LaunchT = Now;
			M.bLowSmoke = M.Number == 0;
		}
	}
	for (int32 I = Missiles.Num() - 1; I >= 0; --I)
	{
		FSimFxMissile& M = Missiles[I];
		if (M.bDone)
		{
			ReleaseRibbon(M.Ribbon, M.Key);
			Missiles.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		if (M.Mode == FSimFxMissile::EMode::Entity)
		{
			const FSimEntityState* S = Entity(M.EntityId);
			if (S == nullptr || !S->bAlive)
			{
				// Ended in the simulation: our missile's MissileEnd tells how (and may fly it on); an ARM's ArmBurst.
				if (Now - M.LaunchT > 0.5 || S != nullptr)
				{
					M.bDone = M.Number == 0 || Now - M.LaunchT > 60.0;
					ReleaseRibbon(M.Ribbon, M.Key);
					M.Ribbon = -1;
					if (M.Number > 0 && !M.bDone)
					{
						M.bAlive = false;
					}
				}
				continue;
			}
			const FVector Pos = EntityPos(*S);
			const FVector Fwd = S->Velocity.SizeSquared() > 1.0 ? S->Velocity.GetSafeNormal() : S->Rotation.Vector();
			M.Pos = ToE(Pos);
			// Its drawn state, for a MissileEnd that flies it on from here (without it the flight on started from the
			// event's state, a jump from where it was drawn) and for its warhead's fragments.
			M.Vel = ToE(Pos + S->Velocity) - M.Pos;
			M.SeenT = Now;
			M.PosU = Pos;
			M.FwdU = Fwd;
			DrawMissile(M, Pos, Fwd, Now - M.LaunchT);
			continue;
		}
		// Flown here: a Hermite from its last state to where it bursts (flying on), or a chase of its target.
		if (!M.bWaitForEnd)
		{
			const double H = FMath::Max(M.EndT - M.T0, 0.05);
			const double Uu = FMath::Clamp((Now - M.T0) / H, 0.0, 1.0);
			const Vec3 V1 = (M.Aim - M.P0).Normalized() * FMath::Max(M.V0.Length(), 100.0);
			M.Pos = SimFxEffHermite(M.P0, M.V0, M.Aim, V1, H, Uu);
			M.Vel = SimFxEffHermiteRate(M.P0, M.V0, M.Aim, V1, H, Uu);
			if (Now >= M.EndT)
			{
				if (M.bBurstAtEnd)
				{
					AddBurst(M.BurstStyle, M.Aim, M.ChargeKg, M.EndT, M.Vel);
				}
				M.bDone = true;
				continue;
			}
		}
		else if (Dt > 0.0f)
		{
			Vec3 Tgt = M.Aim;
			Vec3 TgtVel;
			const FSimEntityState* S = M.TargetId >= 0 ? Entity(M.TargetId) : EntityByName(M.TargetName);
			if (S != nullptr && S->bAlive)
			{
				const FVector P = EntityPos(*S);
				Tgt = ToE(P);
				TgtVel = ToE(P + S->Velocity) - Tgt;
			}
			const double Since = Now - M.LaunchT;
			const double Left = M.EndT - Now;
			const Vec3 To = Tgt + TgtVel * FMath::Clamp(Left, 0.0, 5.0) * 0.5 - M.Pos;
			const double Dist = To.Length();
			double Speed = M.SpeedMps;
			if (Left > 0.2)
			{
				Speed = FMath::Clamp(Dist / Left, 0.5 * M.SpeedMps, 2.0 * M.SpeedMps);
			}
			Speed *= FMath::Clamp(Since / 1.5, 0.15, 1.0);  // gathering speed off the rail
			const Vec3 Dir = M.Vel.Normalized();
			const double Turn = (Since < 0.6 ? 0.2 : 1.2) * Dt;
			const Vec3 NewDir = simcore::TurnToward(Dir.LengthSquared() > 0.5 ? Dir : To.Normalized(), To.Normalized(), Turn);
			M.Vel = NewDir * Speed;
			M.Pos += M.Vel * Dt;
			if (Now > M.EndT + 6.0)
			{
				AddBurst(M.BurstStyle, M.Pos, M.ChargeKg, Now, M.Vel);  // long overdue: it destroys itself
				M.bDone = true;
				continue;
			}
		}
		const FVector PosU = ToU(M.Pos);
		FVector Fwd = (ToU(M.Pos + M.Vel.Normalized()) - PosU).GetSafeNormal();
		if (Fwd.IsNearlyZero())
		{
			Fwd = M.FwdU;
		}
		M.PosU = PosU;
		M.FwdU = Fwd;
		DrawMissile(M, PosU, Fwd, Now - M.LaunchT);
	}
}

// ------------------------------------------------------------------ bombs, rockets, gun runs

void FSimFxWorld::UpdateBombs()
{
	for (int32 I = Bombs.Num() - 1; I >= 0; --I)
	{
		FSimFxBomb& B = Bombs[I];
		const FSimFxMunition& Mun = *B.Mun;
		if (B.bDone)
		{
			ReleaseRibbon(B.Ribbon, B.Key);
			Bombs.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		if (!B.bReleased)
		{
			if (Now < B.ReleaseT)
			{
				continue;
			}
			// Each leaves from where its aircraft is at its own release instant.
			Vec3 Pos = B.FallbackPos, Vel = B.FallbackVel;
			if (const FSimEntityState* S = Entity(B.SourceId))
			{
				if (S->bAlive)
				{
					const FVector P = EntityPos(*S);
					Pos = ToE(P);
					Vel = ToE(P + S->Velocity) - Pos;
				}
			}
			simcore::fx::BombPathParams P;
			P.release_pos = Pos;
			P.release_vel = Vel;
			P.impact_pos = B.ImpactEnu;
			P.release_t_s = B.ReleaseT;
			P.impact_t_s = B.ImpactT;
			P.kind = B.PathKind;
			if (B.PathKind == simcore::fx::BombPathParams::Parachute)
			{
				P.chute_open_s = FMath::Max(B.PartT - B.ReleaseT, 0.1);
				if (B.ChuteDescentMps > 0.0)
				{
					P.chute_descent_mps = B.ChuteDescentMps;
				}
			}
			B.Path = simcore::fx::BombPath(P);
			B.bReleased = true;
			if (Mun.bCluster)
			{
				B.ClusterOpenT = Mun.OpensS > 0.0 ? B.ReleaseT + Mun.OpensS : simcore::fx::ClusterOpenTime(B.Path, Mun.OpensAglM > 0.0 ? Mun.OpensAglM : 500.0);
			}
		}
		const bool bCluster = Mun.bCluster;
		if (bCluster && !B.bClusterOpened && Now >= B.ClusterOpenT)
		{
			// The dispenser opens: its bomblets fall as a cloud onto the pattern, and burst there in a ripple.
			B.bClusterOpened = true;
			const Vec3 Open = B.Path.Position(B.ClusterOpenT);
			const FVector OpenU = ToU(Open);
			const double Fall = FMath::Max(B.ImpactT - B.ClusterOpenT, 0.5);
			Vec3 Track = simcore::fx::trajectory_detail::Horizontal(B.Path.Velocity(B.ClusterOpenT)).Normalized();
			if (Track.LengthSquared() < 0.5)
			{
				Track = Vec3{0.0, 1.0, 0.0};
			}
			const Vec3 Across{Track.y, -Track.x, 0.0};
			const double W = Mun.PatternWM > 0.0 ? Mun.PatternWM : 120.0;
			const double Len = Mun.PatternLM > 0.0 ? Mun.PatternLM : 240.0;
			const bool bSeen = Seen(OpenU, 30000.0) || Seen(ToU(B.ImpactEnu), 100.0 * Len);
			if (bSeen)
			{
				FSimFxParticle Pop;
				Pop.Layer = ESimFxLayer::Glow;
				Pop.Pos = OpenU;
				Pop.Life = 0.12f;
				Pop.Size0 = Pop.Size1 = 400.0f;
				Pop.Tint = Tune.Explosion.FlashColour * Brightness(10.0);
				Spawn(Pop);
				for (int32 K = 0; K < Tune.Bombs.ClusterCloudSprites; ++K)
				{
					const Vec3 Land = B.ImpactEnu + Track * (RandRange(-0.5, 0.5) * Len) + Across * (RandRange(-0.5, 0.5) * W);
					const FVector LandU = ToU(Land);
					FSimFxParticle P;
					P.Layer = ESimFxLayer::Debris;
					P.Pos = OpenU + RandUnit() * 300.0;
					P.Vel = (LandU - P.Pos) / Fall;
					P.Life = static_cast<float>(Fall);
					P.Size0 = P.Size1 = 60.0f;
					P.Tint = FLinearColor(0.12f, 0.12f, 0.1f);
					P.Frame0 = static_cast<float>(K % 16);
					P.FadeFrom = 0.95f;
					P.MinPx = 1.0f;
					P.Spin = static_cast<float>(RandRange(-6.0, 6.0));
					Spawn(P);
				}
			}
			// The weapon's own bomblets (LOOK 5: 42-202), rippling over the pattern in 1-2 s, then merging into a haze.
			const int32 BurstCount = Mun.Submunitions > 0 ? FMath::Clamp(Mun.Submunitions, 1, FMath::Max(Tune.Bombs.ClusterBurstsMax, 1))
				: FMath::Max(Tune.Bombs.ClusterBursts, 1);
			const double Charge = Mun.ChargeKg > 0.0 ? Mun.ChargeKg : Tune.Bombs.ClusterBurstKg;
			for (int32 K = 0; K < BurstCount; ++K)
			{
				const Vec3 Land = B.ImpactEnu + Track * (RandRange(-0.5, 0.5) * Len) + Across * (RandRange(-0.5, 0.5) * W);
				AddBurst(EFxBurstStyle::Small, GroundUnder(Land), Charge, B.ImpactT + RandRange(0.0, Tune.Bombs.ClusterSpreadS));
			}
			FSimFxBurst& Haze = AddBurst(EFxBurstStyle::Haze, B.ImpactEnu, Charge, B.ImpactT + 0.5 * Tune.Bombs.ClusterSpreadS);
			Haze.Dir = (ToU(B.ImpactEnu + Track) - ToU(B.ImpactEnu)).GetSafeNormal();
			Haze.HazeW = static_cast<float>(W);
			Haze.HazeL = static_cast<float>(Len);
		}
		if (Now >= B.ImpactT)
		{
			if (!bCluster)
			{
				FSimFxBurst& Burst = AddBurst(EFxBurstStyle::Ground, B.ImpactEnu, FMath::Max(B.ChargeKg, 1.0), B.ImpactT);
				Burst.bSoot = MunitionListed(Tune.Explosion.SootMunitions, Mun.Id.IsEmpty() ? B.Weapon : Mun.Id);
				if (B.bFire)
				{
					FSimFxFire& F = Fires.AddDefaulted_GetRef();
					F.Pos = ToU(B.ImpactEnu);
					F.Wind = WindAt(B.ImpactEnu);
					F.StartT = B.ImpactT;
					F.FireEndT = B.ImpactT + Tune.Wreck.GroundFireS;
					F.SmokeEndT = B.ImpactT + Tune.Wreck.ColumnS;
					F.SizeM = Tune.Wreck.FireM;
				}
			}
			B.bDone = true;
			continue;
		}
		if (bCluster && B.bClusterOpened)
		{
			continue;  // the dispenser is gone into its bomblets
		}
		const Vec3 Pos = B.Path.Position(Now);
		const Vec3 Fwd = B.Path.Forward(Now);
		const FVector PosU = ToU(Pos);
		const FVector FwdU = (ToU(Pos + Fwd) - PosU).GetSafeNormal();
		if (Mun.bMotor || Mun.Kind == TEXT("rocket") || Mun.Kind == TEXT("missile"))
		{
			FSimFxMissile Temp;
			Temp.Key = B.Key;
			Temp.Mun = B.Mun;
			Temp.Ribbon = B.Ribbon;
			Temp.LastTrailPos = B.LastTrailPos;
			Temp.bHaveTrail = B.bHaveTrail;
			Temp.bShowMesh = true;
			DrawMissile(Temp, PosU, FwdU, Now - B.ReleaseT);
			B.Ribbon = Temp.Ribbon;
			B.LastTrailPos = Temp.LastTrailPos;
			B.bHaveTrail = Temp.bHaveTrail;
			continue;
		}
		double Px = 0.0;
		TierAt(PosU, Mun.LengthM, &Px);
		if (Px > Tune.Lod.MunitionPx && Mun.bHasMesh)
		{
			const FRotator Rot = FwdU.Rotation();
			if (UStaticMesh* Mesh = FSimMunitionModels::Mesh(Mun.Id))
			{
				Meshes.Add({Mesh, PosU, Rot});
			}
			if (!B.Part.IsEmpty() && Now >= B.PartT)
			{
				if (UStaticMesh* Part = FSimMunitionModels::PartMesh(Mun.Id, B.Part))
				{
					Meshes.Add({Part, PosU, Rot});
				}
			}
		}
	}
}

void FSimFxWorld::UpdateGunRuns()
{
	const FSimFxTuning::FGuns& T = Tune.Guns;
	for (int32 I = GunRuns.Num() - 1; I >= 0; --I)
	{
		FSimFxGunRun& G = GunRuns[I];
		if (Now > G.ImpactEndT + 0.5 && Now > G.FireEndT)
		{
			GunRuns.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		const FSimFxMunition& Mun = *G.Mun;
		// The muzzle: the aircraft's nose while it fires.
		if (Now >= G.StartT && Now < G.FireEndT)
		{
			FVector Nose = ToU(G.Muzzle0 + G.MuzzleVel * (Now - G.StartT));
			FVector Fwd = (ToU(G.Muzzle0 + G.MuzzleVel) - ToU(G.Muzzle0)).GetSafeNormal();
			if (const FSimEntityState* S = Entity(G.SourceId))
			{
				if (S->bAlive)
				{
					Fwd = S->Velocity.GetSafeNormal();
					Nose = EntityPos(*S) + Fwd * 700.0;
				}
			}
			FSimFxSprite F;
			F.Layer = ESimFxLayer::Glow;
			F.Pos = Nose;
			F.Width = F.Length = static_cast<float>(100.0 * Tune.Bombs.MuzzleFlashM) * SimFxEffFlicker(Now, I);
			F.Tint = T.TracerColour * Brightness(T.MuzzleBrightness);
			F.MinPx = 1.0f;
			AddSprite(F);
			if (Sun.bNight)
			{
				AddLight(Nose, T.TracerColour, T.MuzzleLightLumens, 3000.0);
			}
			if (Mun.TracerS > 0.0 && Dt > 0.0f)
			{
				// Tracers toward the line of impacts.
				const int32 N = FMath::Max(1, FMath::RoundToInt(Dt * 20.0f));
				const double Mps = Mun.MuzzleMps > 0.0 ? Mun.MuzzleMps : T.DefaultMuzzleMps;
				const double LengthCm = SimFxLogic::TracerLengthCm(Mps * 100.0, Dt, T.TracerMinFrameS);
				// The munition's colour at night, the day's yellow-white for a warm one by day (LOOK 14), brighter by day against
				// the sky.
				const FLinearColor Lit = SimFxLogic::TracerColourFor(Mun.TracerColour, Mun.bTracerColour, T.TracerDayColour, T.TracerNightColour, Sun.ElevationDeg) *
					Brightness(FMath::Lerp(T.TracerDayBrightness, T.TracerBrightness, static_cast<double>(SimFxLogic::NightShare(Sun.ElevationDeg))));
				for (int32 K = 0; K < N; ++K)
				{
					const double F01 = FMath::Clamp((Now - G.StartT) / FMath::Max(G.FireEndT - G.StartT, 0.1), 0.0, 1.0);
					const FVector Hit = ToU(G.From + (G.To - G.From) * F01);
					FSimFxParticle P;
					P.Layer = ESimFxLayer::Streak;
					P.Facing = ESimFxFacing::Axis;
					P.Pos = Nose;
					P.Vel = (Hit - Nose).GetSafeNormal() * Mps * 100.0;
					P.Life = static_cast<float>(FMath::Min(Mun.TracerS, FVector::Dist(Hit, Nose) / (Mps * 100.0)));
					P.Size0 = P.Size1 = static_cast<float>(100.0 * T.TracerWidthM);
					P.Stretch = static_cast<float>(LengthCm / (100.0 * T.TracerWidthM));
					P.Tint = Lit;
					P.MinPx = static_cast<float>(T.TracerMinPx);
					P.FadeFrom = 0.9f;
					Spawn(P);
				}
			}
		}
		// The impacts walking along the ground.
		const double Rate = FMath::Min(FMath::Max(Mun.RateRpm, 600.0) / 60.0, 25.0);
		while (G.NextImpactT <= Now && G.NextImpactT <= G.ImpactEndT)
		{
			const double F01 = (G.NextImpactT - G.ImpactStartT) / FMath::Max(G.ImpactEndT - G.ImpactStartT, 0.1);
			const Vec3 At = G.From + (G.To - G.From) * F01 + Vec3{RandRange(-5.0, 5.0), RandRange(-5.0, 5.0), 0.0};
			const FVector AtU = ToU(GroundUnder(At));
			if (Seen(AtU, 1000.0) && DistToCamM(AtU) < Lod.MaxRangeM)
			{
				const float Age = static_cast<float>(Now - G.NextImpactT);
				FSimFxParticle D;
				D.Layer = ESimFxLayer::Smoke;
				D.Pos = AtU + FVector(0.0, 0.0, 100.0);
				D.Vel = FVector(0.0, 0.0, 300.0);
				D.Age = Age;
				D.Life = static_cast<float>(RandRange(3.0, 6.0));
				D.Size0 = static_cast<float>(Tune.Bombs.GunDustM * 30.0);
				D.Size1 = static_cast<float>(Tune.Bombs.GunDustM * 100.0);
				D.Tint = Tune.Explosion.DustColour;
				D.Opacity = 0.85f;
				D.Frame0 = static_cast<float>(FMath::RandRange(0, 15));
				D.Drag = 1.5f;
				D.FadeFrom = 0.4f;
				Spawn(D);
				FSimFxParticle S;
				S.Layer = ESimFxLayer::Spark;
				S.Pos = AtU + FVector(0.0, 0.0, 30.0);
				S.Age = Age;
				S.Life = 0.08f;
				S.Size0 = S.Size1 = 150.0f;
				S.Tint = Tune.Explosion.SparkColour * Brightness(Tune.Explosion.SparkBrightness);
				Spawn(S);
			}
			G.NextImpactT += 1.0 / Rate;
		}
	}
}

// ------------------------------------------------------------------ the guns' passes

void FSimFxWorld::UpdateGunPasses()
{
	const FSimFxTuning::FGuns& T = Tune.Guns;
	for (int32 I = GunPasses.Num() - 1; I >= 0; --I)
	{
		FSimFxGunPass& G = GunPasses[I];
		const FSimFxMunition& Mun = *G.Mun;
		const double Mps = Mun.MuzzleMps > 0.0 ? Mun.MuzzleMps : T.DefaultMuzzleMps;
		const double Calibre = Mun.CalibreMm > 0.0 ? Mun.CalibreMm : 23.0;
		const double DragK = simcore::fx::kDragKTypical23mm * 23.0 / Calibre;
		simcore::fx::RoundParams Round;
		Round.muzzle_mps = Mps;
		Round.drag_k = DragK;
		Round.tracer_s = Mun.TracerS > 0.0 ? Mun.TracerS : (Mun.TracerS < 0.0 && !G.bFlak ? 5.0 : 0.0);
		Round.sd_s = 0.0;
		// Firing: bursts with pauses between, while the pass lasts.
		const double Rate = FMath::Max(Mun.RateRpm > 0.0 ? Mun.RateRpm : T.DefaultRateRpm, 30.0) / 60.0;
		const FSimEntityState* Tgt = G.TargetId >= 0 ? Entity(G.TargetId) : EntityByName(G.TargetName);
		Vec3 TgtPos = G.AimFallback, TgtVel;
		if (Tgt != nullptr && Tgt->bAlive)
		{
			const FVector P = EntityPos(*Tgt);
			TgtPos = ToE(P);
			TgtVel = ToE(P + Tgt->Velocity) - TgtPos;
		}
		// Which of the belt's rounds carry a tracer (LOOK 14: the Shilka's 1 in 4, the ZU-23's 1 in 5); the rest unseen.
		int32 Every = T.TracedEveryDefault;
		for (const TPair<FString, int32>& Each : T.TracedEvery)
		{
			if (!Each.Key.IsEmpty() && (Mun.Id.StartsWith(Each.Key, ESearchCase::IgnoreCase) || G.Weapon.StartsWith(Each.Key, ESearchCase::IgnoreCase)))
			{
				Every = Each.Value;
				break;
			}
		}
		// The mount's barrels side by side across the line of fire.
		Vec3 Across = simcore::Cross(TgtPos - G.Site, Vec3{0.0, 0.0, 1.0});
		Across = Across.LengthSquared() > 1e-6 ? Across.Normalized() : Vec3{1.0, 0.0, 0.0};
		const int32 Barrels = FMath::Max(Mun.Barrels, 1);
		bool bFiring = false;
		while (G.NextShotT <= Now && G.NextShotT < G.T1)
		{
			if (G.NextShotT >= G.BurstEndT)
			{
				// A pause, then the next burst (or the pass is done firing).
				if (++G.BurstIndex >= G.Bursts.Num())
				{
					G.NextShotT = G.T1;
					break;
				}
				G.NextBurstT = G.Bursts[G.BurstIndex].StartS;
				G.NextShotT = G.NextBurstT;
				G.BurstEndT = G.Bursts[G.BurstIndex].EndS;
				G.bBurstSmoked = false;
				continue;
			}
			bFiring = true;
			const bool bTraced = SimFxLogic::IsTracedRound(G.RoundIndex++, Every);
			const bool bSeenRound = (bTraced && Round.tracer_s > 0.0) || G.bFlak || Mun.SdHiS > 0.0;
			if (bSeenRound && G.Rounds.Num() < T.MaxRounds)
			{
				FSimFxRound R;
				R.FireT = G.NextShotT;
				const double Side = Barrels > 1 ? (G.Barrel % Barrels - 0.5 * (Barrels - 1)) * T.BarrelSpacingM : 0.0;
				R.Muzzle = G.Site + Across * Side;
				double Tof = 0.0;
				Vec3 Dir = simcore::fx::LeadAim(R.Muzzle, TgtPos, TgtVel, Mps, DragK, &Tof);
				const double Mrad = T.DispersionMrad * 1e-3;
				const Vec3 Up{0.0, 0.0, 1.0};
				Vec3 Right = simcore::Cross(Dir, Up).Normalized();
				if (Right.LengthSquared() < 0.5)
				{
					Right = Vec3{1.0, 0.0, 0.0};
				}
				const Vec3 Upish = simcore::Cross(Right, Dir).Normalized();
				const double A = (SimFxEffRand(G.Rng) + SimFxEffRand(G.Rng) - 1.0) * Mrad;
				const double B = (SimFxEffRand(G.Rng) + SimFxEffRand(G.Rng) - 1.0) * Mrad;
				R.Dir = (Dir + Right * A + Upish * B).Normalized();
				if (G.bFlak)
				{
					R.BurstT = FMath::Max(Tof + (SimFxEffRand(G.Rng) * 2.0 - 1.0) * T.FlakJitterS, 0.5);
				}
				else if (Mun.SdHiS > 0.0)
				{
					R.SdT = Mun.SdLoS + (Mun.SdHiS - Mun.SdLoS) * SimFxEffRand(G.Rng);
				}
				R.bTracer = bTraced && Round.tracer_s > 0.0;
				G.Rounds.Add(R);
			}
			G.Barrel++;
			G.NextShotT += 1.0 / Rate;
		}
		if (bFiring || (Now >= G.T0 && Now < G.BurstEndT && Now < G.T1 && Now >= G.NextBurstT - 1e-3))
		{
			// A flash at each barrel's muzzle, 1-2 m, #FFC88C (LOOK 14), and at night the mount's light.
			const FLinearColor MuzzleColour(1.0f, 0.578f, 0.262f);
			const FVector AcrossU = ToU(G.Site + Across) - G.SiteU;
			const FVector AimU = (ToU(TgtPos) - G.SiteU).GetSafeNormal();
			for (int32 B = 0; B < Barrels; ++B)
			{
				const double Side = Barrels > 1 ? (B - 0.5 * (Barrels - 1)) * T.BarrelSpacingM : 0.0;
				FSimFxSprite F;
				F.Layer = ESimFxLayer::Glow;
				F.Pos = G.SiteU + FVector(0.0, 0.0, 150.0) + AcrossU * Side + AimU * 150.0;
				F.Width = F.Length = static_cast<float>(100.0 * T.MuzzleFlashM * (G.bFlak ? 2.0 : 1.0)) * SimFxEffFlicker(Now, I * 3.0 + B * 1.37);
				F.Tint = MuzzleColour * Brightness(T.MuzzleBrightness);
				F.MinPx = B == 0 ? 1.5f : 0.0f;
				AddSprite(F);
			}
			if (Sun.bNight)
			{
				AddLight(G.SiteU + FVector(0.0, 0.0, 150.0), MuzzleColour, T.MuzzleLightLumens * (G.bFlak ? 2.0 : 1.0), 2500.0);
			}
			// Each burst's gun smoke, pale grey, drifting off the mount on the wind (LOOK 14: 2-4 m, 4-8 s).
			if (!G.bBurstSmoked)
			{
				G.bBurstSmoked = true;
				if (Seen(G.SiteU, 1000.0) && DistToCamM(G.SiteU) < Lod.MaxRangeM)
				{
					const FVector Wind = WindAt(G.Site);
					for (int32 K = 0; K < T.SmokePuffs; ++K)
					{
						FSimFxParticle P;
						P.Layer = ESimFxLayer::Smoke;
						P.Pos = G.SiteU + FVector(0.0, 0.0, 180.0) + AimU * RandRange(100.0, 300.0) + AcrossU * RandRange(-80.0, 80.0);
						P.Vel = AimU * 250.0 + FVector(0.0, 0.0, 40.0);
						P.Wind = Wind;
						P.Life = static_cast<float>(T.SmokeS * RandRange(0.67, 1.33));
						P.Size0 = static_cast<float>(T.SmokeM * 35.0);
						P.Size1 = static_cast<float>(T.SmokeM * 100.0 * RandRange(0.67, 1.33));
						P.Tint = T.SmokeColour;
						P.Opacity = static_cast<float>(T.SmokeOpacity);
						P.Frame0 = static_cast<float>(FMath::RandRange(0, 15));
						P.Drag = 1.0f;
						P.FadeInS = 0.05f;
						P.FadeFrom = 0.4f;
						P.Spin = static_cast<float>(RandRange(-0.2, 0.2));
						Spawn(P);
					}
				}
			}
		}
		// The rounds: tracers while they burn, a puff where they destroy themselves, a flak burst at the fuze's time.
		// The tracer white-yellow by day, red-orange at night (LOOK 14), a frame's flight long (the eye's streak: 16 m at 60 Hz).
		// The munition's own colour is its night colour; a warm one turns the day's yellow-white by day (LOOK 14: the Shilka's
		// orange was drawn by day as faint salmon dots), and brighter by day against the sky.
		const FLinearColor TracerTint = SimFxLogic::TracerColourFor(Mun.TracerColour, Mun.bTracerColour, T.TracerDayColour, T.TracerNightColour, Sun.ElevationDeg) *
			Brightness(FMath::Lerp(T.TracerDayBrightness, T.TracerBrightness, static_cast<double>(SimFxLogic::NightShare(Sun.ElevationDeg))));
		for (int32 K = G.Rounds.Num() - 1; K >= 0; --K)
		{
			FSimFxRound& R = G.Rounds[K];
			const double Age = Now - R.FireT;
			if (R.BurstT > 0.0 && Age >= R.BurstT)
			{
				const Vec3 At = simcore::fx::RoundAt(R.Muzzle, R.Dir, Round, R.BurstT).position + Vec3{0.0, 0.0, 0.0};
				const Vec3 Off{(SimFxEffRand(G.Rng) - 0.5) * 2.0 * T.FlakOffsetM, (SimFxEffRand(G.Rng) - 0.5) * 2.0 * T.FlakOffsetM,
					(SimFxEffRand(G.Rng) - 0.5) * 2.0 * T.FlakOffsetM};
				AddBurst(EFxBurstStyle::Flak, At + Off, FMath::Max(Mun.ChargeKg, 0.1), R.FireT + R.BurstT).CalibreMm = Mun.CalibreMm;
				G.Rounds.RemoveAtSwap(K, EAllowShrinking::No);
				continue;
			}
			if (R.SdT > 0.0 && Age >= R.SdT)
			{
				const Vec3 At = simcore::fx::RoundAt(R.Muzzle, R.Dir, Round, R.SdT).position;
				AddBurst(EFxBurstStyle::SdPuff, At, FMath::Max(Mun.ChargeKg, 0.02), R.FireT + R.SdT).CalibreMm = Mun.CalibreMm;
				G.Rounds.RemoveAtSwap(K, EAllowShrinking::No);
				continue;
			}
			const double MaxAge = FMath::Max3(Round.tracer_s, R.SdT, R.BurstT) + 0.1;
			if (Age > MaxAge && Age > 0.2)
			{
				G.Rounds.RemoveAtSwap(K, EAllowShrinking::No);
				continue;
			}
			if (R.bTracer && Age >= 0.0 && Age < Round.tracer_s)
			{
				const simcore::fx::RoundState St = simcore::fx::RoundAt(R.Muzzle, R.Dir, Round, Age);
				const FVector P = ToU(St.position);
				const FVector V = ToU(St.position + St.velocity) - P;
				const double Speed = V.Size();
				FSimFxSprite S;
				S.Layer = ESimFxLayer::Streak;
				S.Facing = ESimFxFacing::Axis;
				S.Axis = Speed > 1.0 ? V / Speed : FVector::UpVector;
				S.Width = static_cast<float>(100.0 * T.TracerWidthM);
				S.Length = static_cast<float>(FMath::Max(SimFxLogic::TracerLengthCm(Speed, Dt, T.TracerMinFrameS), 4.0 * S.Width));
				S.Pos = P - S.Axis * (0.5 * S.Length);
				S.Tint = TracerTint;
				S.MinPx = static_cast<float>(T.TracerMinPx);
				AddSprite(S);
			}
		}
		if (Now >= G.T1 && G.Rounds.Num() == 0)
		{
			GunPasses.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
}

// ------------------------------------------------------------------ wrecks and fires

void FSimFxWorld::UpdateWrecks()
{
	const FSimFxTuning::FWreck& T = Tune.Wreck;
	for (int32 I = WreckFx.Num() - 1; I >= 0; --I)
	{
		FSimFxWreckFx& W = WreckFx[I];
		FSimFxWrecks::FWreck* Wr = Wrecks.Find(W.Id);
		if (Wr == nullptr)
		{
			WreckFx.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		const double Since = Now - Wr->StartT;
		const double Ground = Wr->Fall.GroundTime();
		if (Since >= Ground)
		{
			if (!W.bLanded)
			{
				W.bLanded = true;
				Wr->bLanded = true;
				ReleaseRibbon(W.Ribbon, W.Key);
				W.Ribbon = -1;
				const Vec3 At = Wr->Fall.Position(Ground);
				AddBurst(EFxBurstStyle::Ground, At, T.GroundChargeKg, Wr->StartT + Ground);
				FSimFxFire& F = Fires.AddDefaulted_GetRef();
				F.Pos = ToU(At);
				F.Wind = WindAt(At);
				F.StartT = Wr->StartT + Ground;
				F.FireEndT = F.StartT + (Wr->bFire ? T.GroundFireS : T.GroundFireS * 0.3);
				F.SmokeEndT = F.StartT + T.ColumnS;
				F.SizeM = T.FireM * 1.5;
			}
			continue;
		}
		if (Since < 0.0)
		{
			continue;
		}
		const Vec3 Pos = Wr->Fall.Position(Since);
		const FVector PosU = ToU(Pos);
		// Black smoke behind it all the way down (within the effects' range and the visibility: docs/fx/README.md 4.1).
		if (DistToCamM(PosU) < TrailRangeM(false))
		{
			TrailPoint(W.Ribbon, W.Key, W.LastTrailPos, W.bHaveTrail, PosU, static_cast<float>(T.SmokeWidthM * 100.0), static_cast<float>(T.SmokeOpacity),
				T.SmokePointM, WreckTrailStyle(), WindAt(Pos));
		}
		else if (W.Ribbon >= 0)
		{
			ReleaseRibbon(W.Ribbon, W.Key);
			W.Ribbon = -1;
			W.bHaveTrail = false;
		}
		if (Wr->bFire)
		{
			// The fire streams aft from the wing root or the tail (LOOK 11): a torch 15-50 m long drawn each frame at the
			// wreck, and flames shed into the slipstream, slipping back 5-15 m/s.
			const ESimFxTier Tier = TierAt(PosU, T.TorchLengthM);
			const FVector Vel = ToU(Pos + Wr->Fall.Velocity(Since)) - PosU;
			const FVector Aft = Vel.SizeSquared() > 100.0 ? -Vel.GetSafeNormal() : FVector::DownVector;
			if (Tier == ESimFxTier::Full || Tier == ESimFxTier::Impostor)
			{
				// The flame streaming aft as one motor-flame sprite (the plume picture: a long flame, hot at its root), with
				// a shorter, wider one over its first half flickering apart (the second art review's item 5: one round
				// flipbook frame stretched over 40 m showed a 7 m flame). #FFA64A at the root cooling to #FF8100.
				const float Flicker = SimFxEffFlicker(Now, W.Id * 1.3);
				const double TorchCm = T.TorchLengthM * 100.0 * Flicker;
				for (int32 K = 0; K < 2; ++K)
				{
					const double LengthCm = TorchCm * (K == 0 ? 1.0 : 0.55);
					FSimFxSprite Torch;
					Torch.Layer = ESimFxLayer::Plume;
					Torch.Facing = ESimFxFacing::Axis;
					Torch.Axis = Aft;
					Torch.Length = static_cast<float>(LengthCm);
					Torch.Width = static_cast<float>(T.TorchWidthM * 100.0) * (K == 0 ? 0.8f : 1.2f) *
						(0.85f + 0.3f * SimFxEffFlicker(Now, W.Id * 1.3 + K * 2.1));
					Torch.Pos = PosU + Aft * (0.5 * LengthCm - 100.0);
					Torch.Tint = (K == 0 ? FLinearColor(1.0f, 0.38f, 0.07f) : FLinearColor(1.0f, 0.22f, 0.0f)) *
						Brightness(T.FireBrightness * (Sun.bNight ? T.TorchNightShare : 0.3) * (K == 0 ? 1.0 : 0.6));
					Torch.MinPx = K == 0 ? 1.5f : 0.0f;
					AddSprite(Torch);
				}
			}
			W.FireAccum += Dt * T.FireRate * (Tier == ESimFxTier::Full ? 1.0 : 0.3);
			while (W.FireAccum >= 1.0)
			{
				W.FireAccum -= 1.0;
				if (Tier == ESimFxTier::None || Tier == ESimFxTier::Glint || Tier == ESimFxTier::Speck)
				{
					continue;
				}
				FSimFxParticle P;
				P.Layer = ESimFxLayer::Fire;
				P.Pos = PosU + Aft * RandRange(200.0, 0.6 * T.TorchLengthM * 100.0) + RandUnit() * 150.0;
				P.Vel = Vel * 0.95 + Aft * RandRange(500.0, 1500.0);
				P.Wind = Vel;
				P.Life = static_cast<float>(RandRange(0.4, 0.8));
				P.Size0 = static_cast<float>(T.FireM * 100.0 * RandRange(0.5, 0.9));
				P.Size1 = P.Size0 * 1.8f;
				P.Heat0 = 1.2f;
				P.Heat1 = 0.1f;
				P.Frame0 = static_cast<float>(FMath::RandRange(4, 12));
				P.Frames = 30.0f;
				P.Drag = 1.0f;
				P.Kind = ESimFxKind::HeatFlipbook;
				P.Tint = FLinearColor(0.05f, 0.045f, 0.04f);  // the soot's albedo, lit in the material
				P.Roll = static_cast<float>(RandRange(0.0, 6.28));
				Spawn(P);
			}
			FSimFxSprite G;
			G.Layer = ESimFxLayer::Glow;
			G.Pos = PosU + Aft * (0.25 * T.TorchLengthM * 100.0);
			G.Width = G.Length = static_cast<float>(T.FireM * 160.0);
			// #FFA64A-#FF8100 kept as a hue (the art review's K4: a white disc at night lost it).
			G.Tint = FLinearColor(1.0f, 0.38f, 0.07f) * Brightness(T.FireBrightness * (Sun.bNight ? T.TorchNightShare : 0.3));
			G.MinPx = 1.5f;
			AddSprite(G);
			if (Sun.bNight)
			{
				// The wreck's own smoke trail, 1-5 m from the light, clips to a cream-white cigar at night under a strong one and
				// hides the torch's hue (night pictures, polish_7 at 5e7 lm, art-polish-2 at 2e7): wreck.night_light_lumens. From
				// the torch's middle, not inside the wreck's basic model, which it lit as a white block.
				AddLight(PosU + Aft * (0.5 * T.TorchLengthM * 100.0), FLinearColor(1.0f, 0.55f, 0.25f), T.NightLightLumens, 6000.0);
			}
		}
	}
	// Wrecks whose fall and burning are long done are forgotten (GetWreckPose then says nothing more of them).
	TArray<FSimFxWrecks::FWreck>& All = Wrecks.Items();
	for (int32 I = All.Num() - 1; I >= 0; --I)
	{
		if (Now - All[I].StartT > All[I].Fall.GroundTime() + 3600.0)
		{
			All.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
}

void FSimFxWorld::UpdateFires()
{
	const FSimFxTuning::FWreck& T = Tune.Wreck;
	for (int32 I = Fires.Num() - 1; I >= 0; --I)
	{
		FSimFxFire& F = Fires[I];
		if (Now > F.SmokeEndT)
		{
			Fires.RemoveAtSwap(I, EAllowShrinking::No);
			continue;
		}
		if (Now < F.StartT)
		{
			continue;
		}
		const bool bBurning = Now < F.FireEndT;
		const float Fade = static_cast<float>(1.0 - FMath::Clamp((Now - F.StartT) / FMath::Max(F.SmokeEndT - F.StartT, 1.0), 0.0, 1.0));
		const FVector Top = F.Pos + FVector(0.0, 0.0, 3000.0);
		const ESimFxTier Tier = TierAt(Top, 60.0);
		if (Tier == ESimFxTier::None)
		{
			continue;  // unseen: nothing animated
		}
		if (bBurning && Tier != ESimFxTier::Glint && Tier != ESimFxTier::Speck)
		{
			F.FireAccum += Dt * T.FireRate * (Tier == ESimFxTier::Full ? 1.0 : 0.25);
			while (F.FireAccum >= 1.0)
			{
				F.FireAccum -= 1.0;
				FSimFxParticle P;
				P.Layer = ESimFxLayer::Fire;
				P.Pos = F.Pos + FVector(RandRange(-1.0, 1.0), RandRange(-1.0, 1.0), 0.2) * (F.SizeM * 40.0);
				P.Vel = FVector(0.0, 0.0, RandRange(200.0, 500.0));
				P.Wind = F.Wind;
				P.Life = static_cast<float>(RandRange(0.7, 1.3));
				P.Size0 = static_cast<float>(F.SizeM * 100.0 * RandRange(0.5, 0.9)) * (Tier == ESimFxTier::Full ? 1.0f : 1.6f);
				P.Size1 = P.Size0 * 1.5f;
				P.Heat0 = 1.1f;
				P.Heat1 = 0.1f;
				P.Frame0 = static_cast<float>(FMath::RandRange(6, 14));
				P.Frames = 28.0f;
				P.Drag = 1.0f;
				P.Kind = ESimFxKind::HeatFlipbook;
				P.Tint = FLinearColor(0.05f, 0.045f, 0.04f);  // the soot's albedo, lit in the material
				P.Roll = static_cast<float>(RandRange(0.0, 6.28));
				Spawn(P);
			}
			if (Sun.bNight)
			{
				AddLight(F.Pos + FVector(0.0, 0.0, 300.0), FLinearColor(1.0f, 0.5f, 0.2f), 4.0e7, 8000.0);
				FSimFxSprite G;
				G.Layer = ESimFxLayer::Glow;
				G.Pos = F.Pos + FVector(0.0, 0.0, 200.0);
				G.Width = G.Length = static_cast<float>(F.SizeM * 200.0);
				G.Tint = FLinearColor(1.0f, 0.45f, 0.15f) * Brightness(T.FireBrightness * 0.2);
				G.MinPx = 2.0f;
				AddSprite(G);
			}
		}
		if (Tier == ESimFxTier::Glint)
		{
			continue;
		}
		// The column: big dark puffs rising and leaning with the wind (far: fewer, bigger).
		const bool bFar = Tier != ESimFxTier::Full;
		F.SmokeAccum += Dt * T.ColumnRate * Fade * (bFar ? 0.35 : 1.0);
		while (F.SmokeAccum >= 1.0)
		{
			F.SmokeAccum -= 1.0;
			FSimFxParticle P;
			P.Layer = ESimFxLayer::Smoke;
			P.Pos = F.Pos + FVector(RandRange(-1.0, 1.0), RandRange(-1.0, 1.0), 0.5) * (F.SizeM * 60.0);
			P.Vel = FVector(0.0, 0.0, T.ColumnRiseMps * 100.0 * RandRange(0.7, 1.3));
			P.Wind = F.Wind + FVector(0.0, 0.0, T.ColumnRiseMps * 60.0);
			P.Life = static_cast<float>(T.ColumnPuffS * RandRange(0.7, 1.2));
			P.Size0 = static_cast<float>(T.ColumnPuffM * 100.0 * (bFar ? 2.0 : 1.0));
			P.Size1 = P.Size0 * static_cast<float>(T.ColumnGrow);
			P.Opacity = 0.9f * FMath::Max(Fade, 0.2f);
			P.FadeInS = 1.0f;
			P.FadeFrom = 0.5f;
			P.Tint = T.ColumnColour;
			P.Frame0 = static_cast<float>(FMath::RandRange(0, 15));
			P.Drag = 0.3f;
			P.Heat0 = bBurning ? 0.25f : 0.0f;
			P.Heat1 = 0.0f;
			P.HeatTauS = 1.0f;  // lit by the flames only near them: a puff glowing 30 m up read as an orange disc in the column
			P.Roll = static_cast<float>(RandRange(0.0, 6.28));
			P.Spin = static_cast<float>(RandRange(-0.05, 0.05));
			Spawn(P);
		}
	}
}

// ------------------------------------------------------------------ aircraft: contrails and afterburners

void FSimFxWorld::UpdateAircraft()
{
	for (TPair<int32, FSimFxAircraftFx>& Pair : Aircraft)
	{
		Pair.Value.bSeen = false;
	}
	const FSimFxTuning::FContrail& C = Tune.Contrail;
	const FSimFxTuning::FAfterburner& AB = Tune.Afterburner;
	if (States != nullptr && Runner != nullptr)
	{
		for (const FSimEntityState& S : *States)
		{
			if (S.bIsMissile || S.bIsGround || !S.bAlive || S.Id < 0 || S.Id >= SimFxIds::MissileBase)
			{
				continue;
			}
			if (Wrecks.Find(S.Id) != nullptr)
			{
				continue;
			}
			FSimFxAircraftFx& A = Aircraft.FindOrAdd(S.Id);
			A.bSeen = true;
			if (A.Id != S.Id || A.Model != S.Model)
			{
				A.Id = S.Id;
				A.Model = S.Model;
				A.Nozzles = NozzlesOf(S.Model);
				A.NextEvalT = -1.0;
				const simcore::scenery::ModelLibrary* Lib = FSimBasicModels::Library();
				const simcore::scenery::ModelDef* Def = Lib != nullptr ? Lib->Find(std::string(TCHAR_TO_UTF8(*S.Model))) : nullptr;
				A.SpanM = Def != nullptr && Def->span_m > 0.0 ? Def->span_m : 10.0;
				A.bBlueFlame = MunitionListed(AB.BlueModels, S.Model);
			}
			if (A.Nozzles.Num() == 0)
			{
				continue;
			}
			const FVector Pos = EntityPos(S);
			const FVector VelU = S.Velocity;
			const double SpeedMps = VelU.Size() / 100.0;
			if (SpeedMps < 1.0)
			{
				continue;
			}
			const FVector Fwd = VelU / (SpeedMps * 100.0);
			const FRotator Rot = Fwd.Rotation();
			const Vec3 Enu = ToE(Pos);
			const double Alt = AltitudeOf(Enu);
			if (Now >= A.NextEvalT)
			{
				A.NextEvalT = Now + C.EvalS;
				A.State = simcore::fx::ContrailAt(Runner->GetWeather(), Alt, C.Eta, C.MaxS);
			}
			// Contrails: one ribbon an engine (as many as the pool gives), starting a short gap behind the nozzles; each
			// plume rolls up into the wingtip vortex on its side within seconds, the pair sinks, and later the Crow
			// instability pinches it into rings (LOOK 12).
			const int32 Engines = FMath::Min(A.Nozzles.Num(), FMath::Clamp(C.EnginesMax, 1, 4));
			// Laid out to twice the effects' range in clear air (the far line), never past the weather's visibility.
			const bool bTrail = A.State.kind != simcore::fx::ContrailKind::None && DistToCamM(Pos) < TrailRangeM(true);
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal(UE_SMALL_NUMBER, Rot.RotateVector(FVector::RightVector));
			for (int32 E = 0; E < 4; ++E)
			{
				const uint64 Key = ContrailKey(S.Id, E);
				if (!bTrail || E >= Engines)
				{
					if (A.Ribbons[E] >= 0)
					{
						ReleaseRibbon(A.Ribbons[E], Key);
						A.Ribbons[E] = -1;
					}
					continue;
				}
				// More engines than the model's first four: the rest share the ribbon nearest.
				const FVector Nozzle = Pos + Rot.RotateVector(A.Nozzles[E]) - Fwd * (C.GapM * 100.0);
				const bool bPersistent = A.State.kind == simcore::fx::ContrailKind::Persistent;
				const float Life = static_cast<float>(FMath::Min(A.State.lifetime_s, C.MaxS));
				bool bHave = A.Ribbons[E] >= 0;
				// Where this plume rolls to: the vortex on its side, 0.39 span out (a single engine on the axis stays).
				const double NozzleSide = FVector::DotProduct(Rot.RotateVector(A.Nozzles[E]), Across);
				const double Vortex = FMath::Abs(NozzleSide) > 5.0 ? FMath::Sign(NozzleSide) * C.VortexSpanShare * A.SpanM * 100.0 : 0.0;
				const FVector Side = Across * (Vortex - NozzleSide);
				TrailPoint(A.Ribbons[E], Key, A.LastPoint[E], bHave, Nozzle, static_cast<float>(C.WidthM * 100.0),
					static_cast<float>(bPersistent ? C.Opacity : C.ShortOpacity), FMath::Max(C.GapM, SpeedMps * C.PointS),
					ContrailStyle(Life, A.State.spread_mps * C.SpreadScale, A.SpanM), WindAt(Enu), Side);
			}
			// Afterburner: supersonic or climbing hard.
			const double SoundMps = Runner->GetWeather().Air(Alt).speed_of_sound_mps;
			const double Climb = (ToE(Pos + VelU) - Enu).z;
			if (SpeedMps > AB.Mach * SoundMps || Climb > AB.ClimbMps)
			{
				// By day a short pale-orange flame, nearly lost in the sunlight; at night 5-8 m, orange (Soviet engines) or
				// blue-violet (the F100's), with its shock diamonds bright knots along it (LOOK 13).
				const float Night = SimFxLogic::NightShare(Sun.ElevationDeg);
				const FLinearColor NightColour = A.bBlueFlame ? AB.NightBlueColour : AB.NightColour;
				const FLinearColor Colour = FMath::Lerp(AB.DayColour, NightColour, Night);
				const float LengthCm = static_cast<float>(100.0 * FMath::Lerp(AB.DayLengthM, AB.NightLengthM, static_cast<double>(Night)));
				// The night's flame against the day's: its brightness stays what the plume's is at night, its day share small.
				const float Bright = Brightness(FMath::Lerp(AB.DayBrightness, AB.Brightness, static_cast<double>(Night)));
				const float Flick = SimFxEffFlicker(Now, S.Id);
				// Seen as a point (far, or at night past a few pixels), the flame is one glint: LOOK 13, at night seen to
				// 15 km and beyond (the coordinator's look of 1 Nov: at 2.4 km it was nothing).
				double FlamePx = 0.0;
				TierAt(Pos, LengthCm / 100.0, &FlamePx);
				const bool bPoint = FlamePx < AB.PointPx;
				for (const FVector& N : A.Nozzles)
				{
					const FVector At = Pos + Rot.RotateVector(N);
					if (!bPoint)
					{
						// The flame: a nozzle-wide column tapering aft (T_FX_Afterburner, not the rocket plume's flaring cone,
						// which drew a sausage wider than the nozzles: the coordinator's look of 1 Nov).
						FSimFxSprite P;
						P.Layer = ESimFxLayer::Plume;
						P.Kind = ESimFxKind::Afterburner;
						P.Facing = ESimFxFacing::Axis;
						P.Axis = -Fwd;
						P.Length = LengthCm * Flick;
						P.Width = static_cast<float>(AB.WidthM * 100.0);
						P.Pos = At - Fwd * (0.5 * P.Length);
						P.Tint = Colour * Bright;
						AddSprite(P);
					}
					FSimFxSprite G;
					G.Layer = ESimFxLayer::Glow;
					// Its glow a little aft of the nozzle (the second art review's item 4: centred on it, the nozzle hid its
					// middle and it read as a ring), no wider than the nozzle's flame.
					G.Pos = At - Fwd * (100.0 * AB.GlowAftM);
					G.Width = G.Length = static_cast<float>(AB.GlowM * 100.0);
					G.Tint = Colour * (Bright * static_cast<float>(AB.GlowShare));
					G.MinPx = 1.0f;
					if (Night > 0.5f)
					{
						// At night a glint the eye sees whole (GlintAlpha), its bright core about glint_px; seen as a point, the
						// whole flame's light in it.
						G.MinPx = static_cast<float>(Tune.Lod.GlintSpritePx);
						G.KeepAlpha = static_cast<float>(Tune.Lod.GlintKeep);
						if (bPoint)
						{
							G.Tint = Colour * Brightness(AB.Brightness * AB.GlintShare);
						}
					}
					AddSprite(G);
					if (Night > 0.05f && !bPoint)
					{
						// The diamonds: the first Mach disk about 1 m aft (0.67 D sqrt(P0 / P)), then every 0.9 m, fading:
						// small knots inside the flame, not glows wider than it.
						for (int32 K = 0; K < AB.Diamonds; ++K)
						{
							FSimFxSprite D;
							D.Layer = ESimFxLayer::Glow;
							D.Pos = At - Fwd * (100.0 * (AB.DiamondFirstM + K * AB.DiamondSpacingM) * Flick);
							D.Width = D.Length = static_cast<float>(100.0 * AB.DiamondM * (1.0 - 0.12 * K));
							D.Tint = AB.DiamondColour * (Brightness(AB.Brightness * AB.DiamondShare) * Night * (1.0f - 0.15f * K));
							AddSprite(D);
						}
					}
				}
				if (Sun.bNight && DistToCamM(Pos) < AB.LightRangeM && A.Nozzles.Num() > 0)
				{
					// The flame's light from its middle, well aft of the nozzles: 8 m from the aircraft's centre it sat inside
					// the tail and lit the nozzle's rim as a ring and the fin as a hard block (the coordinator's look of 1 Nov).
					const FVector Nozzle = Pos + Rot.RotateVector(A.Nozzles[0]);
					AddLight(Nozzle - Fwd * (100.0 * AB.LightAftM), NightColour, AB.LightLumens, 100.0 * AB.LightRadiusM);
				}
			}
		}
	}
	// Aircraft gone (or down): their trails stop being laid and fade.
	for (auto It = Aircraft.CreateIterator(); It; ++It)
	{
		if (!It->Value.bSeen)
		{
			for (int32 E = 0; E < 4; ++E)
			{
				ReleaseRibbon(It->Value.Ribbons[E], ContrailKey(It->Key, E));
				It->Value.Ribbons[E] = -1;
			}
			It.RemoveCurrent();
		}
	}
}
