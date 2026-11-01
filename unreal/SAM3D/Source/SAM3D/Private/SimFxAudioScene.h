#pragma once

// The weapons' and aircraft's sound as plain C++ (docs/fx/README.md section 4.2): what is heard where and when, how
// loud, how high and how dull, and which few sounds get the voices. No world, no audio component: USimFxAudioSubsystem
// feeds it the sources and the listener each frame and plays its voices; the automation tests drive it directly.
//
// Every source keeps a simcore::fx::SourceTrack of where it has been; each frame simcore::fx::Hear finds the sound now
// reaching the listener (it left the source earlier, from somewhere else), so a jet is heard behind where it is seen and
// a supersonic one not at all until its cone arrives (simcore::fx::BoomWatch then plays the boom). A one-shot starts
// when its wavefront reaches the listener. The levels at the ear go through a loudness window (simcore::fx::
// LoudnessWindow, an HDR mix): the loudest sound sets the top, the quiet ones under it are not played, and the loudest
// few get the voices.

#include "CoreMinimal.h"
#include "SimCoreIncludes.h"

#include <string>

namespace SimFxAudio
{
	using simcore::Vec3;

	enum class EDirectivity : uint8 { Omni, Roar, Whine };
	enum class EWhen : uint8 { Always, Supersonic };
	/** At which distances a layer plays: always, or as the near or the far half of a crossfade (Arma's distance sets). */
	enum class EBand : uint8 { All, Near, Far };

	/** One layer of an engine's sound: a looping sample, its level 1 m away (dB), how it varies round the source. */
	struct FLayerDef
	{
		FName Sample;
		double LevelDb1m = 140.0;
		EDirectivity Directivity = EDirectivity::Omni;
		double Pitch = 1.0;
		EWhen When = EWhen::Always;
		EBand Band = EBand::All;
	};

	/** How a kind of source sounds (sounds.json "profiles"). */
	struct FProfileDef
	{
		FString Id;
		TArray<FLayerDef> Layers;
		double SustainOffsetDb = -6.0;  // a rocket motor's sustain phase against its boost
		double NearM = -1.0;            // its near and far layers crossfade between these (< 0: the bank's)
		double FarM = -1.0;
	};

	/** The gains of a near and a far layer at DistanceM: an equal-power crossfade from NearM to FarM (cos, sin). */
	void BandGains(double DistanceM, double NearM, double FarM, double& OutNear, double& OutFar);

	/** A one-shot event's sample and level 1 m away. */
	struct FEventSoundDef
	{
		FName Sample;
		double LevelDb1m = 140.0;
		double Pitch = 1.0;
	};

	/** An explosion's samples by its charge (TNT kg up to MaxKg), on the ground or in the air, near and far. */
	struct FBlastClassDef
	{
		double MaxKg = 1.0e9;
		FName Near, Far, AirNear, AirFar;
		double RefKg = 100.0;
		FName Tail;               // a recorded roll under the far sample (the far half of the crossfade), TailDelayS in
		double TailDelayS = 1.0;
		double TailDb = -8.0;     // against the blast's level
	};

	/** A gun's sound: a loop at its rate (a burst), or single reports at its rate, and a tail after a burst. Its rate and
	 *  muzzle speed are the munition's (data/scenery/munitions.json) where it has them; its bursts are the picture's. */
	struct FGunDef
	{
		FName Loop, Single, Tail;
		double LevelDb1m = 155.0;
		double Pitch = 1.0;
		double RateHz = 30.0;
		double MuzzleMps = 970.0;
	};

