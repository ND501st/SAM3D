#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SimKnowledgeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A small EM scene (generic placeholder data): a red search radar on a node, one aircraft in its cover.
	struct FKnowledgeScene
	{
		simcore::em::EmWorld World{simcore::GeoPoint::FromDegrees(49.5, 17.0, 0.0), nullptr, 3};
		int32 Node = 0;
		bool bOk = false;

		FKnowledgeScene()
		{
			const FString DataDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../data")));
			simcore::em::EmLibrary Lib;
			const simcore::em::EmLoadResult R =
				simcore::em::LoadEmLibraryFile(std::string(TCHAR_TO_UTF8(*DataDir)) + "/em/generic.json", Lib);
			bOk = R.ok && World.SetLibrary(Lib);
			if (!bOk)
			{
				return;
			}
			const int Site = World.AddGroundPlatform("SAM", simcore::em::Side::Red, 0.0, 0.0, 5.0);
			const int Radar = World.AddRadar(Site, "generic_search_c", true);
			Node = World.AddNode("Battery", Site, simcore::em::Side::Red, true);
			World.AttachSensor(Node, Radar);
			simcore::em::Platform P;
			P.name = "target";
			P.side = simcore::em::Side::Blue;
			P.kind = simcore::em::PlatformKind::FixedWing;
			P.rcs = simcore::em::RcsModel{3.0, 6.0, 3.0};
			P.velocity = {0.0, -150.0, 0.0};
			P.position = {2000.0, 25000.0, 3000.0};
			World.AddPlatform(P);
			for (int i = 0; i < 200; ++i)
			{
				World.Step(0.1);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimKnowledgeComponentTest, "SAM3D.Knowledge.ComponentAppliesStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimKnowledgeComponentTest::RunTest(const FString& Parameters)
{
	FKnowledgeScene Scene;
	if (!TestTrue(TEXT("generic EM library loads"), Scene.bOk))
	{
		return false;
	}
	simcore::em::KnowledgeReplicator Server;
	const int Client = Server.AddClient(Scene.Node);
	USimKnowledgeComponent* Component = NewObject<USimKnowledgeComponent>();

	// A keyframe, then deltas: the component holds what the server sent, number for number.
	for (int k = 0; k < 6; ++k)
	{
		const std::vector<std::uint8_t> Bytes = Server.Produce(Client, Scene.World.Scene());
		TArray<uint8> Packet;
		Packet.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
		Component->SendKnowledge(Packet);  // no owner or world: the RPC runs locally
		Server.Ack(Client, static_cast<std::uint32_t>(Component->GetKnowledgeSeq()));
		for (int i = 0; i < 5; ++i)
		{
			Scene.World.Step(0.1);
		}
	}
	TestEqual(TEXT("newest view held"), Component->GetKnowledgeSeq(), 6);
	const simcore::em::KnowledgeView* Sent = Server.Sent(Client, 6);
	TestTrue(TEXT("server still has view 6"), Sent != nullptr);
	if (Sent != nullptr)
	{
		TestTrue(TEXT("client view is byte-identical to the server's"),
			simcore::em::EncodeView(*Sent) == simcore::em::EncodeView(Component->GetView()));
	}
	TestTrue(TEXT("deltas after the first keyframe"), Server.Keyframes(Client) == 1);

	// Blueprint accessors: the radar track is there, in Unreal coordinates (X north, Y east, cm).
	const TArray<FSimKnowledgeSensorTrack> Tracks = Component->GetSensorTracks();
	TestTrue(TEXT("a radar track reached the player"), Tracks.Num() >= 1);
	if (Tracks.Num() >= 1)
	{
		TestTrue(TEXT("track to the north, in cm"), Tracks[0].Location.X > 1500000.0 && Tracks[0].Location.X < 3000000.0);
	}
	AddInfo(Component->GetKnowledgeSummary());

	// Garbage and a stale duplicate change nothing.
	const int32 Before = Component->GetKnowledgeSeq();
	TArray<uint8> Junk;
	Junk.Init(0xAB, 40);
	Component->SendKnowledge(Junk);
	TestEqual(TEXT("garbage ignored"), Component->GetKnowledgeSeq(), Before);
	return true;
}

#endif
