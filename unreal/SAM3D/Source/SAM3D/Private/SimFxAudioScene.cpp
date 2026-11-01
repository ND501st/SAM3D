#include "SimFxAudioScene.h"

#include <algorithm>
#include <cmath>

namespace SimFxAudio
{
	namespace
	{
		using simcore::json::Type;
		using simcore::json::Value;

		FString SimFxAudioToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

		double SimFxAudioNum(const Value* Obj, const char* Key, double Default)
		{
			const Value* V = Obj != nullptr ? Obj->Find(Key) : nullptr;
			return V != nullptr && V->Is(Type::Number) ? V->number : Default;
		}

		FString SimFxAudioStr(const Value* Obj, const char* Key, const FString& Default = FString())
		{
			const Value* V = Obj != nullptr ? Obj->Find(Key) : nullptr;
			return V != nullptr && V->Is(Type::String) ? SimFxAudioToF(V->string) : Default;
		}

		FName SimFxAudioName(const Value* Obj, const char* Key)
		{
			const FString S = SimFxAudioStr(Obj, Key);
			return S.IsEmpty() ? NAME_None : FName(*S);
		}

		FEventSoundDef SimFxAudioEvent(const Value* Obj, const char* Key, const FEventSoundDef& Default)
		{
			const Value* V = Obj != nullptr ? Obj->Find(Key) : nullptr;
			if (V == nullptr || !V->Is(Type::Object))
			{
				return Default;
			}
			FEventSoundDef E;
			E.Sample = SimFxAudioName(V, "sample");
			E.LevelDb1m = SimFxAudioNum(V, "level_db_1m", Default.LevelDb1m);
			E.Pitch = SimFxAudioNum(V, "pitch", 1.0);
			return E;
		}

		// The sum of loss and spread is a level change; levels below this are nothing.
		constexpr double SimFxAudioSilentDb = -1000.0;
	}

	void BandGains(double DistanceM, double NearM, double FarM, double& OutNear, double& OutFar)
	{
		const double W = FMath::Clamp((DistanceM - NearM) / FMath::Max(FarM - NearM, 1.0), 0.0, 1.0);
		OutNear = std::cos(W * UE_DOUBLE_HALF_PI);
		OutFar = std::sin(W * UE_DOUBLE_HALF_PI);
	}

	// ============================================================================================ the bank