	/** data/fx/sounds.json. */
	struct FBank
	{
		// The mix.
		double FloorDb = 105.0, DepthDb = 60.0, Slope = 0.7, AttackDbPerS = 400.0, ReleaseDbPerS = 12.0;
		double HysteresisDb = 3.0, LoopFadeS = 0.075, OneShotFadeS = 0.05;
		double OneShotHoldS = 0.25, OneShotDecayDbPerS = 20.0, PeakToLevelDb = -10.0;
		// Propagation.
		double NearM = 600.0, FarM = 2500.0, MaxRangeM = 40000.0, OneShotRangeM = 60000.0, AbsorptionBudgetDb = 6.0;
		double VoiceDistanceM = 500.0, ListenerMaxMps = 400.0, ListenerJumpM = 2000.0, ListenerSmoothS = 0.25;
		double TrackSampleS = 0.1, GroundListenerAglM = 100.0;
		// The closed cabin.
		double CabinCutoffHz = 700.0, CabinLossDb = 14.0, CabinNearM = 30.0, CabinNearLossDb = 6.0;
		// Sources.
		TMap<FString, FProfileDef> Profiles;
		TMap<FString, FString> ModelProfiles;
		TMap<FString, FString> ArchetypeProfiles;
		double HeavyJetLengthM = 35.0;
		FString HeavyProfile = TEXT("heavy_jet");
		FString MissileProfile = TEXT("rocket_small");
		FString LargeRocketProfile = TEXT("rocket_large");
		double LargeRocketFromLengthM = 7.0;
		FString FireProfile = TEXT("fire");
		double MissileBoostS = 3.0, MissileSustainS = 12.0;
		double FlyerMps = 600.0;  // a site's missile with no flight time: its mean speed to its aim
		// Recordings (sounds.json "recordings"): how far off each was recorded, which the air has already dulled.
		TMap<FName, double> RecordedAtM;
		double RecordedAt(FName Sample) const { const double* M = RecordedAtM.Find(Sample); return M != nullptr ? *M : 0.0; }
		// Events.
		FEventSoundDef Launch, ArmLaunch, RocketLaunch, BombRush, Debris, Cluster, Crack, Fireball;
		// A gun pass's rhythm (effects.json "guns", set by the subsystem from FSimFxTuning: the picture's bursts).
		double GunBurstMinS = 0.2, GunBurstMaxS = 0.35, GunPauseMinS = 2.0, GunPauseMaxS = 3.0;
		TArray<TPair<FString, int32>> GunBurstMinRounds;  // effects.json guns.burst_min_rounds: the picture's
		double FlakJitterS = 0.4, FlakOffsetM = 35.0;
		int32 SdPopsPerBurst = 3;   // a burst's rounds heard destroying themselves (a few of them)
		double FlakDefaultKg = 0.2;
		double RushLeadS = 2.0, RushRangeM = 1000.0, DebrisRangeM = 600.0;
		double RippleIntervalS = 0.08;
		int32 RippleMax = 19;
		double StickIntervalS = 0.12;
		int32 StickMax = 8;
		double StickSpreadM = 15.0;
		double KillBlastKg = 40.0, WarheadDefaultKg = 15.0, SamDefaultKg = 30.0, ArmDefaultKg = 25.0, CruiseDefaultKg = 450.0;
		double ChargeShareOfMass = 0.42, GunImpactKg = 0.05;
		int32 GunImpactBursts = 6;
		double AirBurstAglM = 10.0, CrackRangeM = 40.0, WreckGroundFireS = 60.0;
		TArray<FBlastClassDef> Blasts;
		double BlastPitchExponent = 0.1;
		TMap<FString, FGunDef> Guns;
		// Booms.
		FName BoomShort, BoomLong;
		double BoomLongFromLengthM = 30.0, BoomShapeFactor = 0.1, BoomDefaultLengthM = 15.0, BoomLevelOffsetDb = 0.0;

		bool bLoaded = false;

		/** Read sounds.json's text. False, with the reason, if it is not the file expected. */
		bool LoadJson(const std::string& Text, FString& OutError);

		/** The profile of a model (its own entry, else its archetype's; a long jet a heavy jet); null if none. */
		const FProfileDef* ProfileFor(const FString& Model, const FString& Archetype, double LengthM) const;
		const FProfileDef* Profile(const FString& Id) const { return Profiles.Find(Id); }

		/** The blast class of a charge (the last for the biggest). */
		const FBlastClassDef* BlastFor(double ChargeKg) const;

		/** A gun's sound by a system or weapon id ("default" if none of its own). */
		const FGunDef* GunFor(const FString& Id) const;

		/** Every sample any entry names. */
		void AllSamples(TSet<FName>& Out) const;
	};

	/** Where the listener is (the player's camera) and whether he is shut in the cabin. */
	struct FListener
	{
		Vec3 Enu;
		double AltM = 0.0;
		double AglM = 1.7;
		bool bEnclosed = false;  // in the closed cabin: what is outside is muffled
		Vec3 VehicleEnu;         // the cabin's vehicle
	};

	/** The air between the listener and the sources, tabulated once a while: the mean speed of sound from the listener's
	 *  height to a source's, the local speed of sound and pressure, the absorption's low-pass by distance. */
	class FAir
	{
	public:
		void Build(TFunctionRef<simcore::AtmosphereState(double)> Air, double ListenerAltM, double RhPct, double BudgetDb);
		bool IsBuilt() const { return bBuilt; }
		double BuiltForAltM() const { return ListenerAlt; }

