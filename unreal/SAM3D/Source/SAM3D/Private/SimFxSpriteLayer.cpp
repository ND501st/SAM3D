#include "SimFxSpriteLayer.h"

void USimFxSpriteLayerComponent::SetFxBounds(const FBox& LocalBox)
{
	FxLocalBox = LocalBox;
}

FBoxSphereBounds USimFxSpriteLayerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (FxLocalBox.IsValid)
	{
		return FBoxSphereBounds(FxLocalBox).TransformBy(LocalToWorld);
	}
	return Super::CalcBounds(LocalToWorld);
}

void USimFxSpriteLayerComponent::UpdateBounds()
{
	if (FxLocalBox.IsValid)
	{
		// Past the instanced component's pass over its instances: the static mesh component's update calls CalcBounds above.
		UStaticMeshComponent::UpdateBounds();
		return;
	}
	Super::UpdateBounds();
}