	bool FBank::LoadJson(const std::string& Text, FString& OutError)
	{
		bLoaded = false;
		Value Root;
		std::string Error;
		if (!simcore::json::Parse(Text, Root, &Error))
		{
			OutError = SimFxAudioToF(Error);
			return false;
		}
		if (!Root.Is(Type::Object))
		{
			OutError = TEXT("not an object");
			return false;
		}

		const Value* Mix = Root.Find("mix");
		FloorDb = SimFxAudioNum(Mix, "floor_db", FloorDb);
		DepthDb = SimFxAudioNum(Mix, "depth_db", DepthDb);
		Slope = SimFxAudioNum(Mix, "slope", Slope);
		AttackDbPerS = SimFxAudioNum(Mix, "attack_db_per_s", AttackDbPerS);
		ReleaseDbPerS = SimFxAudioNum(Mix, "release_db_per_s", ReleaseDbPerS);
		HysteresisDb = SimFxAudioNum(Mix, "hysteresis_db", HysteresisDb);
		LoopFadeS = SimFxAudioNum(Mix, "loop_fade_s", LoopFadeS);
		OneShotFadeS = SimFxAudioNum(Mix, "one_shot_fade_s", OneShotFadeS);
		OneShotHoldS = SimFxAudioNum(Mix, "one_shot_hold_s", OneShotHoldS);
		OneShotDecayDbPerS = SimFxAudioNum(Mix, "one_shot_decay_db_per_s", OneShotDecayDbPerS);
		PeakToLevelDb = SimFxAudioNum(Mix, "peak_to_level_db", PeakToLevelDb);

		const Value* Prop = Root.Find("propagation");
		NearM = SimFxAudioNum(Prop, "near_m", NearM);
		FarM = FMath::Max(SimFxAudioNum(Prop, "far_m", FarM), NearM + 1.0);
		MaxRangeM = SimFxAudioNum(Prop, "max_range_m", MaxRangeM);
		OneShotRangeM = SimFxAudioNum(Prop, "one_shot_range_m", OneShotRangeM);
		AbsorptionBudgetDb = SimFxAudioNum(Prop, "absorption_budget_db", AbsorptionBudgetDb);
		VoiceDistanceM = SimFxAudioNum(Prop, "voice_distance_m", VoiceDistanceM);
		ListenerMaxMps = SimFxAudioNum(Prop, "listener_max_mps", ListenerMaxMps);
		ListenerJumpM = SimFxAudioNum(Prop, "listener_jump_m", ListenerJumpM);
		ListenerSmoothS = SimFxAudioNum(Prop, "listener_smooth_s", ListenerSmoothS);
		TrackSampleS = SimFxAudioNum(Prop, "track_sample_s", TrackSampleS);
		GroundListenerAglM = SimFxAudioNum(Prop, "ground_listener_agl_m", GroundListenerAglM);

		const Value* Cabin = Root.Find("enclosure");
		CabinCutoffHz = SimFxAudioNum(Cabin, "cutoff_hz", CabinCutoffHz);
		CabinLossDb = SimFxAudioNum(Cabin, "loss_db", CabinLossDb);
		CabinNearM = SimFxAudioNum(Cabin, "near_m", CabinNearM);
		CabinNearLossDb = SimFxAudioNum(Cabin, "near_loss_db", CabinNearLossDb);

		Profiles.Reset();
		if (const Value* P = Root.Find("profiles"); P != nullptr && P->Is(Type::Object))
		{
			for (size_t I = 0; I < P->keys.size(); ++I)
			{
				const Value& Def = P->items[I];
				if (!Def.Is(Type::Object))
				{
					continue;
				}
				FProfileDef NewProfile;
				const FString Id = SimFxAudioToF(P->keys[I]);
				NewProfile.Id = Id;
				NewProfile.SustainOffsetDb = SimFxAudioNum(&Def, "sustain_offset_db", -6.0);
				NewProfile.NearM = SimFxAudioNum(&Def, "near_m", -1.0);
				NewProfile.FarM = SimFxAudioNum(&Def, "far_m", -1.0);
				if (const Value* Layers = Def.Find("layers"); Layers != nullptr && Layers->Is(Type::Array))
				{
					for (const Value& L : Layers->items)
					{
						FLayerDef Layer;
						Layer.Sample = SimFxAudioName(&L, "sample");
						Layer.LevelDb1m = SimFxAudioNum(&L, "level_db_1m", 140.0);
						Layer.Pitch = SimFxAudioNum(&L, "pitch", 1.0);
						const FString Dir = SimFxAudioStr(&L, "directivity", TEXT("omni"));
						Layer.Directivity = Dir == TEXT("roar") ? EDirectivity::Roar : Dir == TEXT("whine") ? EDirectivity::Whine : EDirectivity::Omni;
						Layer.When = SimFxAudioStr(&L, "when") == TEXT("supersonic") ? EWhen::Supersonic : EWhen::Always;
						const FString Band = SimFxAudioStr(&L, "band", TEXT("all"));
						Layer.Band = Band == TEXT("near") ? EBand::Near : Band == TEXT("far") ? EBand::Far : EBand::All;
						if (!Layer.Sample.IsNone())
						{
							NewProfile.Layers.Add(Layer);
						}
					}
				}
				Profiles.Add(Id, MoveTemp(NewProfile));
			}
		}
		if (Profiles.Num() == 0)
		{
			OutError = TEXT("no profiles");
			return false;
		}

		const auto ReadMap = [](const Value* Obj, TMap<FString, FString>& Out)
		{
			Out.Reset();
			if (Obj == nullptr || !Obj->Is(Type::Object))
			{
				return;
			}
			for (size_t I = 0; I < Obj->keys.size(); ++I)
			{
				if (Obj->keys[I] != "source" && Obj->items[I].Is(Type::String))
				{
					Out.Add(SimFxAudioToF(Obj->keys[I]), SimFxAudioToF(Obj->items[I].string));
				}
			}
		};
		ReadMap(Root.Find("models"), ModelProfiles);
		const Value* Arch = Root.Find("archetypes");
		ReadMap(Arch, ArchetypeProfiles);
		ArchetypeProfiles.Remove(TEXT("heavy_profile"));
		HeavyJetLengthM = SimFxAudioNum(Arch, "heavy_jet_length_m", HeavyJetLengthM);
		HeavyProfile = SimFxAudioStr(Arch, "heavy_profile", HeavyProfile);

		const Value* Missiles = Root.Find("missiles");
		MissileProfile = SimFxAudioStr(Missiles, "profile", MissileProfile);
		LargeRocketProfile = SimFxAudioStr(Missiles, "large_profile", LargeRocketProfile);
		LargeRocketFromLengthM = SimFxAudioNum(Missiles, "large_from_length_m", LargeRocketFromLengthM);
		MissileBoostS = SimFxAudioNum(Missiles, "boost_s", MissileBoostS);
		MissileSustainS = SimFxAudioNum(Missiles, "sustain_s", MissileSustainS);
		FlyerMps = FMath::Max(SimFxAudioNum(Missiles, "flyer_mps", FlyerMps), 50.0);

		RecordedAtM.Reset();
		if (const Value* Rec = Root.Find("recordings"); Rec != nullptr && Rec->Is(Type::Object))
		{
			for (size_t I = 0; I < Rec->keys.size(); ++I)
			{
				if (Rec->items[I].Is(Type::Object))
				{
					RecordedAtM.Add(FName(*SimFxAudioToF(Rec->keys[I])), FMath::Max(SimFxAudioNum(&Rec->items[I], "recorded_at_m", 0.0), 0.0));
				}
			}
		}

		const Value* Ev = Root.Find("events");
		Launch = SimFxAudioEvent(Ev, "launch", Launch);
		ArmLaunch = SimFxAudioEvent(Ev, "arm_launch", ArmLaunch);
		RocketLaunch = SimFxAudioEvent(Ev, "rocket_launch", RocketLaunch);
		BombRush = SimFxAudioEvent(Ev, "bomb_rush", BombRush);
		Debris = SimFxAudioEvent(Ev, "debris", Debris);
		Cluster = SimFxAudioEvent(Ev, "cluster", Cluster);
		Crack = SimFxAudioEvent(Ev, "crack", Crack);
		Fireball = SimFxAudioEvent(Ev, "fireball", Fireball);
		SdPopsPerBurst = static_cast<int32>(SimFxAudioNum(Ev, "sd_pops_per_burst", SdPopsPerBurst));
		FlakDefaultKg = SimFxAudioNum(Ev, "flak_default_kg", FlakDefaultKg);
		RushLeadS = SimFxAudioNum(Ev, "rush_lead_s", RushLeadS);
		RushRangeM = SimFxAudioNum(Ev, "rush_range_m", RushRangeM);
		DebrisRangeM = SimFxAudioNum(Ev, "debris_range_m", DebrisRangeM);
		RippleIntervalS = SimFxAudioNum(Ev, "ripple_interval_s", RippleIntervalS);
		RippleMax = static_cast<int32>(SimFxAudioNum(Ev, "ripple_max", RippleMax));
		StickIntervalS = SimFxAudioNum(Ev, "stick_interval_s", StickIntervalS);
		StickMax = static_cast<int32>(SimFxAudioNum(Ev, "stick_max", StickMax));
		StickSpreadM = SimFxAudioNum(Ev, "stick_spread_m", StickSpreadM);
		KillBlastKg = SimFxAudioNum(Ev, "kill_blast_kg", KillBlastKg);
		WarheadDefaultKg = SimFxAudioNum(Ev, "warhead_default_kg", WarheadDefaultKg);
		SamDefaultKg = SimFxAudioNum(Ev, "sam_default_kg", SamDefaultKg);
		ArmDefaultKg = SimFxAudioNum(Ev, "arm_default_kg", ArmDefaultKg);
		CruiseDefaultKg = SimFxAudioNum(Ev, "cruise_default_kg", CruiseDefaultKg);
		ChargeShareOfMass = SimFxAudioNum(Ev, "charge_share_of_mass", ChargeShareOfMass);
		GunImpactKg = SimFxAudioNum(Ev, "gun_impact_kg", GunImpactKg);
		GunImpactBursts = static_cast<int32>(SimFxAudioNum(Ev, "gun_impact_bursts", GunImpactBursts));
		AirBurstAglM = SimFxAudioNum(Ev, "air_burst_agl_m", AirBurstAglM);
		CrackRangeM = SimFxAudioNum(Ev, "crack_range_m", CrackRangeM);
		WreckGroundFireS = SimFxAudioNum(Ev, "wreck_ground_fire_s", WreckGroundFireS);

		Blasts.Reset();
		if (const Value* B = Root.Find("blasts"); B != nullptr && B->Is(Type::Array))
		{
			for (const Value& C : B->items)
			{
				FBlastClassDef Class;
				Class.MaxKg = SimFxAudioNum(&C, "max_kg", 1.0e9);
				Class.Near = SimFxAudioName(&C, "near");
				Class.Far = SimFxAudioName(&C, "far");
				Class.AirNear = SimFxAudioName(&C, "air_near");
				Class.AirFar = SimFxAudioName(&C, "air_far");
				Class.RefKg = SimFxAudioNum(&C, "ref_kg", 100.0);
				Class.Tail = SimFxAudioName(&C, "tail");
				Class.TailDelayS = FMath::Max(SimFxAudioNum(&C, "tail_delay_s", Class.TailDelayS), 0.0);
				Class.TailDb = SimFxAudioNum(&C, "tail_db", Class.TailDb);
				if (Class.AirNear.IsNone())
				{
					Class.AirNear = Class.Near;
				}
				if (Class.AirFar.IsNone())
				{
					Class.AirFar = Class.Far;
				}
				Blasts.Add(Class);
			}
			Blasts.Sort([](const FBlastClassDef& A, const FBlastClassDef& C) { return A.MaxKg < C.MaxKg; });
		}
		if (Blasts.Num() == 0)
		{
			OutError = TEXT("no blasts");
			return false;
		}
		BlastPitchExponent = SimFxAudioNum(&Root, "blast_pitch_exponent", BlastPitchExponent);

		Guns.Reset();
		if (const Value* G = Root.Find("guns"); G != nullptr && G->Is(Type::Object))
		{
			for (size_t I = 0; I < G->keys.size(); ++I)
			{
				const Value& Def = G->items[I];
				if (!Def.Is(Type::Object))
				{
					continue;
				}
				FGunDef Gun;
				Gun.Loop = SimFxAudioName(&Def, "loop");
				Gun.Single = SimFxAudioName(&Def, "single");
				Gun.Tail = SimFxAudioName(&Def, "tail");
				Gun.LevelDb1m = SimFxAudioNum(&Def, "level_db_1m", Gun.LevelDb1m);
				Gun.Pitch = SimFxAudioNum(&Def, "pitch", 1.0);
				Gun.RateHz = FMath::Max(SimFxAudioNum(&Def, "rate_hz", Gun.RateHz), 0.01);
				Gun.MuzzleMps = SimFxAudioNum(&Def, "muzzle_mps", Gun.MuzzleMps);
				Guns.Add(SimFxAudioToF(G->keys[I]), Gun);
			}
		}

		const Value* Booms = Root.Find("booms");
		BoomShort = SimFxAudioName(Booms, "short");
		BoomLong = SimFxAudioName(Booms, "long");
		if (BoomLong.IsNone())
		{
			BoomLong = BoomShort;
		}
		BoomLongFromLengthM = SimFxAudioNum(Booms, "long_from_length_m", BoomLongFromLengthM);
		BoomShapeFactor = SimFxAudioNum(Booms, "shape_factor", BoomShapeFactor);
		BoomDefaultLengthM = SimFxAudioNum(Booms, "default_length_m", BoomDefaultLengthM);
		BoomLevelOffsetDb = SimFxAudioNum(Booms, "level_offset_db", BoomLevelOffsetDb);

		bLoaded = true;
		return true;
	}

