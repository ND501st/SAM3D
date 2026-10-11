#include "SimEmLayer.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SAM3D.h"
#include "SimWorldSubsystem.h"

namespace
{
// Default: everything except the busy per-look layers (line of sight, signal paths, detection numbers).
constexpr int32 DefaultMask = 1 | 2 | 4 | 8 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096 | 8192;

TAutoConsoleVariable<int32> CVarEmDebug(TEXT("sam3d.em.Debug"), DefaultMask,
	TEXT("Electromagnetic-layer debug overlays, a bit mask (0 = none, -1 = all): 1 truth, 2 emitters, 4 beams, "
		 "8 coverage, 16 line of sight, 32 signal paths, 64 bearings, 128 fixes, 256 uncertainty, 512 tracks, "
		 "1024 C2 links, 2048 comms traffic, 4096 jamming, 8192 sensor status, 16384 detection."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarEmOwner(TEXT("sam3d.em.Owner"), 0,
	TEXT("Whose knowledge the electromagnetic-layer overlays show: a node id, 0 = command posts, -1 = every node."),
	ECVF_Default);

USimWorldSubsystem* SubsystemOf(UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
}

FAutoConsoleCommandWithWorld CmdEmStart(TEXT("sam3d.em.Start"),
	TEXT("Start the electromagnetic-layer demo at the current sim origin."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
		if (USimWorldSubsystem* Sim = SubsystemOf(World))
		{
			FString Error;
			if (!Sim->StartEmDemo(Error))
			{
				UE_LOG(LogSAM3D, Warning, TEXT("sam3d.em.Start: %s"), *Error);
			}
		}
	}));

FAutoConsoleCommandWithWorld CmdEmStop(TEXT("sam3d.em.Stop"), TEXT("Stop the electromagnetic-layer demo."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
		if (USimWorldSubsystem* Sim = SubsystemOf(World))
		{
			Sim->StopEmDemo();
		}
	}));

FColor ToColor(uint32 Rgba)
{
	return FColor(static_cast<uint8>(Rgba >> 24), static_cast<uint8>((Rgba >> 16) & 0xFF), static_cast<uint8>((Rgba >> 8) & 0xFF),
		static_cast<uint8>(Rgba & 0xFF));
}

simcore::Vec3 Along(const simcore::Vec3& From, double Azimuth, double Length)
{
	return From + simcore::Vec3{FMath::Sin(Azimuth), FMath::Cos(Azimuth), 0.0} * Length;
}
}  // namespace

FSimEmLayer::FSimEmLayer() = default;
FSimEmLayer::~FSimEmLayer() = default;

bool FSimEmLayer::StartDemo(const FString& DataDirectory, const simcore::GeoPoint& Origin, const simcore::Terrain* Terrain,
	FString& OutError)
{
	FString Text;
	const FString Path = FPaths::Combine(DataDirectory, TEXT("em/generic.json"));
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *Path);
		return false;
	}
	simcore::em::EmLibrary Lib;
	const simcore::em::EmLoadResult Load = simcore::em::LoadEmLibraryText(TCHAR_TO_UTF8(*Text), Lib);
	if (!Load.ok)
	{
		OutError = UTF8_TO_TCHAR(Load.error.c_str());
		return false;
	}
	TUniquePtr<simcore::em::EmDemoScenario> NewDemo = MakeUnique<simcore::em::EmDemoScenario>(Origin, Terrain, 7);
	std::string Error;
	if (!NewDemo->Build(Lib, &Error))
	{
		OutError = UTF8_TO_TCHAR(Error.c_str());
		return false;
	}
	Demo = MoveTemp(NewDemo);
	Accumulator = 0.0;
	bPrimsDirty = true;
	UE_LOG(LogSAM3D, Log, TEXT("EM layer demo started at %.5f, %.5f (sam3d.em.Debug selects the overlays)"), Origin.LatDeg(),
		Origin.LonDeg());
	return true;
}

void FSimEmLayer::Stop()
{
	Demo.Reset();
	Prims.clear();
}

bool FSimEmLayer::IsRunning() const
{
	return Demo.IsValid();
}

void FSimEmLayer::Advance(double DeltaSeconds)
{
	if (!Demo)
	{
		return;
	}
	Accumulator += FMath::Min(DeltaSeconds, 0.5);
	int32 Steps = 0;
	while (Accumulator >= StepSeconds && Steps < 10)
	{
		Demo->Step(StepSeconds);
		Accumulator -= StepSeconds;
		++Steps;
	}
	if (Steps > 0)
	{
		bPrimsDirty = true;
	}
}

double FSimEmLayer::GetTimeSeconds() const
{
	return Demo ? Demo->Layer().Time() : 0.0;
}

const simcore::em::EmScene* FSimEmLayer::GetScene() const
{
	return Demo ? &Demo->Layer().Scene() : nullptr;
}

