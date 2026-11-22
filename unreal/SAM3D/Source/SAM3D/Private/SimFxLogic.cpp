#include "SimFxLogic.h"

void FSimFxView::Set(const FVector& InLocation, const FRotator& Rotation, double InFovDeg, double InWidthPx, double HeightOverWidth)
{
	Location = InLocation;
	const FRotationMatrix M(Rotation);
	Forward = M.GetUnitAxis(EAxis::X);
	Right = M.GetUnitAxis(EAxis::Y);
	Up = M.GetUnitAxis(EAxis::Z);
	FovDeg = FMath::Clamp(InFovDeg, 0.01, 179.0);
	TanHalfX = FMath::Tan(FMath::DegreesToRadians(FovDeg * 0.5));
	TanHalfY = TanHalfX * FMath::Max(HeightOverWidth, 0.05);
	WidthPx = FMath::Max(InWidthPx, 1.0);
	bActive = true;
}

double FSimFxView::PixelsPerCm(double DistCm) const
{
	return WidthPx / (2.0 * FMath::Max(DistCm, 1.0) * TanHalfX);
}

bool FSimFxView::Contains(const FVector& P, double RadiusCm, double Margin) const
{
	const FVector D = P - Location;
	const double Z = FVector::DotProduct(D, Forward);
	if (Z < -RadiusCm)
	{
		return false;
	}
	const double Zc = FMath::Max(Z, 0.0);
	// The sphere widens each side's limit by its radius over the cosine of the half angle (about the radius for the
	// narrow lenses).
	const double X = FMath::Abs(FVector::DotProduct(D, Right));
	const double Y = FMath::Abs(FVector::DotProduct(D, Up));
	const double Rx = RadiusCm * FMath::Sqrt(1.0 + TanHalfX * TanHalfX);
	const double Ry = RadiusCm * FMath::Sqrt(1.0 + TanHalfY * TanHalfY);
	return X <= Zc * TanHalfX * Margin + Rx && Y <= Zc * TanHalfY * Margin + Ry;
}