	const FProfileDef* FBank::ProfileFor(const FString& Model, const FString& Archetype, double LengthM) const
	{
		if (const FString* P = ModelProfiles.Find(Model))
		{
			if (const FProfileDef* Def = Profiles.Find(*P))
			{
				return Def;
			}
		}
		if (Archetype == TEXT("jet") && LengthM >= HeavyJetLengthM)
		{
			if (const FProfileDef* Def = Profiles.Find(HeavyProfile))
			{
				return Def;
			}
		}
		if (const FString* P = ArchetypeProfiles.Find(Archetype))
		{
			return Profiles.Find(*P);
		}
		return nullptr;
	}

	const FBlastClassDef* FBank::BlastFor(double ChargeKg) const
	{
		for (const FBlastClassDef& C : Blasts)
		{
			if (ChargeKg <= C.MaxKg)
			{
				return &C;
			}
		}
		return Blasts.Num() > 0 ? &Blasts.Last() : nullptr;
	}

	const FGunDef* FBank::GunFor(const FString& Id) const
	{
		if (const FGunDef* G = Guns.Find(Id))
		{
			return G;
		}
		return Guns.Find(TEXT("default"));
	}

	void FBank::AllSamples(TSet<FName>& Out) const
	{
		const auto Add = [&Out](FName N)
		{
			if (!N.IsNone())
			{
				Out.Add(N);
			}
		};
		for (const TPair<FString, FProfileDef>& P : Profiles)
		{
			for (const FLayerDef& L : P.Value.Layers)
			{
				Add(L.Sample);
			}
		}
		for (const FEventSoundDef* E : {&Launch, &ArmLaunch, &RocketLaunch, &BombRush, &Debris, &Cluster, &Crack, &Fireball})
		{
			Add(E->Sample);
		}
		for (const FBlastClassDef& B : Blasts)
		{
			Add(B.Near);
			Add(B.Far);
			Add(B.AirNear);
			Add(B.AirFar);
			Add(B.Tail);
		}
		for (const TPair<FString, FGunDef>& G : Guns)
		{
			Add(G.Value.Loop);
			Add(G.Value.Single);
			Add(G.Value.Tail);
		}
		Add(BoomShort);
		Add(BoomLong);
	}

	// ============================================================================================ the air

	void FAir::Build(TFunctionRef<simcore::AtmosphereState(double)> InAir, double ListenerAltM, double InRhPct, double BudgetDb)
	{
		const auto AirAt = [&InAir](double Alt) { return InAir(Alt); };
		for (int32 I = 0; I < NumAlt; ++I)
		{
			const double Alt = AltMin + AltStep * I;
			MeanC[I] = simcore::fx::MeanSoundSpeed(AirAt, ListenerAltM, Alt);
			const simcore::AtmosphereState S = InAir(Alt);
			LocalC[I] = simcore::fx::SpeedOfSound(S.temperature_k);
			Pressure[I] = S.pressure_pa;
		}
		const simcore::AtmosphereState L = InAir(ListenerAltM);
		ListenerP = L.pressure_pa;
		for (int32 J = 0; J < NumDist; ++J)
		{
			const double D = std::exp(std::log(1.0e5) * J / (NumDist - 1));
			Cutoff[J] = simcore::fx::AbsorptionCutoffHz(D, L.temperature_k, InRhPct, L.pressure_pa, BudgetDb);
		}
		ListenerAlt = ListenerAltM;
		bBuilt = true;
	}

	double FAir::Lookup(const double* Table, double AltM) const
	{
		const double F = FMath::Clamp((AltM - AltMin) / AltStep, 0.0, static_cast<double>(NumAlt - 1));
		const int32 I = FMath::Min(static_cast<int32>(F), NumAlt - 2);
		const double U = F - I;
		return Table[I] + (Table[I + 1] - Table[I]) * U;
	}

