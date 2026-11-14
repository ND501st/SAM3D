#pragma once

// The weapons effects' plain logic (docs/fx/README.md 4.1, 5): detail by projected size, the views' frustums, the
// sprite budget, the CPU particles in flat arrays, ribbon decimation, the feed's cursor and the wrecks' falls. Nothing
// here touches a world, so the automation tests (SAM3D.Fx.*) call it directly.

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

/** How much of an effect is drawn, by its size on the largest view (docs/fx/README.md 4.1). */
enum class ESimFxTier : uint8
{
	None,      // nothing drawn (by day under the speck size, past the range or outside every view)
	Glint,     // a flash at night only: under the impostor size
	Speck,     // by day, between the speck and the impostor size: one smoke sprite at its true size and a flash (T12)
	Impostor,  // one flipbook sprite and a flash
	Full,      // the whole particle effect
};

/** The sprite layers: one instanced static mesh each, in this order. */
enum class ESimFxLayer : uint8
{
	Smoke,   // premultiplied, lit by the sun (M_FX_Smoke); the Fire layer's sprites are drawn in it (DrawLayerOf)
	Debris,  // premultiplied, the debris atlas (M_FX_Smoke with T_FX_DebrisAtlas)
	Fire,    // fireballs and flames: kept apart for the effects, drawn with the smoke, sorted together (T5)
	Glow,    // additive (M_FX_Additive with T_FX_Glow): flashes, motor glows, glints
	Spark,   // additive, T_FX_Spark
	Streak,  // additive, T_FX_Streak: tracers, velocity-aligned
	Ring,    // additive, T_FX_Ring: shock rings
	Plume,   // additive, T_FX_Plume: motor flames along the body
	Count
};

constexpr int32 SimFxLayerCount = static_cast<int32>(ESimFxLayer::Count);

/** The picture a smoke-layer sprite draws from (M_FX_Smoke's float 8). */
enum class ESimFxKind : uint8
{
	Atlas,           // the smoke (or debris) atlas, 4 x 4
	HeatFlipbook,    // the generated fireball, 8 x 8 frames: temperature through the heat ramp
	ColourFlipbook,  // the Unity Labs explosion, 5 x 5 colour frames with their HDR brightness (art/fx/recorded)
	Flash,           // the additive layers only: the spiky detonation flash (M_FX_Additive's T_FX_Flash), not the soft glow
	Afterburner,     // the additive layers only: an afterburner's flame (M_FX_Additive's T_FX_Afterburner), a nozzle-wide
	                 // column tapering aft, not the rocket plume's flaring cone
};

/** How a sprite's quad is turned. */
enum class ESimFxFacing : uint8
{
	Camera,  // a billboard facing the camera (rolled by Roll)
	Axis,    // its length along Axis, turned about it to face the camera (streaks, plumes)
	Flat,    // lying flat, its face up (a ring on the ground)
};

/** One view the effects must look right in: the player's camera, or the TV sight's capture. */
struct FSimFxView
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;
	double FovDeg = 90.0;
	double TanHalfX = 1.0;
	double TanHalfY = 0.5625;
	double WidthPx = 1280.0;
	bool bActive = false;

	/** Place it: horizontal field of view in degrees, the picture's width in pixels and its height over width. */
	void Set(const FVector& InLocation, const FRotator& Rotation, double InFovDeg, double InWidthPx, double HeightOverWidth = 0.5625);

	/** Pixels per centimetre of something DistCm away (across the picture's width). */
	double PixelsPerCm(double DistCm) const;

	/** True if a sphere of RadiusCm at P is inside the frustum widened by Margin (1.25: a quarter more each way). */
	bool Contains(const FVector& P, double RadiusCm, double Margin) const;
};

/** The detail thresholds (the sam3d.fx.* cvars and the weather's visibility). */
struct FSimFxLod
{
	double FullPx = 48.0;
	double ImpostorPx = 6.0;
	double SpeckPx = 1.5;     // by day, a speck down to this (T12)
	double MaxRangeM = 40000.0;
	double VisibilityM = 40000.0;
	double Margin = 1.25;
	bool bNight = false;
};