FString FSimEmLayer::GetStatusText() const
{
	if (!Demo)
	{
		return TEXT("EM layer not running");
	}
	const simcore::em::EmScene& S = Demo->Layer().Scene();
	FString Out = FString::Printf(TEXT("t=%.1f s"), S.time_s);
	for (const simcore::em::NodeState& N : S.nodes)
	{
		Out += FString::Printf(TEXT("\n%s: %s, %d items"), UTF8_TO_TCHAR(N.name.c_str()),
			N.alive ? UTF8_TO_TCHAR(simcore::em::C2StatusName(N.status)) : TEXT("destroyed"), static_cast<int32>(N.kb.Size()));
	}
	for (const simcore::em::RadarState& R : S.radars)
	{
		const simcore::em::RadarDef& D = S.lib.radars[static_cast<size_t>(R.def_index)];
		Out += FString::Printf(TEXT("\n%s #%d: %s, %d tracks"), UTF8_TO_TCHAR(D.name.c_str()), R.id, R.on ? TEXT("on") : TEXT("off"),
			static_cast<int32>(R.tracker.Tracks().size()));
	}
	return Out;
}

void FSimEmLayer::Draw(UWorld* World, TFunctionRef<FVector(const simcore::Vec3&)> ToUnreal)
{
#if ENABLE_DRAW_DEBUG
	if (!Demo || World == nullptr)
	{
		return;
	}
	const int32 Mask = CVarEmDebug.GetValueOnGameThread();
	const int32 Owner = CVarEmOwner.GetValueOnGameThread();
	if (Mask == 0)
	{
		return;
	}
	if (bPrimsDirty || Mask != PrimsMask || Owner != PrimsOwner)
	{
		Prims = simcore::em::BuildDebugScene(Demo->Layer().Scene(), static_cast<uint32>(Mask), Owner, 2.0);
		PrimsMask = Mask;
		PrimsOwner = Owner;
		bPrimsDirty = false;
	}
	using simcore::Vec3;
	using simcore::em::DebugShape;
	auto Segment = [&](const Vec3& A, const Vec3& B, const FColor& Color, bool bDashed) {
		const FVector UA = ToUnreal(A), UB = ToUnreal(B);
		if (!bDashed)
		{
			DrawDebugLine(World, UA, UB, Color, false, -1.f, SDPG_World, 0.f);
			return;
		}
		// 500 m dashes (UE 5.8 has no world-space dashed debug line).
		const double Length = FVector::Dist(UA, UB);
		const int32 Dashes = FMath::Clamp(static_cast<int32>(Length / 100000.0), 1, 200);
		for (int32 K = 0; K < Dashes; ++K)
		{
			const double T0 = static_cast<double>(K) / Dashes, T1 = (K + 0.5) / Dashes;
			DrawDebugLine(World, FMath::Lerp(UA, UB, T0), FMath::Lerp(UA, UB, T1), Color, false, -1.f, SDPG_World, 0.f);
		}
	};
	auto Ring = [&](const Vec3& C, double Major, double Minor, double Azimuth, const FColor& Color, bool bDashed) {
		const Vec3 U{FMath::Sin(Azimuth), FMath::Cos(Azimuth), 0.0};
		const Vec3 V{FMath::Cos(Azimuth), -FMath::Sin(Azimuth), 0.0};
		const int32 N = 48;
		Vec3 Prev = C + U * Major;
		for (int32 I = 1; I <= N; ++I)
		{
			const double T = 2.0 * UE_DOUBLE_PI * I / N;
			const Vec3 P = C + U * (Major * FMath::Cos(T)) + V * (Minor * FMath::Sin(T));
			Segment(Prev, P, Color, bDashed);
			Prev = P;
		}
	};
	for (const simcore::em::DebugPrim& P : Prims)
	{
		const FColor Color = ToColor(P.rgba);
		switch (P.shape)
		{
			case DebugShape::Line:
				Segment(P.a, P.b, Color, P.dashed);
				break;
			case DebugShape::Circle:
				Ring(P.a, P.r1, P.r1, 0.0, Color, P.dashed);
				break;
			case DebugShape::Ellipse:
				Ring(P.a, P.r1, P.r2, P.angle, Color, false);
				break;
			case DebugShape::Sector:
			{
				const int32 N = 8;
				Vec3 Prev = P.a;
				for (int32 I = 0; I <= N; ++I)
				{
					const Vec3 Q = Along(P.a, P.angle - P.angle2 + 2.0 * P.angle2 * I / N, P.r1);
					Segment(Prev, Q, Color, false);
					Prev = Q;
				}
				Segment(Prev, P.a, Color, false);
				break;
			}
			case DebugShape::Text:
				DrawDebugString(World, ToUnreal(P.a), UTF8_TO_TCHAR(P.text.c_str()), nullptr, Color, -1.f, false, 1.f);
				break;
			case DebugShape::Point:
				DrawDebugPoint(World, ToUnreal(P.a), P.width * 2.f, Color, false, -1.f, SDPG_World);
				if (!P.text.empty())
				{
					DrawDebugString(World, ToUnreal(P.a), UTF8_TO_TCHAR(P.text.c_str()), nullptr, Color, -1.f, false, 1.f);
				}
				break;
			case DebugShape::Polygon:
				for (size_t I = 0; I < P.points.size(); ++I)
				{
					Segment(P.points[I], P.points[(I + 1) % P.points.size()], Color, P.dashed);
				}
				break;
		}
	}
#endif
}
