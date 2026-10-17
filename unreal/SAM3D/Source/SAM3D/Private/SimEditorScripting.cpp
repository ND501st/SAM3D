#include "SimEditorScripting.h"

#include "Engine/StaticMesh.h"

bool USimEditorScripting::SetDistanceFieldScale(UStaticMesh* Mesh, float Scale)
{
#if WITH_EDITOR
	if (Mesh == nullptr || Mesh->GetNumSourceModels() == 0)
	{
		return false;
	}
	Mesh->Modify();
	Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale = Scale;
	Mesh->Build(true);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

float USimEditorScripting::GetDistanceFieldScale(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	if (Mesh != nullptr && Mesh->GetNumSourceModels() > 0)
	{
		return Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale;
	}
#endif
	return -1.0f;
}
