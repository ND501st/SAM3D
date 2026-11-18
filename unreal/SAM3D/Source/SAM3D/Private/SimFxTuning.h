#pragma once

// The effects' game values (data/fx/effects.json, "source": "sam3d game value"): sizes, lifetimes, counts, colours,
// intervals, brightness. Defaults here; the file overrides any it names. Sizes in metres, times in seconds, colours
// linear RGB, brightness in multiples of a white surface lit by the level's sun (sam3d.fx.EmissiveScale scales them).

#include "CoreMinimal.h"
#include "SimFxLogic.h"

struct FSimFxTuning
{
	struct FLod
	{
		double ViewMargin = 1.25;     // the frustum widened by a quarter each way
		double FarRibbonM = 5000.0;   // a ribbon farther than this is rebuilt every fourth frame
		double GlintPx = 3.0;         // a night glint's smallest size on the screen
		double GlintSpritePx = 8.0;   // a night point light's glow sprite at least this wide: its bright core about GlintPx
		double GlintKeep = 0.5;       // and its opacity at that floor never under this (SimFxLogic::GlintAlpha)
		double SpeckPx = 1.5;         // by day a burst over this is a grey speck, under it nothing (LOOK T12)
		double MunitionPx = 3.0;      // a munition's mesh is shown over this
		double TrailMarginM = 2000.0; // trails laid and drawn to min(range, visibility) plus this (SimFxLogic::TrailRangeM)
		double ContrailRangeScale = 2.0;  // a contrail to this times the range (still never past the visibility)
	} Lod;

	struct FLighting
	{
		double SunReferenceLux = 10.0;  // the sun the brightness values were chosen under (ASimSkyWeatherActor's)
		double AmbientDay = 0.30;       // the sky's light on a shaded side, against the sun's on a lit one
		double AmbientDusk = 0.10;
		double AmbientNight = 2.5e-6;   // a full moon's 0.25 lux against a 100000-lux sun (LOOK 15)
		FLinearColor SkyTint = FLinearColor(0.70f, 0.80f, 1.0f);
		double Backlit = 0.25;          // how lit a puff's side to the camera is with the sun straight behind it
		double SmokeSun = 0.3;          // the sun's light on smoke and trails against a surface's (photographic: sunlit
		                                // smoke short of clipping, its shaded side seen)
		double SmokeSky = 0.6;          // and the sky's on them (Ambient's share)
		double SmokeSkyLow = 0.55;      // the sky's light on a puff's underside against its top (SkyShare)
		double LightScale = 1.0e-4;     // the light pool's lumens per real lumen (a 10-lux sun against a 100000-lux one)
		double MinLightLumens = 20.0;
	} Lighting;

