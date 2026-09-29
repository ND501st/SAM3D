using UnrealBuildTool;

public class SAM3DEditorTarget : TargetRules
{
	public SAM3DEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SAM3D");
	}
}