	double FAir::CutoffHz(double DistanceM) const
	{
		const double F = FMath::Clamp(std::log(FMath::Max(DistanceM, 1.0)) / std::log(1.0e5) * (NumDist - 1), 0.0,
			static_cast<double>(NumDist - 1));
		const int32 I = FMath::Min(static_cast<int32>(F), NumDist - 2);
		const double U = F - I;
		return std::exp(std::log(Cutoff[I]) + (std::log(Cutoff[I + 1]) - std::log(Cutoff[I])) * U);
	}

	// ============================================================================================ the scene

	struct FScene::FSource
	{
		uint64 Key = 0;
		const FProfileDef* Profile = nullptr;
		simcore::fx::SourceTrack Track{125.0};
		simcore::fx::BoomWatch Watch;
		bool bWatchPrimed = false;
		double TrackStartS = 0.0;
		double LastPushS = -1.0e300;
		Vec3 NewestEnu;
		double NewestAltM = 0.0;
		double AglM = 1000.0;
		double LengthM = 15.0;
		double MaxLevelDb1m = 0.0;
		double OnS = -1.0e300;
		double SustainS = 1.0e300;
		double OffS = 1.0e300;
		bool bEnded = false;
		uint64 LastFrame = 0;
		bool bHeard = false;
	};

	struct FScene::FOneShot
	{
		uint64 Key = 0;
		FOneShotDesc D;
		double LengthS = 0.0;
		bool bStarted = false;
		double StartedAtS = -1.0;
		double AgeS = 0.0;
	};

	FScene::FScene() = default;
	FScene::~FScene() = default;

	void FScene::SetBank(const FBank* InBank)
	{
		Bank = InBank;
		if (Bank != nullptr)
		{
			simcore::fx::LoudnessWindow::Params P;
			P.floor_db = Bank->FloorDb;
			P.depth_db = Bank->DepthDb;
			P.slope = Bank->Slope;
			P.attack_db_per_s = Bank->AttackDbPerS;
			P.release_db_per_s = Bank->ReleaseDbPerS;
			Window = simcore::fx::LoudnessWindow(P);
		}
	}

	void FScene::SetSampleSeconds(FName Sample, double Seconds)
	{
		if (!Sample.IsNone() && Seconds > 0.0)
		{
			SampleSeconds.Add(Sample, Seconds);
		}
	}

	double FScene::SampleSecondsOf(FName Sample) const
	{
		const double* S = Sample.IsNone() ? nullptr : SampleSeconds.Find(Sample);
		return S != nullptr ? *S : 0.0;
	}

	void FScene::SetAllSamplesAvailable(double Seconds)
	{
		if (Bank == nullptr)
		{
			return;
		}
		TSet<FName> All;
		Bank->AllSamples(All);
		for (const FName N : All)
		{
			SetSampleSeconds(N, Seconds);
		}
	}

	void FScene::SetAir(TFunction<simcore::AtmosphereState(double)> InAir, double InRhPct)
	{
		AirFn = MoveTemp(InAir);
		RhPct = InRhPct;
		AirAgeS = 1.0e9;  // rebuild at the next frame
	}

	void FScene::Reset()
	{
		Sources.Empty();
		OneShots.Empty();
		Candidates.Reset();
		for (FVoice& V : Voices)
		{
			V.bActive = false;
			V.bReleasing = false;
			V.Volume = 0.0;
			V.Owner = 0;
			V.Layer = -1;
		}
		SetBank(Bank);
		bHaveListener = false;
		ListenerVel = Vec3{};
		Stats = FStats();
	}

	bool FScene::HasSource(uint64 Key) const { return Sources.Contains(Key); }
	int32 FScene::NumOneShots() const { return OneShots.Num(); }
	double FScene::WindowTopDb() const { return Window.TopDb(); }

	void FScene::RebuildAirIfNeeded()
	{
		if (Bank == nullptr)
		{
			return;
		}
		if (Air.IsBuilt() && AirAgeS < 5.0 && FMath::Abs(Air.BuiltForAltM() - ListenerNow.AltM) < 50.0)
		{
			return;
		}
		if (AirFn)
		{
			Air.Build([this](double A) { return AirFn(A); }, ListenerNow.AltM, RhPct, Bank->AbsorptionBudgetDb);
		}
		else
		{
			Air.Build([](double A) { return simcore::StandardAtmosphere(A); }, ListenerNow.AltM, RhPct, Bank->AbsorptionBudgetDb);
		}
		AirAgeS = 0.0;
	}

	void FScene::BeginFrame(double InNowS, double InRealDtS, const FListener& Listener)
	{
		++Frame;
		RealDtS = FMath::Max(InRealDtS, 0.0);
		PrevNowS = bHaveListener ? NowS : InNowS;
		NowS = InNowS;
		if (bHaveListener && NowS < PrevNowS - 0.5)
		{
			// The clock went back (a restart): nothing heard so far belongs to this time.
			Reset();
			PrevNowS = NowS;
		}
		bListenerJumped = false;
		if (bHaveListener && Bank != nullptr)
		{
			const Vec3 Step = Listener.Enu - ListenerNow.Enu;
			const double DtSim = NowS - PrevNowS;
			if (Step.Length() > Bank->ListenerJumpM)
			{
				// The camera jumped (a new view): no Doppler from it, and no boom from the sudden change of place.
				ListenerVel = Vec3{};
				bListenerJumped = true;
			}
			else if (DtSim > 1.0e-4)
			{
				const Vec3 Raw = Step / DtSim;
				const double Alpha = 1.0 - std::exp(-DtSim / FMath::Max(Bank->ListenerSmoothS, 1.0e-3));
				ListenerVel = ListenerVel + (Raw - ListenerVel) * Alpha;
				const double Speed = ListenerVel.Length();
				if (Speed > Bank->ListenerMaxMps)
				{
					ListenerVel = ListenerVel * (Bank->ListenerMaxMps / Speed);
				}
			}
		}
		ListenerPrev = ListenerNow.Enu;
		ListenerNow = Listener;
		bHaveListener = true;
		AirAgeS += RealDtS;
		RebuildAirIfNeeded();
	}