namespace SimFxLogic
{
	/** How many pixels something SizeM across covers DistM away on a view of FovDeg horizontal and WidthPx wide:
	 *  size / (2 d tan(fov / 2)) * width. */
	double ProjectedPx(double SizeM, double DistM, double FovDeg, double WidthPx);

	/** The tier for a projected size at a distance (past the range or the visibility: None). */
	ESimFxTier TierForPx(double Px, double DistM, const FSimFxLod& Lod);

	/** The best tier over the active views whose frustum holds the effect (SizeM across, at At); None if in none.
	 *  OutPx, if given, gets the largest projected size. */
	ESimFxTier TierFor(TConstArrayView<FSimFxView> Views, const FVector& At, double SizeM, const FSimFxLod& Lod, double* OutPx = nullptr);

	/** True if any active view's frustum (widened by the margin) holds a sphere of RadiusCm at At. */
	bool InAnyView(TConstArrayView<FSimFxView> Views, const FVector& At, double RadiusCm, double Margin);

	/** Keep at most MaxPoints of a trail's points, the first and last always kept: every second point of the older half
	 *  goes, again until it fits (the far, old end of a trail loses detail first). */
	template <typename T>
	void DecimateKeepingEnds(TArray<T>& Points, int32 MaxPoints)
	{
		MaxPoints = FMath::Max(MaxPoints, 2);
		while (Points.Num() > MaxPoints)
		{
			const int32 N = Points.Num();
			const int32 Half = FMath::Max(N / 2, 2);
			int32 Write = 0;
			for (int32 I = 0; I < N; ++I)
			{
				const bool bKeep = I == 0 || I == N - 1 || I >= Half || (I % 2) == 0;
				if (bKeep)
				{
					if (Write != I)
					{
						Points[Write] = MoveTemp(Points[I]);
					}
					++Write;
				}
			}
			if (Write == N)
			{
				break;  // nothing left to drop (cannot happen with MaxPoints >= 2)
			}
			Points.SetNum(Write, EAllowShrinking::No);
		}
	}

