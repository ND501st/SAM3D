using UnrealBuildTool;

public class SAM3DTarget : TargetRules
{
	public SAM3DTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SAM3D");
	}
}