	void FScene::UpdateSource(const FSourceUpdate& U)
	{
		if (Bank == nullptr || U.Profile == nullptr)
		{
			return;
		}
		FSource* S = nullptr;
		if (TUniquePtr<FSource>* Found = Sources.Find(U.Key))
		{
			S = Found->Get();
			if (U.TSimS < S->LastPushS - 1.0e-6)
			{
				// Its clock went back: start its track again.
				S->Track.Clear();
				S->Track.Push(U.TSimS, U.Enu);
				S->TrackStartS = U.TSimS;
				S->LastPushS = U.TSimS;
				S->bWatchPrimed = false;
			}
			else if (U.TSimS >= S->LastPushS + Bank->TrackSampleS)
			{
				S->Track.Push(U.TSimS, U.Enu);
				S->LastPushS = U.TSimS;
			}
		}
		else
		{
			if (bHaveListener && (U.Enu - ListenerNow.Enu).Length() > Bank->MaxRangeM)
			{
				return;  // out of hearing: tracked once it comes nearer (with its past made up)
			}
			TUniquePtr<FSource> New = MakeUnique<FSource>();
			S = New.Get();
			S->Key = U.Key;
			if (U.bSeedHistory && U.VelocityMps.LengthSquared() > 1.0)
			{
				// It was flying before it was first seen: its sound from then is on its way already.
				const double Back = 118.0;
				S->Track.Push(U.TSimS - Back, U.Enu - U.VelocityMps * Back);
				S->TrackStartS = U.TSimS - Back;
			}
			else
			{
				S->TrackStartS = U.TSimS;
			}
			S->Track.Push(U.TSimS, U.Enu);
			S->LastPushS = U.TSimS;
			Sources.Add(U.Key, MoveTemp(New));
		}
		S->Profile = U.Profile;
		S->MaxLevelDb1m = SimFxAudioSilentDb;
		for (const FLayerDef& L : U.Profile->Layers)
		{
			S->MaxLevelDb1m = FMath::Max(S->MaxLevelDb1m, L.LevelDb1m);
		}
		S->NewestEnu = U.Enu;
		S->NewestAltM = U.AltM;
		S->AglM = U.AglM;
		S->LengthM = U.LengthM > 0.0 ? U.LengthM : Bank->BoomDefaultLengthM;
		S->LastFrame = Frame;
	}

	void FScene::SetEmission(uint64 Key, double OnS, double SustainS, double OffS)
	{
		if (TUniquePtr<FSource>* Found = Sources.Find(Key))
		{
			(*Found)->OnS = OnS;
			(*Found)->SustainS = SustainS;
			(*Found)->OffS = (*Found)->bEnded ? FMath::Min((*Found)->OffS, OffS) : OffS;
		}
	}

	void FScene::EndSource(uint64 Key, double AtS)
	{
		if (TUniquePtr<FSource>* Found = Sources.Find(Key))
		{
			FSource& S = **Found;
			if (!S.bEnded)
			{
				S.bEnded = true;
				S.OffS = FMath::Min(S.OffS, AtS);
			}
		}
	}

	uint64 FScene::AddOneShot(const FOneShotDesc& Desc)
	{
		if (Bank == nullptr)
		{
			return 0;
		}
		FOneShot O;
		O.D = Desc;
		O.LengthS = Desc.LoopForS > 0.0 ? Desc.LoopForS
			: FMath::Max(SampleSecondsOf(Desc.Near), SampleSecondsOf(Desc.Far)) - Desc.StartOffsetS;
		if (HasSample(Desc.Tail))
		{
			O.LengthS = FMath::Max(O.LengthS, Desc.TailDelayS + SampleSecondsOf(Desc.Tail) - Desc.StartOffsetS);
		}
		if (O.LengthS <= 0.0 || (!HasSample(Desc.Near) && !HasSample(Desc.Far)))
		{
			return 0;  // nothing to play (a missing asset)
		}
		constexpr int32 MaxOneShots = 1024;
		if (OneShots.Num() >= MaxOneShots)
		{
			// The oldest still on its way gives way.
			const int32 Oldest = OneShots.IndexOfByPredicate([](const FOneShot& X) { return !X.bStarted; });
			OneShots.RemoveAt(Oldest != INDEX_NONE ? Oldest : 0);
		}
		O.Key = (2ull << 56) | NextOneShotKey++;
		OneShots.Add(O);
		return O.Key;
	}

	double FScene::GroundAttenuationDb(double MeanHeightM, double DistanceM)
	{
		const double D = FMath::Max(DistanceM, 1.0);
		return FMath::Max(0.0, 4.8 - (2.0 * FMath::Max(MeanHeightM, 0.0) / D) * (17.0 + 300.0 / D));
	}

	double FScene::ListenerFactor(const Vec3& FromEmit) const
	{
		const Vec3 ToListener = ListenerNow.Enu - FromEmit;
		const double D = ToListener.Length();
		if (D < 1.0e-3)
		{
			return 1.0;
		}
		const double C = Air.SoundSpeedAt(ListenerNow.AltM);
		return FMath::Max(1.0 - simcore::Dot(ListenerVel, ToListener / D) / C, 0.1);
	}

	void FScene::Candidate(uint64 Owner, int32 Layer, FName Sample, bool bLoop, bool bOneShot, double LevelDb, double RankDb,
		const Vec3& Emit, double Pitch, double CutoffHz, double StartOffsetS, double FadeInS)
	{
		FCandidate C;
		C.Owner = Owner;
		C.Layer = Layer;
		C.Sample = Sample;
		C.bLoop = bLoop;
		C.bOneShot = bOneShot;
		C.LevelDb = LevelDb;
		C.RankDb = RankDb;
		C.EmitEnu = Emit;
		C.Pitch = FMath::Clamp(Pitch, 0.5, 2.0);
		C.LowPassHz = CutoffHz;
		C.StartOffsetS = StartOffsetS;
		C.FadeInS = FadeInS;
		if (ListenerNow.bEnclosed)
		{
			// Shut in the cabin: what is outside comes through the hull dull and quiet, what stands beside it less so.
			C.LowPassHz = FMath::Min(C.LowPassHz, Bank->CabinCutoffHz);
			const bool bNear = (Emit - ListenerNow.VehicleEnu).Length() < Bank->CabinNearM;
			C.EnclosureGain = FMath::Pow(10.0, -(bNear ? Bank->CabinNearLossDb : Bank->CabinLossDb) / 20.0);
		}
		Candidates.Add(C);
	}

	double FScene::CutoffFor(FName Sample, double DistanceM) const
	{
		return Air.CutoffHz(FMath::Max(DistanceM - Bank->RecordedAt(Sample), 1.0));
	}