	/** A smooth 0..1 ease. */
	inline float Smooth01(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	/** The share of its growth a particle has made: 1 - e^(-Age/GrowS) with a growth time (full size within a few of
	 *  them: the art review's D7, LOOK 3 "0.1-1 s puff grows to size"); without one (GrowS <= 0) an ease-out over its
	 *  whole life, 1 - (1 - U)^2. */
	float GrowShare(float Age, float Life, float GrowS);

	/** Erosion by the share of life spent: none in the first half, then eaten away to the end (D7, T3). */
	float ErosionAt(float U);

	/** A fireball's heat on Gross's cooling curve (LOOK 0.1): Heat0 held for HoldS, then Heat0 e^(-(Age - HoldS)/TauS).
	 *  With no TauS (<= 0) a straight line from Heat0 to Heat1 over the life (U the share spent). */
	float HeatAt(float Heat0, float Heat1, float U, float Age, float HoldS, float TauS);

	/** The e-folding time of a fireball's heat for a charge (the art review's D2): 0.15 s (W / 100 kg)^(1/3). */
	double FireballTauS(double ChargeKg);

	/** The opacity factor for a sprite widened to a pixel floor (D8): its true area over the drawn, so the light it
	 *  gives stays what its true size gives (a 0.4 px tracer drawn 2 px wide carries a fifth of the light). */
	float SubPixelAlpha(double TrueWidth, double TrueLength, double DrawnWidth, double DrawnLength);

	/** SubPixelAlpha never under Keep (0..1): a light seen as a point at night (a motor's flame, an afterburner) is a
	 *  glint the eye sees whole however small it is, not its true area smeared dim over its pixel floor (the coordinator's
	 *  look of 1 Nov: our launch at 3 s and a far afterburner were one dim pixel or nothing). */
	float GlintAlpha(double TrueWidth, double TrueLength, double DrawnWidth, double DrawnLength, float Keep);

	/** A ribbon's width with a pixel floor (T10, phone-wire anti-aliasing): never under MinPx on the view, and the alpha
	 *  factor that keeps its light (true width over drawn). PixelsPerCm at the point's distance. */
	float RibbonWidthFloor(float TrueWidthCm, double PixelsPerCm, double MinPx, float& OutAlpha);

	/** The sprite layer a layer's sprites are drawn in: the fire's with the smoke (one sorted premultiplied draw, T5). */
	ESimFxLayer DrawLayerOf(ESimFxLayer Layer);

	/** A point light's light on a white surface DistCm away (T9), in the units of FSimFxSun::SunWhite (lux / pi of the
	 *  scaled light pool): Lumens / (4 pi d^2) / pi, windowed to its radius as Unreal's inverse-squared falloff
	 *  ((1 - (d/R)^4)^2). Near the light, d is held at MinDistCm (the puff's own size). */
	FLinearColor LightOnSurface(const FLinearColor& Colour, double Lumens, double DistCm, double RadiusCm, double MinDistCm);

	/** The sky's light on a shaded side against the sun's on a lit one, by the sun's elevation (N1): Day over 6 degrees,
	 *  Dusk from 0 to -6, then falling a decade every 2 degrees (the sky darkens about a thousandfold through nautical
	 *  twilight) to Night. */
	double AmbientShare(double ElevationDeg, double Day, double Dusk, double Night);

	/** A ground burst's dust column (LOOK 0.3, Church 1969): it stabilises at ColumnStabS(W) = 60 s (W / 100 kg)^0.23
	 *  (an estimate fitted to LOOK's table: 60 s at 100 kg, 75 at 200, 100 at 900), rising as t^1/2 to a top of
	 *  Scale 92.6 W^0.25 m (Scale 0.7 Green Field-like to 1.0 Church), W the charge in kg of TNT. */
	double ColumnStabS(double ChargeKg);
	double ColumnTopM(double ChargeKg, double AgeS, double Scale);
	/** Its rise constant: the top is K sqrt(t) m until it stabilises (ColumnTopM's). */
	double ColumnRiseK(double ChargeKg, double Scale);
	/** Its width once stabilised: 2 R = 2 x 4.7 W^0.375 m (Church): 53 m at 100 kg. */
	double ColumnWidthM(double ChargeKg);

	/** A fuel fireball (LOOK 6, CCPS): D = 5.8 M^(1/3) m lasting 0.45 M^(1/3) s, M the fuel burning in kg. */
	double FuelFireballM(double FuelKg);
	double FuelFireballS(double FuelKg);

	/** A burst's size at altitude against sea level (LOOK 3, Sachs scaling): (P0 / P)^(1/3), the standard atmosphere's
	 *  pressure to 11 km (held above): x 1.23 at 5 km, x 1.56 at 10 km. */
	double SachsScale(double AltitudeM);

	/** One row of the flak table (LOOK 4): a calibre's puff, how long it hangs and whether it is TNT's black soot. */
	struct FFlakRow
	{
		double CalibreMm = 23.0;
		double PuffM = 1.5;
		double HangS = 20.0;
		double Black = 0.0;  // 0: light grey (RDX, A-IX-2), 1: black (TNT)
	};
	/** The table's row for a calibre, interpolated between its rows (held past its ends); a default row if empty. */
	FFlakRow FlakFor(TConstArrayView<FFlakRow> Rows, double CalibreMm);

	/** A shock ring lying on the ground seen at a grazing angle is a hard line: its opacity by how much the view looks
	 *  down on it, |to camera . up| / 0.17 (about 10 degrees), at most 1 (the art review's section 2). */
	float RingAlpha(double ToCamUp);

	/** The flashes' light on the smoke held under Cap in its brightest channel (by day a burst's light must not turn its
	 *  own dust white: the art review's section 2); Cap <= 0 holds nothing (the night). */
	FLinearColor CapLight(const FLinearColor& Light, double Cap);

	/** A direction within HalfAngleRad of Axis, uniform over the cone's solid angle from two uniforms 0..1 (fragments
	 *  thrown forward along the missile's velocity: LOOK 3, 8). */
	FVector DirInCone(const FVector& Axis, double HalfAngleRad, double U1, double U2);

	/** Where a particle is after T seconds: thrown at V0 from P0, its velocity relaxing to Wind at Drag (1/s), falling at
	 *  Gravity (cm/s^2): FSimFxParticles::Update's motion in closed form (for the puffs laid along a clod's path). */
	FVector BallisticAt(const FVector& P0, const FVector& V0, const FVector& Wind, double Drag, double Gravity, double T);

	// ---- trails, contrails, guns and afterburners (LOOK 9-14)

	/** Where a ribbon's point laid at Pos is Age seconds later (cm): carried by its own drift (the wind at its height,
	 *  so a climb shears into a staircase: LOOK 9.1), rolled toward Side with an e-folding time RollS (a contrail's
	 *  plume wound into its wingtip vortex: LOOK 12) and sunk by the vortex pair, SinkCmps at first and never more than
	 *  SinkMaxCm (D (1 - e^(-t v / D)): LOOK 12's 2.5 m/s, 125 m at 50 s, 270 m in minutes). */
	FVector RibbonPointAt(const FVector& Pos, const FVector& Drift, const FVector& Side, float RollS, float SinkCmps, float SinkMaxCm, float Age);

	/** A contrail's Crow instability (LOOK 12): how pinched the pair is at DistM along the trail, 0..1: none before FromS,
	 *  growing to full by FullS, and along the trail 0.5 + 0.5 cos(2 pi d / PeriodM) (the vortex rings every 8.6 spans). */
	float CrowAt(float DistM, float PeriodM, float Age, float FromS, float FullS);

	/** A trail's opacity factor by its age (LOOK 9.3): 1 at the missile, thinning to ThinShare by ThinS (0.8 to 0.5 by
	 *  15 s for our 9M33), and gone smoothly over the last third of its life. */
	float TrailOpacityAt(float Age, float LifeS, float ThinS, float ThinShare);

	/** A trail's erosion by its age (LOOK 9.3: from 15 s it breaks into puffs): none before From of its life, then rising
	 *  to 0.85 at its end (eats the texture's thin parts first, T3). */
	float TrailErosionAt(float Age, float LifeS, float From);

	/** How much of the night's look applies at a sun elevation (degrees): 0 by day from 6 degrees up, 1 by night from 6
	 *  degrees under the horizon (civil twilight's end), smooth between. */
	float NightShare(double SunElevationDeg);

	/** A tracer's colour by the light (LOOK 14): yellow-white by day, crimson at night, between them by NightShare. */
	FLinearColor TracerColourAt(const FLinearColor& Day, const FLinearColor& Night, double SunElevationDeg);

	/** A round's tracer colour by the light when its munition names one (Own; bOwn false: Day and Night alone): the
	 *  munition's colour is its night colour; by day a warm one (red, orange: R the largest channel) turns the day's
	 *  yellow-white (LOOK 14: Soviet 23 and 57 mm tracers #FFE8C0 by day, crimson at night; the munition's orange was
	 *  drawn by day as faint salmon dots), any other (the small arms' green) halfway to white. */
	FLinearColor TracerColourFor(const FLinearColor& Own, bool bOwn, const FLinearColor& Day, const FLinearColor& Night, double SunElevationDeg);

	/** A MANPADS corkscrew's radius at DistM along its flight (LOOK 9.2: 0.3-1.5 m, the seeker's corrections uneven): the
	 *  mean radius RadiusM times 0.5-1.5, smooth along the flight from two waves of incommensurate lengths (2.7 and 6.1
	 *  turns of PitchM), shifted by Phase, so no two turns are alike. */
	double HelixRadiusAt(double DistM, double PitchM, double RadiusM, double Phase);

	/** Whether the round with this index in the belt carries a tracer: every Every-th (LOOK 14: belts of 3 OFZ(T) to 1
	 *  BZT, 4 OFZ to 1 BZT); Every 1 or less, every round. */
	bool IsTracedRound(int32 RoundIndex, int32 Every);

	/** One burst of an air defence gun's pass: firing from StartS until EndS (simulation s). */
	struct FGunBurst
	{
		double StartS = 0.0;
		double EndS = 0.0;
	};

	/** A gun pass's bursts (LOOK 14, effects.json "guns"): the first from T0, each BurstMinS-BurstMaxS long with a pause of
	 *  PauseMinS-PauseMaxS before the next, the last cut at T1, at most MaxBursts. The lengths are drawn from Seed alone
	 *  (the event's seq), so the picture (USimFxSubsystem) and the sound (USimFxAudioSubsystem) fire the same bursts. */
	void GunBursts(uint32 Seed, double T0, double T1, double BurstMinS, double BurstMaxS, double PauseMinS, double PauseMaxS,
		TArray<FGunBurst>& Out, int32 MaxBursts = 512);

	/** A gun's burst lengths (s) for its rate: MinS-MaxS, but long enough for MinRounds rounds at least and up to twice as
	 *  many (the Shilka's 0.2-0.35 s is 12-20 rounds, LOOK 14; at the S-60's 2 rounds a second it held one: the second art
	 *  review's item 7). MinRounds 1 or less, or no rate: MinS-MaxS. */
	void GunBurstSpan(double MinS, double MaxS, int32 MinRounds, double RateHz, double& OutMinS, double& OutMaxS);

	/** The value a table of (id prefix, value) gives the first of these ids it matches (case-insensitive), else Default. */
	int32 NamedIntFor(TConstArrayView<TPair<FString, int32>> Table, const FString& Id, const FString& OtherId, int32 Default);

	/** The times the rounds of one burst leave the gun: the first at its start, then every 1 / RateHz while the burst and
	 *  the pass (T1) last; at least the first. As USimFxSubsystem fires them. */
	void GunBurstShots(const FGunBurst& Burst, double RateHz, double T1, TArray<double>& Out);

	/** A tracer's streak length (LOOK 14): what it flies in a frame, never under MinFrameS of flight (1/60 s: 16 m at
	 *  970 m/s), so a slow frame stretches it rather than breaking the stream into dots. */
	double TracerLengthCm(double SpeedCmps, double FrameS, double MinFrameS);

	/** A MANPADS's corkscrew (LOOK 9.2): the offset (cm) across its line after DistM of flight, a helix of PitchM per turn
	 *  and RadiusM about the axis Fwd (Phase in radians). */
	FVector HelixOffset(const FVector& Fwd, double DistM, double PitchM, double RadiusM, double Phase);

	/** A sprite or a trail seen from inside it (LOOK 9.3 and T2: the player stands in his launch's smoke): clear within
	 *  1 m plus half its size of the camera, whole again 3 m plus half its size further on; tested at a sprite's pivot so the
	 *  quad goes together. M_FX_Ribbon fades per pixel by the trail's width (UV3.y), further out. */
	float CameraFade(double DistCm, double SizeCm);

	/** How much of a trail near the camera and seen nearly along its length is drawn as puffs instead of its strip (T10:
	 *  puffs near, a ribbon far; the second art review's item 2, the player looking up his own missile's climb saw
	 *  angular white sheets): 1 under a sine of 0.2 within 200 m, 0 from a sine of 0.5 or from 300 m, smooth between;
	 *  nearer end-on the farther (200 m x 0.2 / sine, at most 2 km, gone by 1.5 times that), where the strip is gone. */
	float TrailPuffShare(double SinView, double DistCm);

	/** A flash's light colour by its age (Gross's curve, LOOK 0.1): white for its first WhiteS, then from Base cooling to
	 *  Ember by the end of its life (the light's brightness falls apart from this). */
	FLinearColor FlashLightColour(const FLinearColor& Base, const FLinearColor& Ember, double AgeS, double DurationS, double WhiteS);

	/** A puff's opacity factor as it thins early in its life and then lingers (LOOK 2.1's dust skirt, 0.6 to 0.3 by 3 s):
	 *  1 at birth to ThinTo at ThinS (smooth), held after; ThinS 0, always 1. */
	float ThinAt(float Age, float ThinS, float ThinTo);

	/** The share of a ground burst's short-lived detail (clods, sparks; its skirt puffs, ejecta jets and fireball sprites at least half)
	 *  drawn when Neighbours other bursts played in full lately stand near it, a crowded burst's clods trailing no dust
	 *  (a stick of bombs: the second art review's cost note, six Mk 82s 769 sprites, 0.7 ms): all of it alone, then
	 *  2 / (n + 2), so a stick's clods together stay near two and a half bursts' worth; never under a quarter. */
	float CrowdShare(int32 Neighbours);

	/** The sun's share on a point of a puff or a trail's edge (the second art review's item 1; M_FX_Smoke does the same per
	 *  pixel): the surface there faces normalize(Out + 0.6 ToCam), Out its offset from the middle over the radius (at most
	 *  1), so a puff is a ball turned to the camera; 0.75 of saturate(0.3 + 0.7 N . ToSun) and 0.25 of Lit (the whole
	 *  puff's, thin smoke against the sun still glowing). */
	float SunShare(const FVector& Out, const FVector& ToCam, const FVector& ToSun, float Lit);

	/** The way that point of a puff faces (SunShare's normal): normalize(Out, at most 1 long, + 0.6 ToCam). */
	FVector PuffNormal(const FVector& Out, const FVector& ToCam);

	/** The sky's share on a surface facing NormalZ up (M_FX_Smoke does the same per pixel; the second art review's item 1:
	 *  the sky lit the undersides as fully as the tops): the whole sky facing up, Low of it facing the ground (what the
	 *  ground sends back), smooth between: Low + (1 - Low) saturate(0.5 + 0.5 NormalZ). */
	float SkyShare(double NormalZ, double Low);

	/** A trail's strip seen along its own length (SinView: the sine of the angle between the trail and the line of sight)
	 *  is a flat card turned edge-on, a sheet or a wedge rather than a column of smoke: faded out between 0.2 and 0.03. */
	float EndOnAlpha(double SinView);

	/** The effects' step this frame (s): the render clock's advance, at least 0, at most a hitch's MaxHitchS of real time
	 *  times the simulation's time scale (at least 1). USimWorldSubsystem steps min(frame, 0.25 s) x scale, so at 32x the
	 *  effects keep the clock that times their events (a fixed 0.25 s cap left a neighbour's SAM kilometres short of its
	 *  burst and smoke living twice its tuned length: the fix-up review of 1 Nov). */
	double FrameDt(double DeltaNowS, double TimeScale, double MaxHitchS);

	/** How many equal slices a step of DtS is integrated in: at most SliceS each, at least 1, at most MaxSlices (past that
	 *  the slices grow: a long stall is caught up in bounded work). */
	int32 SubSteps(double DtS, double SliceS, int32 MaxSlices);

	/** How far a trail is laid and drawn (m): out to RangeScale times the effects' range (a contrail 2, the far line in clear
	 *  air) but never past the weather's visibility, plus MarginM (docs/fx/README.md 4.1: nothing past the visual range;
	 *  the fix-up review of 1 Nov: trails 80 km out in 5 km visibility held the ribbon pool). */
	double TrailRangeM(const FSimFxLod& Lod, double RangeScale, double MarginM);

	/** May a ribbon still being laid be taken over? Only a contrail (a missile's, a rocket's or a wreck's smoke is never
	 *  stopped). Another owner's smoke takes any contrail; a contrail takes one only when that one is over Ratio times as
	 *  far from the camera as itself, so a near aircraft is not left trail-less by a far raid holding every ribbon and two
	 *  near ones do not trade a ribbon back and forth. */
	bool RibbonTakeOver(bool bRequesterContrail, double RequesterDistM, bool bVictimContrail, double VictimDistM, double Ratio);

	/** The trails' mesh section's size in quads this frame, Current its size now (0: none). Whole chunks of Chunk quads;
	 *  it grows at once to hold Quads, and shrinks to fit only after Quads has needed under half of it for ShrinkAfter
	 *  frames in a row (InOutLowFrames counts them): every quad of the section is uploaded every frame, so one busy moment
	 *  must not cost every frame after it (the fix-up review of 1 Nov), and a count wavering about the line rebuilds
	 *  nothing. bOutRebuild: the section is to be made anew at the size returned. */
	int32 RibbonSectionQuads(int32 Quads, int32 Current, int32 Chunk, int32 ShrinkAfter, int32& InOutLowFrames, bool& bOutRebuild);
}

/** A sprite as it is drawn this frame: from a particle, or put straight in by an effect (a plume, a tracer). */
struct FSimFxSprite
{
	FVector Pos = FVector::ZeroVector;  // its centre, world cm
	FVector Axis = FVector::ForwardVector;  // Axis facing: the direction its length runs (the texture's U)
	float Width = 100.0f;   // cm, across
	float Length = 100.0f;  // cm, along (Camera and Flat: the same as Width unless stretched)
	float Roll = 0.0f;      // radians, a billboard's turn in the picture
	float Frame = 0.0f;
	float Opacity = 1.0f;
	float Heat = 0.0f;
	FLinearColor Tint = FLinearColor::White;
	float Erosion = 0.0f;
	float MinPx = 0.0f;     // never drawn smaller than this many pixels on the main view (tracers, glints); the
	                        // sprite's opacity then falls by its true area over the drawn (SubPixelAlpha)
	float KeepAlpha = 0.0f; // widened to MinPx, its opacity never falls under this (GlintAlpha: a night glint)
	ESimFxLayer Layer = ESimFxLayer::Smoke;
	ESimFxFacing Facing = ESimFxFacing::Camera;
	ESimFxKind Kind = ESimFxKind::Atlas;  // the smoke layer's picture (the fire's sprites: a flipbook)
};

/** A CPU particle: born, moved and faded on the simulation's clock. */
struct FSimFxParticle
{
	FVector Pos = FVector::ZeroVector;   // cm (a rooted axis sprite: its root)
	FVector Vel = FVector::ZeroVector;   // cm/s
	FVector Wind = FVector::ZeroVector;  // cm/s: the air it is carried by (drag pulls Vel toward it)
	float Age = 0.0f;       // s; below 0 it waits, unmoved and undrawn, until its time comes (a later puff of an effect)
	float Life = 1.0f;
	float Size0 = 100.0f;   // cm, at birth
	float Size1 = 100.0f;   // cm, grown to (GrowShare)
	float GrowS = 0.0f;     // s: the growth time (0: an ease-out over its whole life)
	float Stretch = 1.0f;   // length over width (Axis facing along the velocity)
	float Opacity = 1.0f;
	float FadeInS = 0.0f;
	float FadeFrom = 0.6f;  // the share of its life after which it fades out
	float ThinS = 0.0f;     // ThinAt: thinned to ThinTo of its opacity over this many seconds, then held (0: none)
	float ThinTo = 1.0f;
	float Heat0 = 0.0f;
	float Heat1 = 0.0f;
	float HeatHoldS = 0.0f;  // Gross's curve (HeatAt): held this long, then falling by HeatTauS
	float HeatTauS = 0.0f;   // 0: a straight line from Heat0 to Heat1
	float Frame0 = 0.0f;
	float Frames = 0.0f;    // frames played over its life (0: Frame0 throughout)
	float Drag = 0.0f;      // 1/s
	float Gravity = 0.0f;   // cm/s^2 down
	float Buoyancy = 0.0f;  // cm/s^2 up, fading with age
	float Roll = 0.0f;
	float Spin = 0.0f;      // rad/s
	float MinPx = 0.0f;
	float SpreadCmps = 0.0f;  // cm/s: slow widening after its growth (a puff hanging aloft for minutes)
	float RiseK = 0.0f;     // cm/s^(1/2): a column's member rises RiseK sqrt(age) (LOOK 0.3), until RiseStopS
	float RiseStopS = 0.0f; // s: the column stabilised (0: never)
	float LengthK = 0.0f;   // cm/s^(1/2): an axis sprite's length at least LengthK sqrt(age) (a column's stem), until RiseStopS
	bool bRooted = false;   // an axis sprite drawn from Pos along its axis (a stem, an ejecta jet), not centred on it
	FVector FixedAxis = FVector::ZeroVector;  // an axis sprite's axis (zero: along its velocity)
	FLinearColor Tint = FLinearColor::White;
	FLinearColor Tint1 = FLinearColor::White;  // with bTintOverLife: the tint it has turned to by the end of its life
	bool bTintOverLife = false;
	ESimFxLayer Layer = ESimFxLayer::Smoke;
	ESimFxFacing Facing = ESimFxFacing::Camera;
	ESimFxKind Kind = ESimFxKind::Atlas;