	struct FExplosion
	{
		double FlashS = 0.15;
		double FlashBrightness = 80.0;
		FLinearColor FlashColour = FLinearColor(1.0f, 0.85f, 0.6f);
		int32 FireSpritesFull = 8;
		double FireBrightness = 30.0;
		double ColourFlipbookGlow = 0.1; // the recorded fireball's emission (its _EmissionLog) against the fire's brightness
		FLinearColor AirSmokeColour = FLinearColor(0.035f, 0.033f, 0.032f);
		FLinearColor DustColour = FLinearColor(0.36f, 0.30f, 0.22f);
		double SmokeOpacity = 0.9;
		double DebrisSpeedMps = 45.0;
		double DebrisSizeM = 0.5;
		FLinearColor DebrisColour = FLinearColor(0.09f, 0.08f, 0.06f);
		int32 DebrisMax = 40;
		int32 SparksMax = 24;
		double SparkSpeedMps = 150.0;
		double SparkS = 0.5;
		double SparkBrightness = 40.0;
		FLinearColor SparkColour = FLinearColor(1.0f, 0.7f, 0.35f);
		double RingS = 0.45;
		double RingSpeedMps = 250.0;
		double RingBrightness = 3.0;
		FLinearColor RingColour = FLinearColor(0.9f, 0.85f, 0.75f);
		double LightS = 0.35;
		FLinearColor LightColour = FLinearColor(1.0f, 0.7f, 0.4f);
		FLinearColor LightEmberColour = FLinearColor(1.0f, 0.22f, 0.0f);  // what a flash's light cools to (#FF8100)
		double LightWhiteS = 0.035;      // a flash's light white this long first
		double NightLightCap = 0.0;      // at night the flashes light a puff at most this (SunColor's units; 0: no cap)
		double CrowdRadiusM = 150.0;    // ground bursts this near one another share out their clods and dust (CrowdShare)
		double CrowdS = 3.0;            // within this many seconds
		double LightRadiusPerFireballM = 30.0;
		double ScorchPerCrater = 2.5;
		double ScorchMinM = 1.5;
		double ScorchS = 1200.0;
		double DayLightCap = 0.6;        // by day the flashes light a puff at most this share of the sun's white (art review 2)
		// The detonation's spiky flash (LOOK 2.1, 3: 1-2 frames, 2-3 x the fireball).
		double SpikeS = 0.035;
		double SpikeFireballs = 2.5;
		// The ground burst's column (LOOK 0.3, 2.1).
		double ColumnScale = 0.85;       // of Church's height (0.7 Green Field-like to 1.0)
		int32 ColumnMembers = 10;
		double ColumnLifeS = 240.0;
		FLinearColor ColumnLowColour = FLinearColor(0.068f, 0.060f, 0.051f);
		FLinearColor ColumnHighColour = FLinearColor(0.29f, 0.24f, 0.19f);
		double ColumnLowOpacity = 0.95;
		double ColumnHighOpacity = 0.7;
		double SootDarken = 0.6;         // a TNT fill's column against a Tritonal one
		TArray<FString> SootMunitions;   // the munitions filled with TNT (black soot, LOOK 0.2)
		int32 EjectaJets = 8;
		double EjectaWidthM = 5.0;
		double EjectaS = 2.2;
		int32 SkirtPuffs = 10;
		double SkirtRadiusM = 75.0;
		double SkirtOpacity = 0.6;
		double SkirtS = 40.0;
		int32 ClodClumps = 4;
		FLinearColor MasonryColour = FLinearColor(0.485f, 0.423f, 0.334f);
		// A warhead's burst in the air (LOOK 3).
		double AirPuffS = 120.0;
		FLinearColor AirGreyColour = FLinearColor(0.155f, 0.144f, 0.13f);
		double AirSparkConeDeg = 35.0;
		double AirSparkReachM = 75.0;
		int32 AirSparks = 30;
		// Flak by calibre (LOOK 4): calibre mm, puff m, hangs s, black 0..1.
		TArray<SimFxLogic::FFlakRow> FlakByCalibre;
		FLinearColor FlakLightColour = FLinearColor(0.58f, 0.56f, 0.53f);
		FLinearColor FlakBlackColour = FLinearColor(0.027f, 0.024f, 0.021f);
		// An ARM near a radar (LOOK 8).
		double ArmBurstUpM = 5.0;
		int32 ArmWpPieces = 20;
		FLinearColor WpColour = FLinearColor(0.89f, 0.88f, 0.84f);
		TArray<FString> WpMunitions;     // ARMs with a white-phosphorus marker (the Shrike from the AGM-45A-2)
	} Explosion;

	struct FMissile
	{
		double LaunchFlashM = 6.0;
		double LaunchFlashS = 0.25;
		int32 LaunchCloudPuffs = 12;
		double LaunchCloudM = 7.0;
		double LaunchCloudGrow = 2.5;
		double LaunchCloudS = 20.0;
		FLinearColor LaunchCloudColour = FLinearColor(0.80f, 0.79f, 0.77f);
		double LaunchCloudOpacity = 0.85;
		double PlumeBoostLengthM = 7.0;
		double PlumeBoostWidthM = 1.4;
		double PlumeSustainLengthM = 3.5;
		double PlumeSustainWidthM = 0.8;
		double PlumeBrightness = 35.0;
		FLinearColor PlumeColour = FLinearColor(1.0f, 0.7f, 0.4f);
		double GlowBoostM = 5.0;
		double GlowSustainM = 2.5;
		double TrailPointM = 20.0;
		double TrailWidthDenseM = 2.5;
		double TrailWidthReducedM = 1.5;
		double TrailWidthMinimalM = 0.8;
		double TrailOpacityDense = 0.85;
		double TrailOpacityReduced = 0.45;
		double TrailOpacityMinimal = 0.15;
		double TrailSpreadMps = 0.8;
		FLinearColor TrailColour = FLinearColor(0.82f, 0.82f, 0.80f);
		// The trail starts about as wide as the fin span (LOOK 9.1), less for a reduced or minimal smoke motor.
		double TrailSpanShareDense = 1.0;
		double TrailSpanShareReduced = 0.8;
		double TrailSpanShareMinimal = 0.6;
		double TrailSpreadJitter = 0.5;  // each point widens 1 +- this as fast: the trail breaks into puffs (LOOK 9.3)
		double TrailTurbMps = 0.4;       // eddies across the trail, smooth along it
		double TrailTileM = 60.0;        // the texture's length along the trail
		double TrailWaveM = 160.0;       // the widening's and the eddies' wavelength along it (8 of its 20 m points)
		double TrailThinS = 15.0;        // its opacity 0.8 at the missile thins to 0.5 by 15 s (LOOK 9.3)
		double TrailThinShare = 0.625;
		double TrailErodeFrom = 0.35;    // from this share of its life it breaks into puffs (LOOK 9.3: from about 15 s of 40)
		double TrailRiseMps = 0.2;       // the warm smoke's slow rise
		double LaunchCloudSpreadM = 10.0;   // the launch cloud's puffs across the way out (LOOK 9.3: low and wide)
		int32 LaunchCloudPuffsImpostor = 5;
		// A MANPADS's corkscrew (LOOK 9.2): pitch 430 / 20 m, radius 0.3-1.5 m; for missiles no wider than this.
		double HelixPitchM = 21.5;
		double HelixRadiusM = 0.6;
		double HelixMaxDiameterM = 0.1;
		double HelixPointM = 3.0;
		double NightLitTrailM = 25.0;          // at night the flame lights this much of its smoke behind it
		double NightLitTrailBrightness = 6.0;  // that glow's brightness (times the plume's and the trail's colours)
		double DefaultSmokeS = 40.0;
		double TracerGlowM = 0.9;
		double TracerBrightness = 20.0;
		FLinearColor TracerColour = FLinearColor(1.0f, 0.85f, 0.6f);
		double SamSpeedMps = 800.0;
		double ArmSpeedMps = 700.0;
		double DefaultChargeKg = 15.0;
		double BurstAtMissileM = 300.0;  // a SAM's burst at the missile when it is this close to the aim, else at the aim
	} Missile;

