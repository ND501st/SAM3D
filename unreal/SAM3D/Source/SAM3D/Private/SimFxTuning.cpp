#include "SimFxTuning.h"

#include "Misc/FileHelper.h"
#include "SimCoreIncludes.h"

namespace
{
	/** Reads one section's keys into typed fields: a key the file lacks (or of the wrong type) keeps its default. */
	struct FSimFxTuningSection
	{
		const simcore::json::Value* Section = nullptr;

		const simcore::json::Value* Get(const char* Key) const
		{
			return Section != nullptr ? Section->Find(Key) : nullptr;
		}
		void Num(const char* Key, double& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V != nullptr && V->Is(simcore::json::Type::Number))
			{
				Out = V->number;
			}
		}
		void Int(const char* Key, int32& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V != nullptr && V->Is(simcore::json::Type::Number))
			{
				Out = static_cast<int32>(V->number);
			}
		}
		void Colour(const char* Key, FLinearColor& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V != nullptr && V->Is(simcore::json::Type::Array) && V->items.size() >= 3)
			{
				Out = FLinearColor(static_cast<float>(V->items[0].number), static_cast<float>(V->items[1].number),
					static_cast<float>(V->items[2].number));
			}
		}
		void Strings(const char* Key, TArray<FString>& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V != nullptr && V->Is(simcore::json::Type::Array))
			{
				Out.Reset();
				for (const simcore::json::Value& Item : V->items)
				{
					if (Item.Is(simcore::json::Type::String))
					{
						Out.Add(UTF8_TO_TCHAR(Item.string.c_str()));
					}
				}
			}
		}
		/** Rows of numbers: [[calibre, puff, hang, black], ...]. */
		void FlakRows(const char* Key, TArray<SimFxLogic::FFlakRow>& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V == nullptr || !V->Is(simcore::json::Type::Array))
			{
				return;
			}
			Out.Reset();
			for (const simcore::json::Value& Row : V->items)
			{
				if (Row.Is(simcore::json::Type::Array) && Row.items.size() >= 4)
				{
					SimFxLogic::FFlakRow R;
					R.CalibreMm = Row.items[0].number;
					R.PuffM = Row.items[1].number;
					R.HangS = Row.items[2].number;
					R.Black = Row.items[3].number;
					Out.Add(R);
				}
			}
			Out.Sort([](const SimFxLogic::FFlakRow& A, const SimFxLogic::FFlakRow& B) { return A.CalibreMm < B.CalibreMm; });
		}
		/** Pairs of a name and a whole number: [["shell_23_zu23", 5], ...]. */
		void NamedInts(const char* Key, TArray<TPair<FString, int32>>& Out) const
		{
			const simcore::json::Value* V = Get(Key);
			if (V == nullptr || !V->Is(simcore::json::Type::Array))
			{
				return;
			}
			Out.Reset();
			for (const simcore::json::Value& Row : V->items)
			{
				if (Row.Is(simcore::json::Type::Array) && Row.items.size() >= 2 && Row.items[0].Is(simcore::json::Type::String) &&
					Row.items[1].Is(simcore::json::Type::Number))
				{
					Out.Emplace(UTF8_TO_TCHAR(Row.items[0].string.c_str()), static_cast<int32>(Row.items[1].number));
				}
			}
		}
	};
}