	/** Not yet begun (a negative age): neither moved nor drawn. */
	bool IsWaiting() const { return Age < 0.0f; }

	/** This particle as drawn now. */
	FSimFxSprite ToSprite() const;
};

/** Every particle, one flat array per layer, under one budget (sam3d.fx.MaxSprites). */
class FSimFxParticles
{
public:
	void SetMax(int32 InMax) { Max = FMath::Max(InMax, 0); }
	int32 GetMax() const { return Max; }

	/** Add one; false (and nothing added) when the budget is full. */
	bool Spawn(const FSimFxParticle& P);

	/** How many more fit. */
	int32 Room() const { return FMath::Max(Max - Total, 0); }

	/** Age, move and drop the dead, one pass over each layer. */
	void Update(float Dt);

	int32 Num() const { return Total; }
	const TArray<FSimFxParticle>& Layer(ESimFxLayer L) const { return Layers[static_cast<int32>(L)]; }
	void Clear();

private:
	TArray<FSimFxParticle> Layers[SimFxLayerCount];
	int32 Total = 0;
	int32 Max = 4000;
};

/** The reader's place in a runner's fx::Feed. */
struct FSimFxFeedCursor
{
	const void* Runner = nullptr;
	uint64 Seq = 0;

	/** Before reading: true (and the cursor back at 0) when the runner changed (a new scenario) or its feed went
	 *  backwards (Last below the cursor); the effects of the old one must then be cleared. */
	bool Sync(const void* InRunner, uint64 Last)
	{
		if (InRunner != Runner || Last < Seq)
		{
			Runner = InRunner;
			Seq = 0;
			return true;
		}
		return false;
	}
};

/** The killed aircraft's falls (docs/fx/README.md 4.3), in ENU, by their World id. */
class FSimFxWrecks
{
public:
	struct FWreck
	{
		int32 Id = -1;
		double StartT = 0.0;  // the simulation time it was destroyed
		simcore::fx::WreckFall Fall;
		bool bFire = false;
		bool bLanded = false;  // the effects have played its impact
		FString Model;
	};

	/** The seed every client and the audio use for an aircraft's fall. */
	static uint32 SeedFor(int32 WorldId) { return static_cast<uint32>(WorldId) * 2654435761u ^ 0x5A17u; }

	/** Register a fall (replacing one of the same id). */
	FWreck& Add(int32 Id, double StartT, const simcore::fx::WreckFallParams& Params, bool bFire, const FString& Model);

	const FWreck* Find(int32 Id) const;
	FWreck* Find(int32 Id);

	/** Where the wreck is at Now (ENU) and how it is turned; bOutVisible false once on the ground. False if unknown. */
	bool PoseEnu(int32 Id, double Now, simcore::Vec3& OutPos, simcore::fx::WreckRotation& OutRot, bool& bOutVisible) const;

	TArray<FWreck>& Items() { return Wrecks; }
	const TArray<FWreck>& Items() const { return Wrecks; }
	void Clear() { Wrecks.Reset(); }

private:
	TArray<FWreck> Wrecks;
};