		double MeanSoundSpeed(double SourceAltM) const { return Lookup(MeanC, SourceAltM); }
		double SoundSpeedAt(double AltM) const { return Lookup(LocalC, AltM); }
		double PressureAt(double AltM) const { return Lookup(Pressure, AltM); }
		double ListenerPressurePa() const { return ListenerP; }
		double CutoffHz(double DistanceM) const;

	private:
		static constexpr int32 NumAlt = 128;
		static constexpr double AltMin = -500.0;
		static constexpr double AltStep = 250.0;
		static constexpr int32 NumDist = 64;
		double Lookup(const double* Table, double AltM) const;

		double MeanC[NumAlt] = {};
		double LocalC[NumAlt] = {};
		double Pressure[NumAlt] = {};
		double Cutoff[NumDist] = {};
		double ListenerAlt = 0.0;
		double ListenerP = 101325.0;
		bool bBuilt = false;
	};

	/** A continuous source's state this frame, at its own simulation time. */
	struct FSourceUpdate
	{
		uint64 Key = 0;
		const FProfileDef* Profile = nullptr;
		double TSimS = 0.0;
		Vec3 Enu;
		Vec3 VelocityMps;         // to make up its past when it first appears (bSeedHistory)
		double AltM = 0.0;
		double AglM = 1000.0;
		double LengthM = 15.0;    // for its boom
		bool bSeedHistory = false;  // it was flying before it was first seen: its past is a straight line back
	};

	/** A sound that happens once (an explosion, a launch, a burst of fire, a boom). */
	struct FOneShotDesc
	{
		double TEventS = 0.0;     // simulation time it happens at its place
		Vec3 Enu;
		double AltM = 0.0;
		double AglM = 0.0;
		FName Near, Far;          // Far none: the near sample at every distance
		double LevelDb1m = 0.0;   // when ChargeKg is 0
		double ChargeKg = 0.0;    // > 0: an explosion, its level from simcore::fx::PeakSplDb
		bool bGroundBurst = false;
		double Pitch = 1.0;
		double LoopForS = 0.0;    // > 0: a loop sample played this long (a gun's burst)
		double StartOffsetS = 0.0;  // into the sample (a riser that must end on its event)
		double MaxRangeM = 0.0;   // > 0: only heard within this of the listener
		bool bArrivedNow = false; // starts at once, at FixedLevelDb at the ear (a boom)
		double FixedLevelDb = 0.0;
		FName Tail;               // a recorded roll TailDelayS after it, TailDb under it, on the far side of the crossfade
		double TailDelayS = 0.0;
		double TailDb = 0.0;
	};

	/** One of the fixed voices: what it plays and how. The subsystem restarts its component when Generation changes. */
	struct FVoice
	{
		bool bActive = false;     // playing, or fading out
		bool bReleasing = false;
		uint64 Owner = 0;
		int32 Layer = -1;
		FName Sample;
		bool bLoop = false;
		uint32 Generation = 0;
		double StartOffsetS = 0.0;
		Vec3 Enu;                 // where to place it (on the line to where the sound left, VoiceDistanceM at most)
		Vec3 EmitEnu;             // where the sound left
		double Pitch = 1.0;
		double Volume = 0.0;
		double LowPassHz = 20000.0;
		double LevelDb = -1000.0; // at the ear
		double Gain = 0.0;        // from the window, before the fade, the cabin and the master volume
		double Fade = 0.0;
		double FadeInS = 0.0;
	};

	struct FStats
	{
		int32 Sources = 0;
		int32 Heard = 0;
		int32 Audible = 0;
		int32 Voices = 0;
		int32 Pending = 0;
		int32 Booms = 0;
		double WindowTopDb = 0.0;
	};

	/** The sound scene. */
	class FScene
	{
	public:
		FScene();
		~FScene();

		void SetBank(const FBank* InBank);
		const FBank* GetBank() const { return Bank; }

		/** A sample's length (s): only samples with a length are played (a missing asset has none). */
		void SetSampleSeconds(FName Sample, double Seconds);
		bool HasSample(FName Sample) const { return !Sample.IsNone() && SampleSeconds.Contains(Sample); }
		double SampleSecondsOf(FName Sample) const;
		/** Every sample of the bank given a length (the tests, with no assets). */
		void SetAllSamplesAvailable(double Seconds);

		/** The air (the weather's, or the standard atmosphere) and the surface humidity. */
		void SetAir(TFunction<simcore::AtmosphereState(double)> InAir, double InRhPct);

		/** Forget every source, sound and voice (a new scenario). Voices go inactive at once. */
		void Reset();

		/** Start a frame: the moment heard (simulation time between steps), the real time since the last frame. */
		void BeginFrame(double NowS, double RealDtS, const FListener& Listener);