	void FScene::HearSource(FSource& S, double DtSimS)
	{
		S.bHeard = false;
		const Vec3& L = ListenerNow.Enu;
		const double D0 = (S.NewestEnu - L).Length();
		const double C = Air.MeanSoundSpeed(S.NewestAltM);
		const auto& Samples = S.Track.Samples();
		bool bFast = false;
		if (Samples.size() >= 2)
		{
			const auto& A = Samples[Samples.size() - 2];
			const auto& B = Samples.back();
			bFast = (B.position - A.position).Length() > 0.9 * C * FMath::Max(B.t_s - A.t_s, 1.0e-6);
		}
		// Too far or too quiet to matter: not heard this frame (a supersonic one always is, for its boom).
		const double Estimate = S.MaxLevelDb1m + 6.0 - simcore::fx::SpreadingLossDb(D0, 1.0);
		if (!bFast && (D0 > Bank->MaxRangeM || Estimate < Window.TopDb() - Bank->DepthDb - 6.0))
		{
			S.bWatchPrimed = false;
			return;
		}
		const simcore::fx::Reception R = simcore::fx::Hear(S.Track, L, NowS, C);
		if (!S.bWatchPrimed || bListenerJumped)
		{
			S.Watch.Update(R, DtSimS, S.TrackStartS);
			S.bWatchPrimed = true;
		}
		else if (S.Watch.Update(R, DtSimS, S.TrackStartS))
		{
			// The cone has reached the listener.
			const double EmitAlt = S.NewestAltM + (R.emit_position.z - S.NewestEnu.z);
			const double CAt = Air.SoundSpeedAt(EmitAlt);
			Vec3 Vel = S.Track.VelocityAt(R.emit_time_s);
			double Mach = Vel.Length() / FMath::Max(CAt, 1.0);
			if (Mach <= 1.0)
			{
				Mach = R.source_mach;
			}
			const bool bOnGround = ListenerNow.AglM < Bank->GroundListenerAglM;
			const bool bReaches = !bOnGround ||
				simcore::fx::BoomReachesGround(Vel.HorizontalLength(), Air.SoundSpeedAt(ListenerNow.AltM));
			if (Mach > 1.0 && bReaches)
			{
				const double H = FMath::Max(EmitAlt - ListenerNow.AltM, 30.0);
				const simcore::fx::BoomSignature Sig = simcore::fx::CarlsonBoom(Mach, H, Air.PressureAt(EmitAlt),
					Air.PressureAt(ListenerNow.AltM), CAt, S.LengthM, Bank->BoomShapeFactor);
				if (Sig.overpressure_pa > 0.0)
				{
					++BoomCount;
					FOneShotDesc Boom;
					Boom.TEventS = NowS;
					Boom.Enu = R.emit_position;
					Boom.AltM = EmitAlt;
					Boom.AglM = 1000.0;
					Boom.Near = S.LengthM >= Bank->BoomLongFromLengthM ? Bank->BoomLong : Bank->BoomShort;
					Boom.bArrivedNow = true;
					Boom.FixedLevelDb = simcore::fx::PressureToDb(Sig.overpressure_pa) + Bank->BoomLevelOffsetDb;
					AddOneShot(Boom);
				}
			}
		}
		if (!R.heard)
		{
			return;  // nothing it has sent has reached the listener yet (ahead of its cone, or just appeared far off)
		}
		const double Te = R.emit_time_s;
		if (Te < S.OnS || Te > S.OffS)
		{
			return;  // it was silent when the sound now arriving left it
		}
		S.bHeard = true;
		const Vec3 VelE = S.Track.VelocityAt(Te);
		const double EmitAlt = S.NewestAltM + (R.emit_position.z - S.NewestEnu.z);
		const double Mach = VelE.Length() / FMath::Max(Air.SoundSpeedAt(EmitAlt), 1.0);
		const Vec3 ToL = L - R.emit_position;
		const double Dist = FMath::Max(R.distance_m, 1.0);
		const double Angle = VelE.LengthSquared() > 1.0 && ToL.LengthSquared() > 1.0e-6 ? simcore::AngleBetween(VelE, ToL) : 0.0;
		const double Spread = simcore::fx::SpreadingLossDb(Dist, 1.0);
		const double Ground = GroundAttenuationDb(0.5 * (S.AglM + ListenerNow.AglM), Dist);
		const double Sustain = Te >= S.SustainS ? S.Profile->SustainOffsetDb : 0.0;
		const double Pitch = R.doppler * ListenerFactor(R.emit_position);
		double GNear = 1.0, GFar = 0.0;
		BandGains(Dist, S.Profile->NearM >= 0.0 ? S.Profile->NearM : Bank->NearM, S.Profile->FarM >= 0.0 ? S.Profile->FarM : Bank->FarM,
			GNear, GFar);
		for (int32 I = 0; I < S.Profile->Layers.Num() && I < 8; ++I)
		{
			const FLayerDef& Layer = S.Profile->Layers[I];
			if (!HasSample(Layer.Sample) || (Layer.When == EWhen::Supersonic && Mach <= 1.0))
			{
				continue;
			}
			// A near or a far layer: its half of the crossfade (a recording that carries its own distance plays far).
			const double BandGain = Layer.Band == EBand::Near ? GNear : Layer.Band == EBand::Far ? GFar : 1.0;
			if (BandGain < 0.05)
			{
				continue;
			}
			double Dir = 0.0;
			if (Layer.Directivity == EDirectivity::Roar)
			{
				Dir = simcore::fx::JetRoarDirectivityDb(Angle);
			}
			else if (Layer.Directivity == EDirectivity::Whine)
			{
				Dir = simcore::fx::JetWhineDirectivityDb(Angle);
			}
			const double Level = Layer.LevelDb1m + Dir + Sustain - Spread - Ground + 20.0 * std::log10(BandGain);
			// Each loop starts somewhere of its own, so two aircraft of a kind do not phase.
			const uint32 Hash = static_cast<uint32>((S.Key * 2654435761ull) ^ (static_cast<uint64>(I) * 40503ull));
			const double Offset = (Hash % 10007u) / 10007.0 * SampleSecondsOf(Layer.Sample);
			Candidate(S.Key, I, Layer.Sample, true, false, Level, Level, R.emit_position, Layer.Pitch * Pitch,
				CutoffFor(Layer.Sample, Dist), Offset, Bank->LoopFadeS);
		}
	}

