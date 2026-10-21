#include "SimKnowledgeComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SimWorldSubsystem.h"

USimKnowledgeComponent::USimKnowledgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Mirror = MakeUnique<simcore::em::KnowledgeMirror>();
}

void USimKnowledgeComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC != nullptr && PC->IsLocalController() && GetWorld() != nullptr)
	{
		if (USimWorldSubsystem* Sim = GetWorld()->GetSubsystem<USimWorldSubsystem>())
		{
			Sim->SetLocalKnowledge(this);
		}
	}
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
		{
			Sim->RegisterKnowledgeClient(this);
		}
	}
}

void USimKnowledgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USimWorldSubsystem* Local = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Local->ClearLocalKnowledge(this);
	}
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
		{
			Sim->UnregisterKnowledgeClient(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void USimKnowledgeComponent::ClientReceiveKnowledge_Implementation(const TArray<uint8>& Bytes)
{
	const std::vector<std::uint8_t> Packet(Bytes.GetData(), Bytes.GetData() + Bytes.Num());
	BytesReceived += Bytes.Num();
	std::uint32_t Ack = 0;
	if (Mirror->Receive(Packet, &Ack))
	{
		ServerAckKnowledge(static_cast<int32>(Ack));
		OnKnowledgeUpdated.Broadcast(static_cast<int32>(Ack));
	}
}

void USimKnowledgeComponent::ServerAckKnowledge_Implementation(int32 Seq)
{
	if (Seq <= 0)
	{
		return;
	}
	if (USimWorldSubsystem* Sim = GetWorld() != nullptr ? GetWorld()->GetSubsystem<USimWorldSubsystem>() : nullptr)
	{
		Sim->AckKnowledge(this, static_cast<uint32>(Seq));
	}
}

const simcore::em::KnowledgeView& USimKnowledgeComponent::GetView() const
{
	return Mirror->View();
}

namespace
{
	FVector ToUnrealPoint(const UWorld* World, const simcore::Vec3& Enu)
	{
		const USimWorldSubsystem* Sim = World != nullptr ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
		return Sim != nullptr ? Sim->ToUnreal(Enu) : USimWorldSubsystem::EnuToUnreal(Enu);
	}

	float AzimuthDegrees(double Rad)
	{
		double Deg = FMath::RadiansToDegrees(Rad);
		Deg = FMath::Fmod(Deg, 360.0);
		return static_cast<float>(Deg < 0.0 ? Deg + 360.0 : Deg);
	}
}

TArray<FSimKnowledgeTrack> USimKnowledgeComponent::GetPictureTracks() const
{
	TArray<FSimKnowledgeTrack> Out;
	for (const simcore::em::ViewTrack& T : Mirror->View().tracks)
	{
		FSimKnowledgeTrack K;
		K.Id = T.id;
		K.IdentityCode = T.identity;
		K.CategoryCode = T.category;
		K.bPositionValid = T.position_valid;
		K.Location = T.position_valid ? ToUnrealPoint(GetWorld(), T.position) : FVector::ZeroVector;
		K.Velocity = USimWorldSubsystem::EnuToUnreal(T.velocity);
		K.SigmaMeters = static_cast<float>(T.sigma_h_m);
		K.bFromFix = T.from_fix;
		K.bBearingOnly = T.bearing_only;
		K.BearingOrigin = T.bearing_only ? ToUnrealPoint(GetWorld(), T.bearing_origin) : FVector::ZeroVector;
		K.BearingDegrees = AzimuthDegrees(T.bearing_rad);
		K.TypeGuess = UTF8_TO_TCHAR(T.type_guess.c_str());
		Out.Add(K);
	}
	return Out;
}

TArray<FSimKnowledgeContact> USimKnowledgeComponent::GetEsmContacts() const
{
	TArray<FSimKnowledgeContact> Out;
	for (const simcore::em::ViewContact& C : Mirror->View().contacts)
	{
		FSimKnowledgeContact K;
		K.Key = C.key;
		K.BearingDegrees = AzimuthDegrees(C.bearing_rad);
		K.SigmaDegrees = static_cast<float>(FMath::RadiansToDegrees(C.sigma_rad));
		K.LibraryName = UTF8_TO_TCHAR(C.library_name.c_str());
		K.bHostile = C.hostile;
		K.bSteady = C.steady;
		Out.Add(K);
	}
	return Out;
}

TArray<FSimKnowledgeSensorTrack> USimKnowledgeComponent::GetSensorTracks() const
{
	TArray<FSimKnowledgeSensorTrack> Out;
	for (const simcore::em::ViewSensorTrack& T : Mirror->View().sensor_tracks)
	{
		FSimKnowledgeSensorTrack K;
		K.Key = T.key;
		K.bConfirmed = T.confirmed;
		K.Location = ToUnrealPoint(GetWorld(), T.position);
		K.Velocity = USimWorldSubsystem::EnuToUnreal(T.velocity);
		K.SigmaMeters = static_cast<float>(T.sigma_m);
		Out.Add(K);
	}
	return Out;
}

int32 USimKnowledgeComponent::GetKnowledgeSeq() const
{
	return static_cast<int32>(Mirror->View().seq);
}

double USimKnowledgeComponent::GetKnowledgeTimeSeconds() const
{
	return Mirror->View().time_s;
}

FString USimKnowledgeComponent::GetKnowledgeSummary() const
{
	const simcore::em::KnowledgeView& V = Mirror->View();
	return FString::Printf(TEXT("view %u (node %d, t %.1f s): %d picture items, %d ESM contacts, %d radar tracks; %lld bytes received"),
		V.seq, V.node, V.time_s, static_cast<int32>(V.tracks.size()), static_cast<int32>(V.contacts.size()),
		static_cast<int32>(V.sensor_tracks.size()), BytesReceived);
}