		/** A continuous source this frame (an aircraft, a missile, a wreck). Not updated in a frame: it has ended. */
		void UpdateSource(const FSourceUpdate& Update);
		/** When it sounds (simulation time): from OnS, at SustainS its sustain's level, silent after OffS. */
		void SetEmission(uint64 Key, double OnS, double SustainS, double OffS);
		/** It stops sounding at AtS (destroyed); it goes once its last sound has passed the listener. */
		void EndSource(uint64 Key, double AtS);
		bool HasSource(uint64 Key) const;

		/** A one-shot; returns its key. */
		uint64 AddOneShot(const FOneShotDesc& Desc);

		/** Hear everything, set the window and give the voices to the loudest. bSounding false: fade every voice out
		 *  (the time runs too fast to be heard). */
		void Mix(int32 MaxVoices, double MasterVolume, bool bSounding = true);

		const TArray<FVoice>& GetVoices() const { return Voices; }
		const FStats& GetStats() const { return Stats; }
		const FListener& GetListener() const { return ListenerNow; }
		Vec3 GetListenerVelocity() const { return ListenerVel; }

		// For the tests and the stats.
		int32 NumActiveVoices() const;
		const FVoice* VoiceFor(uint64 Owner) const;
		const FVoice* VoiceFor(uint64 Owner, FName Sample) const;  // the voice playing that sample of it
		bool IsHeard(uint64 SourceKey) const;
		double LevelAtEar(uint64 Owner) const;   // the loudest layer heard now (dB), -1000 if none
		bool IsAudible(uint64 Owner) const;      // above the window's floor now
		double OneShotStartedAt(uint64 Key) const;  // when its wavefront arrived (simulation time), -1 if not yet
		bool IsOneShotPending(uint64 Key) const;
		int32 NumOneShots() const;
		double WindowTopDb() const;

		/** ISO 9613-2's simplified ground attenuation (dB, >= 0) over a path of `DistanceM` at mean height `MeanHeightM`. */
		static double GroundAttenuationDb(double MeanHeightM, double DistanceM);

	private:
		struct FSource;
		struct FOneShot;
		struct FCandidate
		{
			uint64 Owner = 0;
			int32 Layer = 0;
			FName Sample;
			bool bLoop = false;
			bool bOneShot = false;
			double LevelDb = 0.0;   // at the ear, for the gain
			double RankDb = 0.0;    // for the window and the voices (a one-shot's fades after its attack)
			Vec3 EmitEnu;
			double Pitch = 1.0;
			double LowPassHz = 20000.0;
			double StartOffsetS = 0.0;
			double FadeInS = 0.0;
			double EnclosureGain = 1.0;
		};

		/** The air's low-pass for a sample heard DistanceM away, less what a recording's own distance has dulled already. */
		double CutoffFor(FName Sample, double DistanceM) const;
		void HearSource(FSource& S, double DtSimS);
		void HearOneShot(FOneShot& O);
		void Candidate(uint64 Owner, int32 Layer, FName Sample, bool bLoop, bool bOneShot, double LevelDb, double RankDb,
			const Vec3& Emit, double Pitch, double CutoffHz, double StartOffsetS, double FadeInS);
		void AssignVoices(int32 MaxVoices, double MasterVolume, bool bSounding);
		double ListenerFactor(const Vec3& FromEmit) const;
		void RebuildAirIfNeeded();
		static uint64 CandidateId(uint64 Owner, int32 Layer) { return Owner * 8ull + static_cast<uint64>(Layer & 7); }

		const FBank* Bank = nullptr;
		TMap<FName, double> SampleSeconds;
		TFunction<simcore::AtmosphereState(double)> AirFn;
		double RhPct = 70.0;
		FAir Air;
		double AirAgeS = 1.0e9;

		TMap<uint64, TUniquePtr<FSource>> Sources;
		TArray<FOneShot> OneShots;
		uint64 NextOneShotKey = 1;
		TArray<FCandidate> Candidates;
		TArray<FVoice> Voices;
		simcore::fx::LoudnessWindow Window;
		// AssignVoices' working sets, kept between frames (no allocation once warm).
		TMap<uint64, int32> HeldScratch;
		TArray<int32> OrderScratch;
		TArray<double> ScoreScratch;
		TSet<uint64> WinnersScratch;

		FListener ListenerNow;
		Vec3 ListenerPrev;
		Vec3 ListenerVel;
		bool bHaveListener = false;
		bool bListenerJumped = false;
		double NowS = 0.0;
		double PrevNowS = 0.0;
		double RealDtS = 0.0;
		uint64 Frame = 0;
		int32 BoomCount = 0;
		FStats Stats;
	};
}