	struct FBombs
	{
		double RippleS = 0.08;
		int32 MaxPerRelease = 12;
		double ImpactScatterM = 6.0;
		double RetarderOpenS = 0.3;
		double RocketRippleS = 0.05;
		double RocketSmokeS = 8.0;
		int32 ClusterBursts = 24;
		int32 ClusterCloudSprites = 40;
		double ClusterBurstKg = 0.4;
		double ClusterSpreadS = 1.6;
		int32 ClusterBurstsMax = 200;   // a dispenser's own bomblets (LOOK 5: 42-202), at most this many
		int32 ClusterHazeSprites = 8;
		double ClusterHazeS = 60.0;
		double ClusterHazeM = 8.0;      // the haze's height (LOOK 5: 5-10 m)
		FLinearColor ClusterHazeColour = FLinearColor(0.39f, 0.37f, 0.33f);
		double GunDustM = 3.0;
		double GunWalkMps = 0.0;  // 0: from the gun's rate and the pass
		double MuzzleFlashM = 2.0;
	} Bombs;

	struct FGuns
	{
		double BurstMinS = 0.8;
		double BurstMaxS = 2.5;
		double PauseMinS = 0.6;
		double PauseMaxS = 2.0;
		double DispersionMrad = 4.0;
		int32 MaxRounds = 300;
		double TracerWidthM = 0.35;
		double TracerMinPx = 2.0;
		double TracerBrightness = 35.0;
		double TracerDayBrightness = 35.0;  // by day (blended by NightShare): a sub-pixel streak against the bright sky
		FLinearColor TracerColour = FLinearColor(1.0f, 0.45f, 0.15f);
		double MuzzleFlashM = 1.8;
		double MuzzleBrightness = 40.0;
		double MuzzleLightLumens = 4.0e6;
		double FlakJitterS = 0.4;
		double FlakOffsetM = 35.0;
		double DefaultMuzzleMps = 970.0;
		double DefaultRateRpm = 3400.0;
		// LOOK 14: the tracer's colour by the light, its length by the frame, which of the belt's rounds carry one.
		FLinearColor TracerDayColour = FLinearColor(1.0f, 0.81f, 0.53f);
		FLinearColor TracerNightColour = FLinearColor(1.0f, 0.068f, 0.018f);
		double TracerMinFrameS = 1.0 / 60.0;
		TArray<TPair<FString, int32>> TracedEvery;  // a munition id's prefix and every how many rounds is traced
		int32 TracedEveryDefault = 1;
		TArray<TPair<FString, int32>> BurstMinRounds;  // a munition id's prefix and the fewest rounds in its bursts
		// Gun smoke drifting from the mount (LOOK 14).
		double SmokeM = 3.0;
		double SmokeS = 6.0;
		FLinearColor SmokeColour = FLinearColor(0.58f, 0.56f, 0.53f);
		double SmokeOpacity = 0.4;
		int32 SmokePuffs = 2;
		double BarrelSpacingM = 0.5;  // the barrels' muzzles side by side: a flash at each (LOOK 14)
	} Guns;

