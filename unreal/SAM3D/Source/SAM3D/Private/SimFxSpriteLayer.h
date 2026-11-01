#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "SimFxSpriteLayer.generated.h"

/**
 * One sprite layer of the weapons' effects (USimFxSubsystem): an instanced quad whose bounds the subsystem sets each
 * frame from the sprites it draws, in the component's own space. The stock component works its bounds out over every
 * instance it holds, the hidden spares at their old places included; the subsystem has the drawn sprites' box from the
 * pass it makes anyway, so the bounds are tight and cost nothing more.
 */
UCLASS(ClassGroup = SAM3D)
class USimFxSpriteLayerComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	/** The drawn sprites' box in the component's space; an invalid box when none is drawn (the stock bounds then). */
	void SetFxBounds(const FBox& LocalBox);
	const FBox& GetFxBounds() const { return FxLocalBox; }

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void UpdateBounds() override;

private:
	FBox FxLocalBox = FBox(ForceInit);
};