	void FScene::HearOneShot(FOneShot& O)
	{
		const Vec3& L = ListenerNow.Enu;
		const double D = FMath::Max((O.D.Enu - L).Length(), 1.0);
		if (O.bStarted)
		{
			O.AgeS += RealDtS;
		}
		else if (O.D.bArrivedNow)
		{
			O.bStarted = true;
			O.StartedAtS = NowS;
			O.AgeS = 0.0;
		}
		else
		{
			const double C = Air.MeanSoundSpeed(O.D.AltM);
			const double Travelled = C * (NowS - O.D.TEventS);
			if (Travelled < D)
			{
				if (NowS - O.D.TEventS > Bank->OneShotRangeM / 280.0 + 5.0)
				{
					O.LengthS = 0.0;  // it will never be heard: it goes
				}
				return;
			}
			const double Late = (Travelled - D) / C;
			if ((O.D.MaxRangeM > 0.0 && D > O.D.MaxRangeM) || D > Bank->OneShotRangeM || Late >= O.LengthS)
			{
				O.LengthS = 0.0;  // out of its reach, or it passed while nobody was listening
				return;
			}
			O.bStarted = true;
			O.StartedAtS = NowS - Late;
			O.AgeS = Late;
		}
		if (O.AgeS >= O.LengthS)
		{
			return;
		}
		double Level = 0.0;
		if (O.D.bArrivedNow)
		{
			Level = O.D.FixedLevelDb;
		}
		else
		{
			if (O.D.ChargeKg > 0.0)
			{
				Level = simcore::fx::PeakSplDb(O.D.ChargeKg, D, Air.ListenerPressurePa(),
					O.D.bGroundBurst ? simcore::fx::kSurfaceBurstFactor : 1.0) + Bank->PeakToLevelDb;
			}
			else
			{
				Level = O.D.LevelDb1m - simcore::fx::SpreadingLossDb(D, 1.0);
			}
			Level -= GroundAttenuationDb(0.5 * (O.D.AglM + ListenerNow.AglM), D);
		}
		const bool bLoop = O.D.LoopForS > 0.0;
		const double Rank = Level - (bLoop ? 0.0 : FMath::Max(0.0, O.AgeS - Bank->OneShotHoldS) * Bank->OneShotDecayDbPerS);
		const double Pitch = O.D.Pitch * ListenerFactor(O.D.Enu);
		const double FadeIn = O.AgeS > 0.03 ? Bank->OneShotFadeS : 0.0;
		const bool bNear = HasSample(O.D.Near), bFar = HasSample(O.D.Far);
		// Arma's distance sets: the near sample under NearM, the far one over FarM, an equal-power crossfade between.
		double WNear = 1.0, WFar = 0.0;
		BandGains(D, Bank->NearM, Bank->FarM, WNear, WFar);
		double GNear = bNear ? 1.0 : 0.0, GFar = bFar && !bNear ? 1.0 : 0.0;
		if (bNear && bFar)
		{
			GNear = WNear;
			GFar = WFar;
		}
		const FName Names[2] = {O.D.Near, O.D.Far};
		const double Gains[2] = {GNear, GFar};
		for (int32 K = 0; K < 2; ++K)
		{
			if (Gains[K] < 0.05)
			{
				continue;
			}
			const double Db = 20.0 * std::log10(Gains[K]);
			double Offset = O.D.StartOffsetS + O.AgeS;
			if (bLoop)
			{
				const double Len = SampleSecondsOf(Names[K]);
				Offset = Len > 0.0 ? std::fmod(Offset, Len) : 0.0;
			}
			else if (Offset >= SampleSecondsOf(Names[K]))
			{
				continue;  // played out (its tail goes on)
			}
			Candidate(O.Key, K, Names[K], bLoop, true, Level + Db, Rank + Db, O.D.Enu, Pitch, CutoffFor(Names[K], D), Offset, FadeIn);
		}
		// The recorded roll under a far blast (the country round about answering), from TailDelayS on, the far half only.
		const double TailAge = O.AgeS - O.D.TailDelayS;
		if (!bLoop && HasSample(O.D.Tail) && WFar >= 0.05 && TailAge >= 0.0 && TailAge < SampleSecondsOf(O.D.Tail))
		{
			const double Db = 20.0 * std::log10(WFar) + O.D.TailDb;
			const double TailRank = Level + Db - FMath::Max(0.0, TailAge - Bank->OneShotHoldS) * Bank->OneShotDecayDbPerS;
			Candidate(O.Key, 2, O.D.Tail, false, true, Level + Db, TailRank, O.D.Enu, Pitch, CutoffFor(O.D.Tail, D),
				TailAge, TailAge > 0.03 ? Bank->OneShotFadeS : 0.0);
		}
	}

	void FScene::Mix(int32 MaxVoices, double MasterVolume, bool bSounding)
	{
		Candidates.Reset();
		if (Bank == nullptr || !bHaveListener)
		{
			AssignVoices(MaxVoices, MasterVolume, false);
			return;
		}
		const double DtSim = FMath::Max(NowS - PrevNowS, 0.0);
		int32 Heard = 0;
		for (auto It = Sources.CreateIterator(); It; ++It)
		{
			FSource& S = *It->Value;
			if (S.LastFrame != Frame && !S.bEnded)
			{
				// Not seen this frame: it has gone (its last place stays where its sound came from).
				S.bEnded = true;
				S.OffS = FMath::Min(S.OffS, S.Track.Empty() ? NowS : S.Track.Newest().t_s);
			}
			const double D0 = (S.NewestEnu - ListenerNow.Enu).Length();
			if ((S.bEnded && NowS > S.OffS + D0 / 280.0 + 2.0) || D0 > Bank->MaxRangeM * 1.5)
			{
				It.RemoveCurrent();
				continue;
			}
			HearSource(S, DtSim);
			Heard += S.bHeard ? 1 : 0;
		}
		for (int32 I = 0; I < OneShots.Num(); ++I)
		{
			HearOneShot(OneShots[I]);
		}
		OneShots.RemoveAll([](const FOneShot& O) { return O.LengthS <= 0.0 || (O.bStarted && O.AgeS >= O.LengthS); });

		double Loudest = SimFxAudioSilentDb;
		for (const FCandidate& C : Candidates)
		{
			Loudest = FMath::Max(Loudest, C.RankDb);
		}
		if (bSounding)
		{
			Window.Update(Loudest, RealDtS);
		}
		AssignVoices(MaxVoices, MasterVolume, bSounding);

		Stats.Sources = Sources.Num();
		Stats.Heard = Heard;
		Stats.Audible = 0;
		for (const FCandidate& C : Candidates)
		{
			Stats.Audible += Window.Audible(C.LevelDb) ? 1 : 0;
		}
		Stats.Voices = NumActiveVoices();
		Stats.Pending = 0;
		for (const FOneShot& O : OneShots)
		{
			Stats.Pending += O.bStarted ? 0 : 1;
		}
		Stats.Booms = BoomCount;
		Stats.WindowTopDb = Window.TopDb();
	}