	struct FWreck
	{
		double SmokeWidthM = 4.0;
		double SmokeSpreadMps = 1.5;
		double SmokeS = 50.0;
		double SmokePointM = 30.0;
		FLinearColor SmokeColour = FLinearColor(0.025f, 0.024f, 0.023f);
		double SmokeOpacity = 0.9;
		double FireM = 5.0;
		double FireBrightness = 25.0;
		double FireRate = 12.0;
		double KillChargeKg = 40.0;
		double KillFuelKg = 500.0;      // the fuel burning when a fighter blows up (LOOK 6: 0.3-1 t)
		int32 KillPieces = 24;          // LOOK 6: 10-40
		int32 KillBurningPieces = 8;
		double TorchLengthM = 25.0;     // a burning wreck's torch (LOOK 11: 15-50 m)
		double TorchWidthM = 4.0;
		double TorchNightShare = 0.12;  // the torch's brightness at night, a share of fire_brightness (by day 0.3)
		double NightLightLumens = 2.0e7; // the burning wreck's light at night
		double GroundChargeKg = 150.0;
		double GroundFireS = 300.0;
		double ColumnS = 900.0;
		double ColumnRate = 1.2;
		double ColumnPuffM = 10.0;
		double ColumnGrow = 4.0;
		double ColumnPuffS = 60.0;
		double ColumnRiseMps = 5.0;
		FLinearColor ColumnColour = FLinearColor(0.03f, 0.03f, 0.03f);
	} Wreck;

	struct FContrail
	{
		double Eta = 0.3;
		double EvalS = 0.5;
		double GapM = 30.0;
		double WidthM = 2.5;
		double SpreadScale = 1.0;
		FLinearColor Colour = FLinearColor(0.95f, 0.96f, 0.98f);
		double Opacity = 0.8;
		double ShortOpacity = 0.55;
		double MaxS = 600.0;
		double PointS = 0.5;
		int32 EnginesMax = 4;
		// LOOK 12: the plumes roll into the wingtip vortex pair (0.39 span each side), which sinks; Crow rings later.
		double VortexSpanShare = 0.39;
		double RollS = 2.0;
		double SinkMps = 2.5;
		double SinkMaxM = 270.0;
		double CrowSpans = 8.6;
		double CrowFromS = 30.0;
		double CrowFullS = 60.0;
		double TileM = 400.0;
		double WaveM = 1500.0;
		double ErodeFrom = 0.7;
		double TurbMps = 0.3;
	} Contrail;

	struct FAfterburner
	{
		double Mach = 0.95;
		double ClimbMps = 40.0;
		double WidthM = 1.1;
		double Brightness = 8.0;  // at night; by day DayBrightness
		double LightLumens = 2.0e6;
		double LightRangeM = 3000.0;
		// LOOK 13: short and pale by day, long, diamond-knotted and coloured by the engine at night.
		double DayLengthM = 3.0;
		double DayBrightness = 6.0;
		FLinearColor DayColour = FLinearColor(1.0f, 0.552f, 0.100f);
		double NightLengthM = 6.5;
		FLinearColor NightColour = FLinearColor(1.0f, 0.266f, 0.0075f);
		FLinearColor NightBlueColour = FLinearColor(0.195f, 0.262f, 1.0f);
		FLinearColor DiamondColour = FLinearColor(1.0f, 0.807f, 0.527f);
		int32 Diamonds = 5;
		double DiamondFirstM = 1.0;
		double DiamondSpacingM = 0.9;
		double GlowAftM = 0.6;          // the glow's centre aft of the nozzle
		double DiamondM = 0.5;
		double PointPx = 6.0;           // under this many pixels long the flame is drawn as its glow alone (a glint at night)
		double GlowM = 1.4;             // the glow's size at the nozzle
		double GlowShare = 0.5;         // its brightness, a share of the flame's
		double GlintShare = 1.0;        // at night the glow's brightness, a share of Brightness (seen as a point far off)
		double DiamondShare = 1.0;      // the diamonds' brightness, a share of Brightness
		double LightAftM = 5.0;         // the night light this far aft of the nozzle, in the flame
		double LightRadiusM = 30.0;
		TArray<FString> BlueModels;   // the aircraft whose engines' flames show blue (F100: the F-15, the F-16)
	} Afterburner;

	/** Read data/fx/effects.json (or any path); keys it lacks keep their defaults. False (defaults kept) if it doesn't
	 *  read or parse; OutError says why. */
	bool Load(const FString& Path, FString* OutError = nullptr);
};
