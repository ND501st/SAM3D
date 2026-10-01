#include "SimCabinHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "SimOsaVehicle.h"

void ASimCabinHUD::DrawHUD()
{
	Super::DrawHUD();
	const APlayerController* Pc = GetOwningPlayerController();
	const ASimOsaVehicle* Vehicle = Pc != nullptr ? Cast<ASimOsaVehicle>(Pc->GetPawn()) : nullptr;
	if (Vehicle == nullptr || !Vehicle->IsInCabin() || Canvas == nullptr)
	{
		return;
	}
	const float Cx = Canvas->ClipX * 0.5f;
	const float Cy = Canvas->ClipY * 0.5f;
	const FLinearColor Ink(1.0f, 1.0f, 1.0f, 0.8f);
	DrawLine(Cx - 8.0f, Cy, Cx + 8.0f, Cy, Ink, 1.5f);
	DrawLine(Cx, Cy - 8.0f, Cx, Cy + 8.0f, Ink, 1.5f);

	UFont* Font = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	const FString Hover = Vehicle->GetCabinHoverText();
	if (!Hover.IsEmpty())
	{
		DrawText(Hover, FLinearColor(1.0f, 0.9f, 0.4f), Cx + 16.0f, Cy + 12.0f, Font);
	}
	const FString Feedback = Vehicle->GetCabinFeedbackText();
	if (!Feedback.IsEmpty())
	{
		DrawText(Feedback, FLinearColor(1.0f, 0.4f, 0.3f), Cx + 16.0f, Cy + 32.0f, Font);
	}
	DrawText(FString::Printf(TEXT("%s   |   C leave cabin   Tab next seat   left/right click operate   drag or scroll wheels"),
				 *Vehicle->GetCabinStationLabel()),
		Ink, 20.0f, Canvas->ClipY - 40.0f, Font);
}