bool FSimFxTuning::Load(const FString& Path, FString* OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT("cannot read %s"), *Path);
		}
		return false;
	}
	simcore::json::Value Root;
	std::string Error;
	if (!simcore::json::Parse(std::string(TCHAR_TO_UTF8(*Text)), Root, &Error) || !Root.Is(simcore::json::Type::Object))
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT("%s: %s"), *Path, UTF8_TO_TCHAR(Error.c_str()));
		}
		return false;
	}
	auto Section = [&Root](const char* Name)
	{
		FSimFxTuningSection S;
		S.Section = Root.Find(Name);
		return S;
	};

	{
		const FSimFxTuningSection S = Section("lod");
		S.Num("view_margin", Lod.ViewMargin);
		S.Num("far_ribbon_m", Lod.FarRibbonM);
		S.Num("glint_px", Lod.GlintPx);
		S.Num("glint_sprite_px", Lod.GlintSpritePx);
		S.Num("glint_keep", Lod.GlintKeep);
		S.Num("speck_px", Lod.SpeckPx);
		S.Num("munition_px", Lod.MunitionPx);
		S.Num("trail_margin_m", Lod.TrailMarginM);
		S.Num("contrail_range_scale", Lod.ContrailRangeScale);
	}
	{
		const FSimFxTuningSection S = Section("lighting");
		S.Num("sun_reference_lux", Lighting.SunReferenceLux);
		S.Num("ambient_day", Lighting.AmbientDay);
		S.Num("ambient_dusk", Lighting.AmbientDusk);
		S.Num("ambient_night", Lighting.AmbientNight);
		S.Colour("sky_tint", Lighting.SkyTint);
		S.Num("backlit", Lighting.Backlit);
		S.Num("smoke_sun", Lighting.SmokeSun);
		S.Num("smoke_sky", Lighting.SmokeSky);
		S.Num("smoke_sky_low", Lighting.SmokeSkyLow);
		S.Num("light_scale", Lighting.LightScale);
		S.Num("min_light_lumens", Lighting.MinLightLumens);
	}
	{
		const FSimFxTuningSection S = Section("explosion");
		FExplosion& E = Explosion;
		S.Num("flash_s", E.FlashS);
		S.Num("flash_brightness", E.FlashBrightness);
		S.Colour("flash_colour", E.FlashColour);
		S.Int("fire_sprites_full", E.FireSpritesFull);
		S.Num("fire_brightness", E.FireBrightness);
		S.Num("colour_flipbook_glow", E.ColourFlipbookGlow);
		S.Colour("air_smoke_colour", E.AirSmokeColour);
		S.Colour("dust_colour", E.DustColour);
		S.Num("smoke_opacity", E.SmokeOpacity);
		S.Num("debris_speed_mps", E.DebrisSpeedMps);
		S.Num("debris_size_m", E.DebrisSizeM);
		S.Colour("debris_colour", E.DebrisColour);
		S.Int("debris_max", E.DebrisMax);
		S.Int("sparks_max", E.SparksMax);
		S.Num("spark_speed_mps", E.SparkSpeedMps);
		S.Num("spark_s", E.SparkS);
		S.Num("spark_brightness", E.SparkBrightness);
		S.Colour("spark_colour", E.SparkColour);
		S.Num("ring_s", E.RingS);
		S.Num("ring_speed_mps", E.RingSpeedMps);
		S.Num("ring_brightness", E.RingBrightness);
		S.Colour("ring_colour", E.RingColour);
		S.Num("light_s", E.LightS);
		S.Colour("light_colour", E.LightColour);
		S.Colour("light_ember_colour", E.LightEmberColour);
		S.Num("light_white_s", E.LightWhiteS);
		S.Num("night_light_cap", E.NightLightCap);
		S.Num("crowd_radius_m", E.CrowdRadiusM);
		S.Num("crowd_s", E.CrowdS);
		S.Num("light_radius_per_fireball_m", E.LightRadiusPerFireballM);
		S.Num("scorch_per_crater", E.ScorchPerCrater);
		S.Num("scorch_min_m", E.ScorchMinM);
		S.Num("scorch_s", E.ScorchS);
		S.Num("day_light_cap", E.DayLightCap);
		S.Num("spike_s", E.SpikeS);
		S.Num("spike_fireballs", E.SpikeFireballs);
		S.Num("column_scale", E.ColumnScale);
		S.Int("column_members", E.ColumnMembers);
		S.Num("column_life_s", E.ColumnLifeS);
		S.Colour("column_low_colour", E.ColumnLowColour);
		S.Colour("column_high_colour", E.ColumnHighColour);
		S.Num("column_low_opacity", E.ColumnLowOpacity);
		S.Num("column_high_opacity", E.ColumnHighOpacity);
		S.Num("soot_darken", E.SootDarken);
		S.Strings("soot_munitions", E.SootMunitions);
		S.Int("ejecta_jets", E.EjectaJets);
		S.Num("ejecta_width_m", E.EjectaWidthM);
		S.Num("ejecta_s", E.EjectaS);
		S.Int("skirt_puffs", E.SkirtPuffs);
		S.Num("skirt_radius_m", E.SkirtRadiusM);
		S.Num("skirt_opacity", E.SkirtOpacity);
		S.Num("skirt_s", E.SkirtS);
		S.Int("clod_clumps", E.ClodClumps);
		S.Colour("masonry_colour", E.MasonryColour);
		S.Num("air_puff_s", E.AirPuffS);
		S.Colour("air_grey_colour", E.AirGreyColour);
		S.Num("air_spark_cone_deg", E.AirSparkConeDeg);
		S.Num("air_spark_reach_m", E.AirSparkReachM);
		S.Int("air_sparks", E.AirSparks);
		S.FlakRows("flak_by_calibre", E.FlakByCalibre);
		S.Colour("flak_light_colour", E.FlakLightColour);
		S.Colour("flak_black_colour", E.FlakBlackColour);
		S.Num("arm_burst_up_m", E.ArmBurstUpM);
		S.Int("arm_wp_pieces", E.ArmWpPieces);
		S.Colour("wp_colour", E.WpColour);
		S.Strings("wp_munitions", E.WpMunitions);
	}
	{
		const FSimFxTuningSection S = Section("missile");
		FMissile& M = Missile;
		S.Num("launch_flash_m", M.LaunchFlashM);
		S.Num("launch_flash_s", M.LaunchFlashS);
		S.Int("launch_cloud_puffs", M.LaunchCloudPuffs);
		S.Num("launch_cloud_m", M.LaunchCloudM);
		S.Num("launch_cloud_grow", M.LaunchCloudGrow);
		S.Num("launch_cloud_s", M.LaunchCloudS);
		S.Colour("launch_cloud_colour", M.LaunchCloudColour);
		S.Num("launch_cloud_opacity", M.LaunchCloudOpacity);
		S.Num("plume_boost_length_m", M.PlumeBoostLengthM);
		S.Num("plume_boost_width_m", M.PlumeBoostWidthM);
		S.Num("plume_sustain_length_m", M.PlumeSustainLengthM);
		S.Num("plume_sustain_width_m", M.PlumeSustainWidthM);
		S.Num("plume_brightness", M.PlumeBrightness);
		S.Colour("plume_colour", M.PlumeColour);
		S.Num("glow_boost_m", M.GlowBoostM);
		S.Num("glow_sustain_m", M.GlowSustainM);
		S.Num("trail_point_m", M.TrailPointM);
		S.Num("trail_width_dense_m", M.TrailWidthDenseM);
		S.Num("trail_width_reduced_m", M.TrailWidthReducedM);
		S.Num("trail_width_minimal_m", M.TrailWidthMinimalM);
		S.Num("trail_opacity_dense", M.TrailOpacityDense);
		S.Num("trail_opacity_reduced", M.TrailOpacityReduced);
		S.Num("trail_opacity_minimal", M.TrailOpacityMinimal);
		S.Num("trail_spread_mps", M.TrailSpreadMps);
		S.Colour("trail_colour", M.TrailColour);
		S.Num("trail_span_share_dense", M.TrailSpanShareDense);
		S.Num("trail_span_share_reduced", M.TrailSpanShareReduced);
		S.Num("trail_span_share_minimal", M.TrailSpanShareMinimal);
		S.Num("trail_spread_jitter", M.TrailSpreadJitter);
		S.Num("trail_turb_mps", M.TrailTurbMps);
		S.Num("trail_tile_m", M.TrailTileM);
		S.Num("trail_wave_m", M.TrailWaveM);
		S.Num("trail_thin_s", M.TrailThinS);
		S.Num("trail_thin_share", M.TrailThinShare);
		S.Num("trail_erode_from", M.TrailErodeFrom);
		S.Num("trail_rise_mps", M.TrailRiseMps);
		S.Num("launch_cloud_spread_m", M.LaunchCloudSpreadM);
		S.Int("launch_cloud_puffs_impostor", M.LaunchCloudPuffsImpostor);
		S.Num("helix_pitch_m", M.HelixPitchM);
		S.Num("helix_radius_m", M.HelixRadiusM);
		S.Num("helix_max_diameter_m", M.HelixMaxDiameterM);
		S.Num("helix_point_m", M.HelixPointM);
		S.Num("night_lit_trail_m", M.NightLitTrailM);
		S.Num("night_lit_trail_brightness", M.NightLitTrailBrightness);
		S.Num("default_smoke_s", M.DefaultSmokeS);
		S.Num("tracer_glow_m", M.TracerGlowM);
		S.Num("tracer_brightness", M.TracerBrightness);
		S.Colour("tracer_colour", M.TracerColour);
		S.Num("sam_speed_mps", M.SamSpeedMps);
		S.Num("arm_speed_mps", M.ArmSpeedMps);
		S.Num("default_charge_kg", M.DefaultChargeKg);
		S.Num("burst_at_missile_m", M.BurstAtMissileM);
	}
	{
		const FSimFxTuningSection S = Section("bombs");
		FBombs& B = Bombs;
		S.Num("ripple_s", B.RippleS);
		S.Int("max_per_release", B.MaxPerRelease);
		S.Num("impact_scatter_m", B.ImpactScatterM);
		S.Num("retarder_open_s", B.RetarderOpenS);
		S.Num("rocket_ripple_s", B.RocketRippleS);
		S.Num("rocket_smoke_s", B.RocketSmokeS);
		S.Int("cluster_bursts", B.ClusterBursts);
		S.Int("cluster_cloud_sprites", B.ClusterCloudSprites);
		S.Num("cluster_burst_kg", B.ClusterBurstKg);
		S.Num("cluster_spread_s", B.ClusterSpreadS);
		S.Int("cluster_bursts_max", B.ClusterBurstsMax);
		S.Int("cluster_haze_sprites", B.ClusterHazeSprites);
		S.Num("cluster_haze_s", B.ClusterHazeS);
		S.Num("cluster_haze_m", B.ClusterHazeM);
		S.Colour("cluster_haze_colour", B.ClusterHazeColour);
		S.Num("gun_dust_m", B.GunDustM);
		S.Num("gun_walk_mps", B.GunWalkMps);
		S.Num("muzzle_flash_m", B.MuzzleFlashM);
	}
	{
		const FSimFxTuningSection S = Section("guns");
		FGuns& G = Guns;
		S.Num("burst_min_s", G.BurstMinS);
		S.Num("burst_max_s", G.BurstMaxS);
		S.Num("pause_min_s", G.PauseMinS);
		S.Num("pause_max_s", G.PauseMaxS);
		S.Num("dispersion_mrad", G.DispersionMrad);
		S.Int("max_rounds", G.MaxRounds);
		S.Num("tracer_width_m", G.TracerWidthM);
		S.Num("tracer_min_px", G.TracerMinPx);
		S.Num("tracer_brightness", G.TracerBrightness);
		S.Num("tracer_day_brightness", G.TracerDayBrightness);
		S.Colour("tracer_colour", G.TracerColour);
		S.Num("muzzle_flash_m", G.MuzzleFlashM);
		S.Num("muzzle_brightness", G.MuzzleBrightness);
		S.Num("muzzle_light_lumens", G.MuzzleLightLumens);
		S.Num("flak_jitter_s", G.FlakJitterS);
		S.Num("flak_offset_m", G.FlakOffsetM);
		S.Num("default_muzzle_mps", G.DefaultMuzzleMps);
		S.Num("default_rate_rpm", G.DefaultRateRpm);
		S.Colour("tracer_day_colour", G.TracerDayColour);
		S.Colour("tracer_night_colour", G.TracerNightColour);
		S.Num("tracer_min_frame_s", G.TracerMinFrameS);
		S.NamedInts("traced_every", G.TracedEvery);
		S.Int("traced_every_default", G.TracedEveryDefault);
		S.NamedInts("burst_min_rounds", G.BurstMinRounds);
		S.Num("smoke_m", G.SmokeM);
		S.Num("smoke_s", G.SmokeS);
		S.Colour("smoke_colour", G.SmokeColour);
		S.Num("smoke_opacity", G.SmokeOpacity);
		S.Int("smoke_puffs", G.SmokePuffs);
		S.Num("barrel_spacing_m", G.BarrelSpacingM);
	}
	{
		const FSimFxTuningSection S = Section("air_battle");
		FAirBattle& A = AirBattle;
		S.Int("chaff_puffs_per_cartridge", A.ChaffPuffsPerCartridge);
		S.Int("chaff_puffs_max", A.ChaffPuffsMax);
		S.Num("chaff_bloom_m", A.ChaffBloomM);
		S.Num("chaff_bloom_s", A.ChaffBloomS);
		S.Num("chaff_s", A.ChaffS);
		S.Num("chaff_spread_m", A.ChaffSpreadM);
		S.Num("chaff_drag_per_s", A.ChaffDragPerS);
		S.Num("chaff_opacity", A.ChaffOpacity);
		S.Colour("chaff_colour", A.ChaffColour);
		S.Int("chaff_glints", A.ChaffGlints);
		S.Num("flare_burn_s", A.FlareBurnS);
		S.Num("flare_eject_mps", A.FlareEjectMps);
		S.Num("flare_drag_per_s", A.FlareDragPerS);
		S.Num("flare_size_m", A.FlareSizeM);
		S.Num("flare_brightness", A.FlareBrightness);
		S.Colour("flare_colour", A.FlareColour);
		S.Num("flare_lumens", A.FlareLumens);
		S.Num("flare_salvo_s", A.FlareSalvoS);
		S.Int("flares_max", A.FlaresMax);
		S.Num("flare_smoke_width_m", A.FlareSmokeWidthM);
		S.Num("flare_smoke_s", A.FlareSmokeS);
		S.Num("flare_smoke_point_m", A.FlareSmokePointM);
		S.Colour("flare_smoke_colour", A.FlareSmokeColour);
		S.Num("tumble_deg_per_s", A.TumbleDegPerS);
		S.Num("aam_fly_on_s", A.AamFlyOnS);
		S.Num("aam_turn_per_s", A.AamTurnPerS);
		S.Num("air_gun_dispersion_mrad", A.AirGunDispersionMrad);
	}
	{
		const FSimFxTuningSection S = Section("wreck");
		FWreck& W = Wreck;
		S.Num("smoke_width_m", W.SmokeWidthM);
		S.Num("smoke_spread_mps", W.SmokeSpreadMps);
		S.Num("smoke_s", W.SmokeS);
		S.Num("smoke_point_m", W.SmokePointM);
		S.Colour("smoke_colour", W.SmokeColour);
		S.Num("smoke_opacity", W.SmokeOpacity);
		S.Num("fire_m", W.FireM);
		S.Num("fire_brightness", W.FireBrightness);
		S.Num("fire_rate", W.FireRate);
		S.Num("kill_charge_kg", W.KillChargeKg);
		S.Num("kill_fuel_kg", W.KillFuelKg);
		S.Int("kill_pieces", W.KillPieces);
		S.Int("kill_burning_pieces", W.KillBurningPieces);
		S.Num("torch_length_m", W.TorchLengthM);
		S.Num("torch_width_m", W.TorchWidthM);
		S.Num("torch_night_share", W.TorchNightShare);
		S.Num("night_light_lumens", W.NightLightLumens);
		S.Num("ground_charge_kg", W.GroundChargeKg);
		S.Num("ground_fire_s", W.GroundFireS);
		S.Num("column_s", W.ColumnS);
		S.Num("column_rate", W.ColumnRate);
		S.Num("column_puff_m", W.ColumnPuffM);
		S.Num("column_grow", W.ColumnGrow);
		S.Num("column_puff_s", W.ColumnPuffS);
		S.Num("column_rise_mps", W.ColumnRiseMps);
		S.Colour("column_colour", W.ColumnColour);
	}
	{
		const FSimFxTuningSection S = Section("contrail");
		FContrail& C = Contrail;
		S.Num("eta", C.Eta);
		S.Num("eval_s", C.EvalS);
		S.Num("gap_m", C.GapM);
		S.Num("width_m", C.WidthM);
		S.Num("spread_scale", C.SpreadScale);
		S.Colour("colour", C.Colour);
		S.Num("opacity", C.Opacity);
		S.Num("short_opacity", C.ShortOpacity);
		S.Num("max_s", C.MaxS);
		S.Num("point_s", C.PointS);
		S.Int("engines_max", C.EnginesMax);
		S.Num("vortex_span_share", C.VortexSpanShare);
		S.Num("roll_s", C.RollS);
		S.Num("sink_mps", C.SinkMps);
		S.Num("sink_max_m", C.SinkMaxM);
		S.Num("crow_spans", C.CrowSpans);
		S.Num("crow_from_s", C.CrowFromS);
		S.Num("crow_full_s", C.CrowFullS);
		S.Num("tile_m", C.TileM);
		S.Num("wave_m", C.WaveM);
		S.Num("erode_from", C.ErodeFrom);
		S.Num("turb_mps", C.TurbMps);
	}
	{
		const FSimFxTuningSection S = Section("afterburner");
		FAfterburner& A = Afterburner;
		S.Num("mach", A.Mach);
		S.Num("climb_mps", A.ClimbMps);
		S.Num("width_m", A.WidthM);
		S.Num("brightness", A.Brightness);
		S.Num("light_lumens", A.LightLumens);
		S.Num("light_range_m", A.LightRangeM);
		S.Num("day_length_m", A.DayLengthM);
		S.Num("day_brightness", A.DayBrightness);
		S.Colour("day_colour", A.DayColour);
		S.Num("night_length_m", A.NightLengthM);
		S.Colour("night_colour", A.NightColour);
		S.Colour("night_blue_colour", A.NightBlueColour);
		S.Colour("diamond_colour", A.DiamondColour);
		S.Int("diamonds", A.Diamonds);
		S.Num("diamond_first_m", A.DiamondFirstM);
		S.Num("diamond_spacing_m", A.DiamondSpacingM);
		S.Num("glow_aft_m", A.GlowAftM);
		S.Num("diamond_m", A.DiamondM);
		S.Num("point_px", A.PointPx);
		S.Num("glow_m", A.GlowM);
		S.Num("glow_share", A.GlowShare);
		S.Num("glint_share", A.GlintShare);
		S.Num("diamond_share", A.DiamondShare);
		S.Num("light_aft_m", A.LightAftM);
		S.Num("light_radius_m", A.LightRadiusM);
		S.Strings("blue_models", A.BlueModels);
	}
	return true;
}