	void FScene::AssignVoices(int32 MaxVoices, double MasterVolume, bool bSounding)
	{
		MaxVoices = FMath::Clamp(MaxVoices, 0, 256);
		if (Voices.Num() != MaxVoices)
		{
			Voices.SetNum(MaxVoices);
		}
		// Who holds a voice now.
		TMap<uint64, int32>& Held = HeldScratch;
		Held.Reset();
		for (int32 V = 0; V < Voices.Num(); ++V)
		{
			if (Voices[V].bActive && !Voices[V].bReleasing)
			{
				Held.Add(CandidateId(Voices[V].Owner, Voices[V].Layer), V);
			}
		}
		// The loudest audible candidates win; a playing loop keeps its voice unless beaten by the hysteresis.
		TArray<int32>& Order = OrderScratch;
		TArray<double>& Score = ScoreScratch;
		Order.Reset();
		Order.Reserve(Candidates.Num());
		Score.SetNumUninitialized(Candidates.Num(), EAllowShrinking::No);
		if (bSounding)
		{
			for (int32 I = 0; I < Candidates.Num(); ++I)
			{
				const FCandidate& C = Candidates[I];
				if (!Window.Audible(C.LevelDb) || Window.Gain(C.LevelDb) <= 0.0)
				{
					continue;
				}
				Score[I] = C.RankDb + (C.bLoop && Held.Contains(CandidateId(C.Owner, C.Layer)) ? Bank->HysteresisDb : 0.0);
				Order.Add(I);
			}
			Order.Sort([&Score](int32 A, int32 B) { return Score[A] > Score[B]; });
			if (Order.Num() > MaxVoices)
			{
				Order.SetNum(MaxVoices);
			}
		}
		TSet<uint64>& Winners = WinnersScratch;
		Winners.Reset();
		for (const int32 I : Order)
		{
			Winners.Add(CandidateId(Candidates[I].Owner, Candidates[I].Layer));
		}
		for (FVoice& V : Voices)
		{
			if (V.bActive && !V.bReleasing && !Winners.Contains(CandidateId(V.Owner, V.Layer)))
			{
				V.bReleasing = true;  // fades out, then frees
			}
		}
		const auto Apply = [this](FVoice& V, const FCandidate& C)
		{
			const Vec3 Rel = C.EmitEnu - ListenerNow.Enu;
			const double D = Rel.Length();
			V.EmitEnu = C.EmitEnu;
			V.Enu = D > 1.0e-3 ? ListenerNow.Enu + Rel * (FMath::Min(D, Bank->VoiceDistanceM) / D) : C.EmitEnu;
			V.Pitch = C.Pitch;
			V.LowPassHz = C.LowPassHz;
			V.LevelDb = C.LevelDb;
			V.Gain = Window.Gain(C.LevelDb) * C.EnclosureGain;
		};
		for (const int32 I : Order)
		{
			const FCandidate& C = Candidates[I];
			if (const int32* Has = Held.Find(CandidateId(C.Owner, C.Layer)))
			{
				Apply(Voices[*Has], C);
				continue;
			}
			int32 Free = Voices.IndexOfByPredicate([](const FVoice& V) { return !V.bActive; });
			if (Free == INDEX_NONE && C.bOneShot)
			{
				// A one-shot cannot wait for a fade: it takes the voice closest to silence.
				double Least = 2.0;
				for (int32 V = 0; V < Voices.Num(); ++V)
				{
					if (Voices[V].bReleasing && Voices[V].Fade < Least)
					{
						Least = Voices[V].Fade;
						Free = V;
					}
				}
			}
			if (Free == INDEX_NONE)
			{
				continue;  // a loop waits for a voice to finish fading
			}
			FVoice& V = Voices[Free];
			V.bActive = true;
			V.bReleasing = false;
			V.Owner = C.Owner;
			V.Layer = C.Layer;
			V.Sample = C.Sample;
			V.bLoop = C.bLoop;
			++V.Generation;
			V.StartOffsetS = C.StartOffsetS;
			V.FadeInS = C.FadeInS;
			V.Fade = C.FadeInS > 0.0 ? 0.0 : 1.0;
			Apply(V, C);
		}
		for (FVoice& V : Voices)
		{
			if (!V.bActive)
			{
				V.Volume = 0.0;
				continue;
			}
			if (V.bReleasing)
			{
				const double Out = FMath::Max(V.bLoop ? Bank->LoopFadeS : Bank->OneShotFadeS, 1.0e-3);
				V.Fade -= RealDtS / Out;
				if (V.Fade <= 0.0 || (!bSounding && RealDtS <= 0.0))
				{
					V.Fade = 0.0;
					V.bActive = false;
					V.bReleasing = false;
					V.Volume = 0.0;
					continue;
				}
			}
			else if (V.Fade < 1.0)
			{
				V.Fade = V.FadeInS > 0.0 ? FMath::Min(1.0, V.Fade + RealDtS / V.FadeInS) : 1.0;
			}
			V.Volume = V.Gain * MasterVolume * V.Fade;
		}
	}

	int32 FScene::NumActiveVoices() const
	{
		int32 N = 0;
		for (const FVoice& V : Voices)
		{
			N += V.bActive ? 1 : 0;
		}
		return N;
	}

	const FVoice* FScene::VoiceFor(uint64 Owner) const
	{
		for (const FVoice& V : Voices)
		{
			if (V.bActive && !V.bReleasing && V.Owner == Owner)
			{
				return &V;
			}
		}
		return nullptr;
	}

	const FVoice* FScene::VoiceFor(uint64 Owner, FName Sample) const
	{
		for (const FVoice& V : Voices)
		{
			if (V.bActive && !V.bReleasing && V.Owner == Owner && V.Sample == Sample)
			{
				return &V;
			}
		}
		return nullptr;
	}

	bool FScene::IsHeard(uint64 SourceKey) const
	{
		const TUniquePtr<FSource>* S = Sources.Find(SourceKey);
		return S != nullptr && (*S)->bHeard;
	}

	double FScene::LevelAtEar(uint64 Owner) const
	{
		double Best = SimFxAudioSilentDb;
		for (const FCandidate& C : Candidates)
		{
			if (C.Owner == Owner)
			{
				Best = FMath::Max(Best, C.LevelDb);
			}
		}
		return Best;
	}

	bool FScene::IsAudible(uint64 Owner) const
	{
		for (const FCandidate& C : Candidates)
		{
			if (C.Owner == Owner && Window.Audible(C.LevelDb))
			{
				return true;
			}
		}
		return false;
	}

	double FScene::OneShotStartedAt(uint64 Key) const
	{
		const FOneShot* O = OneShots.FindByPredicate([Key](const FOneShot& X) { return X.Key == Key; });
		return O != nullptr && O->bStarted ? O->StartedAtS : -1.0;
	}

	bool FScene::IsOneShotPending(uint64 Key) const
	{
		const FOneShot* O = OneShots.FindByPredicate([Key](const FOneShot& X) { return X.Key == Key; });
		return O != nullptr && !O->bStarted;
	}
}