namespace SimFxLogic
{
	double ProjectedPx(double SizeM, double DistM, double FovDeg, double WidthPx)
	{
		const double T = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FovDeg, 0.01, 179.0) * 0.5));
		return SizeM / (2.0 * FMath::Max(DistM, 0.01) * T) * WidthPx;
	}

	ESimFxTier TierForPx(double Px, double DistM, const FSimFxLod& Lod)
	{
		if (DistM > Lod.MaxRangeM || DistM > Lod.VisibilityM)
		{
			return ESimFxTier::None;
		}
		if (Px > Lod.FullPx)
		{
			return ESimFxTier::Full;
		}
		if (Px > Lod.ImpostorPx)
		{
			return ESimFxTier::Impostor;
		}
		if (Lod.bNight)
		{
			return ESimFxTier::Glint;
		}
		return Px > Lod.SpeckPx ? ESimFxTier::Speck : ESimFxTier::None;
	}

	ESimFxTier TierFor(TConstArrayView<FSimFxView> Views, const FVector& At, double SizeM, const FSimFxLod& Lod, double* OutPx)
	{
		ESimFxTier Best = ESimFxTier::None;
		double BestPx = 0.0;
		for (const FSimFxView& V : Views)
		{
			if (!V.bActive || !V.Contains(At, SizeM * 50.0, Lod.Margin))
			{
				continue;
			}
			const double DistM = FVector::Dist(At, V.Location) / 100.0;
			const double Px = ProjectedPx(SizeM, DistM, V.FovDeg, V.WidthPx);
			const ESimFxTier T = TierForPx(Px, DistM, Lod);
			if (static_cast<uint8>(T) > static_cast<uint8>(Best))
			{
				Best = T;
			}
			BestPx = FMath::Max(BestPx, Px);
		}
		if (OutPx != nullptr)
		{
			*OutPx = BestPx;
		}
		return Best;
	}

	bool InAnyView(TConstArrayView<FSimFxView> Views, const FVector& At, double RadiusCm, double Margin)
	{
		for (const FSimFxView& V : Views)
		{
			if (V.bActive && V.Contains(At, RadiusCm, Margin))
			{
				return true;
			}
		}
		return false;
	}

	float GrowShare(float Age, float Life, float GrowS)
	{
		if (GrowS > 0.0f)
		{
			return 1.0f - FMath::Exp(-FMath::Max(Age, 0.0f) / GrowS);
		}
		const float U = FMath::Clamp(Age / FMath::Max(Life, 1e-3f), 0.0f, 1.0f);
		return 1.0f - (1.0f - U) * (1.0f - U);
	}

	float ErosionAt(float U)
	{
		return Smooth01((U - 0.5f) / 0.5f);
	}

	float HeatAt(float Heat0, float Heat1, float U, float Age, float HoldS, float TauS)
	{
		if (TauS <= 0.0f)
		{
			return Heat0 + (Heat1 - Heat0) * FMath::Clamp(U, 0.0f, 1.0f);
		}
		return Age <= HoldS ? Heat0 : Heat0 * FMath::Exp(-(Age - HoldS) / TauS);
	}

	double FireballTauS(double ChargeKg)
	{
		return 0.15 * FMath::Pow(FMath::Max(ChargeKg, 0.01) / 100.0, 1.0 / 3.0);
	}

	float SubPixelAlpha(double TrueWidth, double TrueLength, double DrawnWidth, double DrawnLength)
	{
		const double W = DrawnWidth > 0.0 ? FMath::Clamp(TrueWidth / DrawnWidth, 0.0, 1.0) : 1.0;
		const double L = DrawnLength > 0.0 ? FMath::Clamp(TrueLength / DrawnLength, 0.0, 1.0) : 1.0;
		return static_cast<float>(W * L);
	}

	float GlintAlpha(double TrueWidth, double TrueLength, double DrawnWidth, double DrawnLength, float Keep)
	{
		return FMath::Max(SubPixelAlpha(TrueWidth, TrueLength, DrawnWidth, DrawnLength), FMath::Clamp(Keep, 0.0f, 1.0f));
	}

	float RibbonWidthFloor(float TrueWidthCm, double PixelsPerCm, double MinPx, float& OutAlpha)
	{
		OutAlpha = 1.0f;
		if (PixelsPerCm <= 0.0 || MinPx <= 0.0)
		{
			return TrueWidthCm;
		}
		const double FloorCm = MinPx / PixelsPerCm;
		if (TrueWidthCm >= FloorCm)
		{
			return TrueWidthCm;
		}
		OutAlpha = static_cast<float>(FMath::Max(TrueWidthCm, 0.0f) / FloorCm);
		return static_cast<float>(FloorCm);
	}

	ESimFxLayer DrawLayerOf(ESimFxLayer Layer)
	{
		return Layer == ESimFxLayer::Fire ? ESimFxLayer::Smoke : Layer;
	}

	FLinearColor LightOnSurface(const FLinearColor& Colour, double Lumens, double DistCm, double RadiusCm, double MinDistCm)
	{
		if (Lumens <= 0.0 || DistCm >= RadiusCm)
		{
			return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}
		const double DM = FMath::Max(DistCm, FMath::Max(MinDistCm, 1.0)) / 100.0;
		const double Window = FMath::Square(1.0 - FMath::Pow(FMath::Clamp(DistCm / FMath::Max(RadiusCm, 1.0), 0.0, 1.0), 4.0));
		const double K = Lumens / (4.0 * UE_DOUBLE_PI * DM * DM) / UE_DOUBLE_PI * Window;
		FLinearColor C = Colour * static_cast<float>(K);
		C.A = 0.0f;
		return C;
	}

	double AmbientShare(double ElevationDeg, double Day, double Dusk, double Night)
	{
		const double E = ElevationDeg;
		if (E >= 6.0)
		{
			return Day;
		}
		if (E >= 0.0)
		{
			return FMath::Lerp(Dusk, Day, E / 6.0);
		}
		if (E >= -6.0)
		{
			return Dusk;
		}
		return FMath::Max(Dusk * FMath::Pow(10.0, (E + 6.0) / 2.0), Night);
	}

	double ColumnStabS(double ChargeKg)
	{
		return 60.0 * FMath::Pow(FMath::Max(ChargeKg, 0.01) / 100.0, 0.23);
	}

	double ColumnTopM(double ChargeKg, double AgeS, double Scale)
	{
		const double Stab = ColumnStabS(ChargeKg);
		return ColumnRiseK(ChargeKg, Scale) * FMath::Sqrt(FMath::Clamp(AgeS, 0.0, Stab));
	}

	double ColumnRiseK(double ChargeKg, double Scale)
	{
		return Scale * 92.6 * FMath::Pow(FMath::Max(ChargeKg, 0.01), 0.25) / FMath::Sqrt(ColumnStabS(ChargeKg));
	}

	double ColumnWidthM(double ChargeKg)
	{
		return 2.0 * 4.7 * FMath::Pow(FMath::Max(ChargeKg, 0.01), 0.375);
	}

	double FuelFireballM(double FuelKg)
	{
		return 5.8 * FMath::Pow(FMath::Max(FuelKg, 0.0), 1.0 / 3.0);
	}

	double FuelFireballS(double FuelKg)
	{
		return 0.45 * FMath::Pow(FMath::Max(FuelKg, 0.0), 1.0 / 3.0);
	}

	double SachsScale(double AltitudeM)
	{
		const double H = FMath::Clamp(AltitudeM, 0.0, 11000.0);
		const double PressureShare = FMath::Pow(1.0 - 2.25577e-5 * H, 5.25588);
		return FMath::Pow(1.0 / FMath::Max(PressureShare, 1e-3), 1.0 / 3.0);
	}

	FFlakRow FlakFor(TConstArrayView<FFlakRow> Rows, double CalibreMm)
	{
		if (Rows.Num() == 0)
		{
			return FFlakRow();
		}
		if (CalibreMm <= Rows[0].CalibreMm)
		{
			return Rows[0];
		}
		for (int32 I = 1; I < Rows.Num(); ++I)
		{
			const FFlakRow& A = Rows[I - 1];
			const FFlakRow& B = Rows[I];
			if (CalibreMm <= B.CalibreMm)
			{
				const double U = (CalibreMm - A.CalibreMm) / FMath::Max(B.CalibreMm - A.CalibreMm, 1e-6);
				FFlakRow R;
				R.CalibreMm = CalibreMm;
				R.PuffM = FMath::Lerp(A.PuffM, B.PuffM, U);
				R.HangS = FMath::Lerp(A.HangS, B.HangS, U);
				R.Black = FMath::Lerp(A.Black, B.Black, U);
				return R;
			}
		}
		return Rows.Last();
	}

	float RingAlpha(double ToCamUp)
	{
		return static_cast<float>(FMath::Clamp(FMath::Abs(ToCamUp) / 0.17, 0.0, 1.0));
	}

	float EndOnAlpha(double SinView)
	{
		return static_cast<float>(FMath::SmoothStep(0.03, 0.2, FMath::Abs(SinView)));
	}

	double FrameDt(double DeltaNowS, double TimeScale, double MaxHitchS)
	{
		return FMath::Clamp(DeltaNowS, 0.0, FMath::Max(MaxHitchS, 0.0) * FMath::Max(TimeScale, 1.0));
	}

	int32 SubSteps(double DtS, double SliceS, int32 MaxSlices)
	{
		if (DtS <= 0.0 || SliceS <= 0.0)
		{
			return 1;
		}
		return FMath::Clamp(FMath::CeilToInt32(DtS / SliceS - 1e-9), 1, FMath::Max(MaxSlices, 1));
	}

	int32 ChaffPuffs(int32 Cartridges, int32 PerCartridge, int32 Max)
	{
		return FMath::Clamp(FMath::Max(Cartridges, 1) * FMath::Max(PerCartridge, 1), 1, FMath::Max(Max, 1));
	}

	FVector FlareEjectDir(int32 Index, const FVector& Fwd)
	{
		FVector Track = FVector(Fwd.X, Fwd.Y, 0.0);
		Track = Track.SizeSquared() > 1e-6 ? Track.GetSafeNormal() : FVector::ForwardVector;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Track).GetSafeNormal();
		const double Side = (Index % 2 == 0) ? 1.0 : -1.0;
		return (FVector(0.0, 0.0, -1.0) + Right * (0.6 * Side) - Track * 0.2).GetSafeNormal();
	}

	float FlareGlowAt(double Age, double BurnS)
	{
		if (Age < 0.0 || BurnS <= 0.0 || Age >= BurnS)
		{
			return 0.0f;
		}
		const double Fade = FMath::Max(0.2 * BurnS, 0.3);
		const double Up = FMath::Clamp(Age / 0.1, 0.0, 1.0);
		const double Down = FMath::Clamp((BurnS - Age) / Fade, 0.0, 1.0);
		return static_cast<float>(FMath::Min(Up, Down));
	}

	FRotator TumbleAt(uint64 Key, double Age, double MaxDegPerS)
	{
		// Two keyed draws in 0.35..1 of the fastest rate, each with its own sign.
		const uint32 H1 = static_cast<uint32>(Key * 2654435761ull) ^ 0x9E3779B9u;
		const uint32 H2 = (H1 * 1664525u + 1013904223u) ^ (H1 >> 13);
		const double U1 = (H1 % 10007u) / 10007.0;
		const double U2 = (H2 % 10009u) / 10009.0;
		const double Pitch = MaxDegPerS * (0.35 + 0.65 * U1) * ((H1 & 1u) ? 1.0 : -1.0);
		const double Roll = MaxDegPerS * (0.35 + 0.65 * U2) * ((H2 & 2u) ? 1.0 : -1.0);
		const double T = FMath::Max(Age, 0.0);
		return FRotator(Pitch * T, 15.0 * FMath::Sin(1.3 * T + 6.0 * U1), Roll * T);
	}

	double TrailRangeM(const FSimFxLod& Lod, double RangeScale, double MarginM)
	{
		return FMath::Min(Lod.MaxRangeM * FMath::Max(RangeScale, 0.0), Lod.VisibilityM) + FMath::Max(MarginM, 0.0);
	}

	bool RibbonTakeOver(bool bRequesterContrail, double RequesterDistM, bool bVictimContrail, double VictimDistM, double Ratio)
	{
		if (!bVictimContrail)
		{
			return false;
		}
		return !bRequesterContrail || VictimDistM > FMath::Max(Ratio, 1.0) * FMath::Max(RequesterDistM, 0.0);
	}

	int32 RibbonSectionQuads(int32 Quads, int32 Current, int32 Chunk, int32 ShrinkAfter, int32& InOutLowFrames, bool& bOutRebuild)
	{
		const int32 C = FMath::Max(Chunk, 1);
		const int32 Wanted = Quads > 0 ? ((Quads + C - 1) / C) * C : 0;
		bOutRebuild = false;
		if (Quads > Current)
		{
			InOutLowFrames = 0;
			bOutRebuild = true;
			return Wanted;
		}
		if (Wanted < Current / 2)
		{
			if (++InOutLowFrames >= FMath::Max(ShrinkAfter, 1))
			{
				InOutLowFrames = 0;
				bOutRebuild = true;
				return Wanted;
			}
			return Current;
		}
		InOutLowFrames = 0;
		return Current;
	}

	float TrailPuffShare(double SinView, double DistCm)
	{
		const double Sin = FMath::Abs(SinView);
		const double ByAngle = 1.0 - FMath::SmoothStep(0.2, 0.5, Sin);
		// Seen end-on the strip draws nothing at all: the puffs go on the farther, to 2 km straight along it.
		const double NearCm = 20000.0 * FMath::Clamp(0.2 / FMath::Max(Sin, 1e-6), 1.0, 10.0);
		const double ByDist = 1.0 - FMath::SmoothStep(NearCm, 1.5 * NearCm, DistCm);
		return static_cast<float>(ByAngle * ByDist);
	}

	FLinearColor FlashLightColour(const FLinearColor& Base, const FLinearColor& Ember, double AgeS, double DurationS, double WhiteS)
	{
		if (AgeS < WhiteS)
		{
			return FLinearColor::White;
		}
		const double U = FMath::Clamp((AgeS - WhiteS) / FMath::Max(DurationS - WhiteS, 1e-3), 0.0, 1.0);
		return FMath::Lerp(Base, Ember, static_cast<float>(FMath::Sqrt(U)));
	}

	float ThinAt(float Age, float ThinS, float ThinTo)
	{
		if (ThinS <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Lerp(1.0f, ThinTo, Smooth01(FMath::Max(Age, 0.0f) / ThinS));
	}

	float CrowdShare(int32 Neighbours)
	{
		if (Neighbours <= 0)
		{
			return 1.0f;
		}
		return FMath::Max(2.0f / static_cast<float>(Neighbours + 2), 0.25f);
	}

	FVector PuffNormal(const FVector& Out, const FVector& ToCam)
	{
		const double Len = Out.Size();
		const FVector P = Len > 1.0 ? Out / Len : Out;
		return (P + 0.6 * ToCam).GetSafeNormal(1e-6, ToCam);
	}

	float SkyShare(double NormalZ, double Low)
	{
		const double Up = FMath::Clamp(0.5 + 0.5 * NormalZ, 0.0, 1.0);
		return static_cast<float>(Low + (1.0 - Low) * Up);
	}

	float SunShare(const FVector& Out, const FVector& ToCam, const FVector& ToSun, float Lit)
	{
		const FVector N = PuffNormal(Out, ToCam);
		const double Shade = FMath::Clamp(0.3 + 0.7 * FVector::DotProduct(N, ToSun), 0.0, 1.0);
		return static_cast<float>(0.75 * Shade + 0.25 * static_cast<double>(Lit));
	}

	float CameraFade(double DistCm, double SizeCm)
	{
		const double Size = FMath::Max(SizeCm, 0.0);
		return static_cast<float>(FMath::Clamp((DistCm - 100.0 - 0.5 * Size) / (300.0 + 0.5 * Size), 0.0, 1.0));
	}

	FLinearColor CapLight(const FLinearColor& Light, double Cap)
	{
		const float Peak = FMath::Max3(Light.R, Light.G, Light.B);
		if (Cap <= 0.0 || Peak <= Cap)
		{
			return Light;
		}
		FLinearColor C = Light * static_cast<float>(Cap / Peak);
		C.A = Light.A;
		return C;
	}

	FVector DirInCone(const FVector& Axis, double HalfAngleRad, double U1, double U2)
	{
		const FVector A = Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const double CosT = 1.0 - FMath::Clamp(U1, 0.0, 1.0) * (1.0 - FMath::Cos(FMath::Clamp(HalfAngleRad, 0.0, UE_DOUBLE_PI)));
		const double SinT = FMath::Sqrt(FMath::Max(1.0 - CosT * CosT, 0.0));
		const double Phi = 2.0 * UE_DOUBLE_PI * U2;
		const FVector Ref = FMath::Abs(A.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		const FVector X = FVector::CrossProduct(A, Ref).GetSafeNormal();
		const FVector Y = FVector::CrossProduct(A, X);
		return A * CosT + (X * FMath::Cos(Phi) + Y * FMath::Sin(Phi)) * SinT;
	}

	FVector BallisticAt(const FVector& P0, const FVector& V0, const FVector& Wind, double Drag, double Gravity, double T)
	{
		T = FMath::Max(T, 0.0);
		if (Drag < 1e-4)
		{
			return P0 + V0 * T - FVector(0.0, 0.0, 0.5 * Gravity * T * T);
		}
		const double E = 1.0 - FMath::Exp(-Drag * T);
		// dv/dt = k (W - v) - g z: v relaxes to W - (g / k) z.
		const FVector Terminal = Wind - FVector(0.0, 0.0, Gravity / Drag);
		return P0 + Terminal * T + (V0 - Terminal) * (E / Drag);
	}

	FVector RibbonPointAt(const FVector& Pos, const FVector& Drift, const FVector& Side, float RollS, float SinkCmps, float SinkMaxCm, float Age)
	{
		const float T = FMath::Max(Age, 0.0f);
		FVector P = Pos + Drift * T;
		if (RollS > 0.0f)
		{
			P += Side * (1.0f - FMath::Exp(-T / RollS));
		}
		else
		{
			P += Side;
		}
		if (SinkCmps > 0.0f && SinkMaxCm > 0.0f)
		{
			P.Z -= SinkMaxCm * (1.0f - FMath::Exp(-T * SinkCmps / SinkMaxCm));
		}
		return P;
	}

	float CrowAt(float DistM, float PeriodM, float Age, float FromS, float FullS)
	{
		if (PeriodM <= 0.0f || Age <= FromS)
		{
			return 0.0f;
		}
		const float Grown = FullS > FromS ? Smooth01((Age - FromS) / (FullS - FromS)) : 1.0f;
		return Grown * (0.5f + 0.5f * FMath::Cos(2.0f * UE_PI * DistM / PeriodM));
	}

	float TrailOpacityAt(float Age, float LifeS, float ThinS, float ThinShare)
	{
		const float Thin = ThinS > 0.0f ? FMath::Lerp(1.0f, ThinShare, Smooth01(Age / ThinS)) : 1.0f;
		return Thin * Smooth01(3.0f * (1.0f - Age / FMath::Max(LifeS, 0.01f)));
	}

	float TrailErosionAt(float Age, float LifeS, float From)
	{
		const float U = Age / FMath::Max(LifeS, 0.01f);
		const float X = FMath::Clamp((U - From) / FMath::Max(1.0f - From, 1e-3f), 0.0f, 1.0f);
		return 0.85f * X;
	}

	float NightShare(double SunElevationDeg)
	{
		return static_cast<float>(simcore::NightShare(SunElevationDeg));  // the simulation's own (simcore/light.hpp, R7)
	}

	FLinearColor TracerColourAt(const FLinearColor& Day, const FLinearColor& Night, double SunElevationDeg)
	{
		return FMath::Lerp(Day, Night, NightShare(SunElevationDeg));
	}

	FLinearColor TracerColourFor(const FLinearColor& Own, bool bOwn, const FLinearColor& Day, const FLinearColor& Night, double SunElevationDeg)
	{
		if (!bOwn)
		{
			return TracerColourAt(Day, Night, SunElevationDeg);
		}
		const bool bWarm = Own.R >= Own.G && Own.R >= Own.B;
		const FLinearColor OwnDay = bWarm ? Day : FMath::Lerp(Own, FLinearColor::White, 0.5f);
		return TracerColourAt(OwnDay, Own, SunElevationDeg);
	}

	double HelixRadiusAt(double DistM, double PitchM, double RadiusM, double Phase)
	{
		const double Turns = PitchM > 0.0 ? DistM / PitchM : 0.0;
		const double Wave = 0.6 * FMath::Sin(2.0 * UE_DOUBLE_PI * Turns / 2.7 + Phase) + 0.4 * FMath::Sin(2.0 * UE_DOUBLE_PI * Turns / 6.1 + 1.7 * Phase);
		return RadiusM * (1.0 + 0.5 * Wave);
	}

	bool IsTracedRound(int32 RoundIndex, int32 Every)
	{
		return Every <= 1 || (RoundIndex % Every) == 0;
	}

	void GunBurstSpan(double MinS, double MaxS, int32 MinRounds, double RateHz, double& OutMinS, double& OutMaxS)
	{
		OutMinS = MinS;
		OutMaxS = MaxS;
		if (MinRounds <= 1 || RateHz <= 0.0)
		{
			return;
		}
		// n rounds take (n - 1) / rate from the first to the last; a quarter of a period more so the last is in.
		OutMinS = FMath::Max(MinS, (static_cast<double>(MinRounds) - 0.75) / RateHz);
		OutMaxS = FMath::Max(MaxS, (2.0 * static_cast<double>(MinRounds) - 0.75) / RateHz);
	}

	int32 NamedIntFor(TConstArrayView<TPair<FString, int32>> Table, const FString& Id, const FString& OtherId, int32 Default)
	{
		for (const TPair<FString, int32>& Each : Table)
		{
			if (!Each.Key.IsEmpty() && ((!Id.IsEmpty() && Id.StartsWith(Each.Key, ESearchCase::IgnoreCase)) ||
				(!OtherId.IsEmpty() && OtherId.StartsWith(Each.Key, ESearchCase::IgnoreCase))))
			{
				return Each.Value;
			}
		}
		return Default;
	}

	void GunBursts(uint32 Seed, double T0, double T1, double BurstMinS, double BurstMaxS, double PauseMinS, double PauseMaxS,
		TArray<FGunBurst>& Out, int32 MaxBursts)
	{
		Out.Reset();
		// Its own small hash (not the effects' particle stream): only the seed decides the rhythm.
		uint32 State = Seed * 2654435761u + 0x51ED27u;
		const auto Next = [&State]()
		{
			State += 0x9E3779B9u;
			uint32 X = State;
			X ^= X >> 16;
			X *= 0x7feb352du;
			X ^= X >> 15;
			X *= 0x846ca68bu;
			X ^= X >> 16;
			return static_cast<double>(X & 0xFFFFFFu) / 16777216.0;
		};
		const double BurstLo = FMath::Max(BurstMinS, 0.01), BurstHi = FMath::Max(BurstMaxS, BurstLo);
		const double PauseLo = FMath::Max(PauseMinS, 0.0), PauseHi = FMath::Max(PauseMaxS, PauseLo);
		double Start = T0;
		while (Start < T1 && Out.Num() < MaxBursts)
		{
			FGunBurst B;
			B.StartS = Start;
			B.EndS = FMath::Min(Start + BurstLo + (BurstHi - BurstLo) * Next(), T1);
			Out.Add(B);
			Start = B.EndS + PauseLo + (PauseHi - PauseLo) * Next();
		}
	}

	void GunBurstShots(const FGunBurst& Burst, double RateHz, double T1, TArray<double>& Out)
	{
		Out.Reset();
		const double Step = 1.0 / FMath::Max(RateHz, 0.01);
		for (int32 K = 0; K < 100000; ++K)
		{
			const double T = Burst.StartS + K * Step;
			if (K > 0 && (T >= Burst.EndS || T >= T1))
			{
				break;
			}
			Out.Add(T);
		}
	}

	double TracerLengthCm(double SpeedCmps, double FrameS, double MinFrameS)
	{
		return FMath::Max(SpeedCmps, 0.0) * FMath::Max(FrameS, MinFrameS);
	}

	FVector HelixOffset(const FVector& Fwd, double DistM, double PitchM, double RadiusM, double Phase)
	{
		const FVector F = Fwd.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		FVector A = FVector::CrossProduct(F, FMath::Abs(F.Z) < 0.95 ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
		const FVector B = FVector::CrossProduct(F, A);
		const double Turn = PitchM > 0.0 ? 2.0 * UE_DOUBLE_PI * DistM / PitchM + Phase : Phase;
		return (A * FMath::Cos(Turn) + B * FMath::Sin(Turn)) * (RadiusM * 100.0);
	}
}

FSimFxSprite FSimFxParticle::ToSprite() const
{
	FSimFxSprite S;
	const float U = FMath::Clamp(Age / FMath::Max(Life, 1e-3f), 0.0f, 1.0f);
	const float Size = Size0 + (Size1 - Size0) * SimFxLogic::GrowShare(Age, Life, GrowS) + SpreadCmps * FMath::Max(Age, 0.0f);
	float Fade = 1.0f;
	if (FadeInS > 0.0f && Age < FadeInS)
	{
		Fade = FMath::Max(Age, 0.0f) / FadeInS;
	}
	if (ThinS > 0.0f)
	{
		Fade *= SimFxLogic::ThinAt(Age, ThinS, ThinTo);
	}
	if (U > FadeFrom)
	{
		Fade *= 1.0f - SimFxLogic::Smooth01((U - FadeFrom) / FMath::Max(1.0f - FadeFrom, 1e-3f));
	}
	S.Pos = Pos;
	S.Width = Size;
	S.Length = Size * Stretch;
	S.Roll = Roll + Spin * Age;
	S.Frame = Frames > 0.0f ? FMath::Min(Frame0 + Frames * U, Frame0 + Frames - 0.001f) : Frame0;
	S.Opacity = Opacity * Fade;
	S.Heat = SimFxLogic::HeatAt(Heat0, Heat1, U, Age, HeatHoldS, HeatTauS);
	S.Tint = bTintOverLife ? FMath::Lerp(Tint, Tint1, SimFxLogic::Smooth01(U)) : Tint;
	S.Erosion = SimFxLogic::ErosionAt(U);
	S.MinPx = MinPx;
	S.Layer = Layer;
	S.Facing = Facing;
	S.Kind = Kind;
	if (Facing == ESimFxFacing::Axis)
	{
		if (!FixedAxis.IsNearlyZero())
		{
			S.Axis = FixedAxis.GetSafeNormal();
		}
		else
		{
			const double Speed = Vel.Size();
			S.Axis = Speed > 1.0 ? Vel / Speed : FVector::UpVector;
		}
		if (LengthK > 0.0f)
		{
			const float T = RiseStopS > 0.0f ? FMath::Min(Age, RiseStopS) : Age;
			S.Length = FMath::Max(S.Length, LengthK * FMath::Sqrt(FMath::Max(T, 0.0f)));
		}
		if (bRooted)
		{
			S.Pos = Pos + S.Axis * (0.5 * S.Length);
		}
	}
	return S;
}

bool FSimFxParticles::Spawn(const FSimFxParticle& P)
{
	if (Total >= Max || P.Life <= 0.0f || P.Age >= P.Life)
	{
		return false;
	}
	Layers[static_cast<int32>(P.Layer)].Add(P);
	++Total;
	return true;
}

void FSimFxParticles::Update(float Dt)
{
	Total = 0;
	for (TArray<FSimFxParticle>& Arr : Layers)
	{
		for (int32 I = Arr.Num() - 1; I >= 0; --I)
		{
			FSimFxParticle& P = Arr[I];
			const float Age0 = P.Age;
			P.Age += Dt;
			if (P.Age >= P.Life)
			{
				Arr.RemoveAtSwap(I, EAllowShrinking::No);
				continue;
			}
			if (P.Age <= 0.0f)
			{
				continue;  // still waiting for its time
			}
			if (P.RiseK > 0.0f)
			{
				// A column's member: up by RiseK (sqrt(t1) - sqrt(t0)) until the column stabilises.
				const float Stop = P.RiseStopS > 0.0f ? P.RiseStopS : P.Life;
				P.Pos.Z += P.RiseK * (FMath::Sqrt(FMath::Clamp(P.Age, 0.0f, Stop)) - FMath::Sqrt(FMath::Clamp(Age0, 0.0f, Stop)));
			}
			if (Dt > 0.0f)
			{
				if (P.Drag > 0.0f)
				{
					const float K = 1.0f - FMath::Exp(-P.Drag * Dt);
					P.Vel += (P.Wind - P.Vel) * K;
				}
				const float Lift = P.Buoyancy * FMath::Max(0.0f, 1.0f - P.Age / P.Life) - P.Gravity;
				P.Vel.Z += Lift * Dt;
				P.Pos += P.Vel * Dt;
			}
		}
		Total += Arr.Num();
	}
}

void FSimFxParticles::Clear()
{
	for (TArray<FSimFxParticle>& Arr : Layers)
	{
		Arr.Reset();
	}
	Total = 0;
}

FSimFxWrecks::FWreck& FSimFxWrecks::Add(int32 Id, double StartT, const simcore::fx::WreckFallParams& Params, bool bFire, const FString& Model)
{
	FWreck* W = Find(Id);
	if (W == nullptr)
	{
		W = &Wrecks.AddDefaulted_GetRef();
	}
	W->Id = Id;
	W->StartT = StartT;
	W->Fall = simcore::fx::WreckFall(Params);
	W->bFire = bFire;
	W->bLanded = false;
	W->Model = Model;
	return *W;
}

const FSimFxWrecks::FWreck* FSimFxWrecks::Find(int32 Id) const
{
	for (const FWreck& W : Wrecks)
	{
		if (W.Id == Id)
		{
			return &W;
		}
	}
	return nullptr;
}

FSimFxWrecks::FWreck* FSimFxWrecks::Find(int32 Id)
{
	for (FWreck& W : Wrecks)
	{
		if (W.Id == Id)
		{
			return &W;
		}
	}
	return nullptr;
}

bool FSimFxWrecks::PoseEnu(int32 Id, double Now, simcore::Vec3& OutPos, simcore::fx::WreckRotation& OutRot, bool& bOutVisible) const
{
	const FWreck* W = Find(Id);
	if (W == nullptr)
	{
		return false;
	}
	const double Since = Now - W->StartT;
	OutPos = W->Fall.Position(Since);
	OutRot = W->Fall.Rotation(Since);
	bOutVisible = Since < W->Fall.GroundTime();
	return true;
}
