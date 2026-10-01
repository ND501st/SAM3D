using System.IO;
using UnrealBuildTool;

public class SAM3D : ModuleRules
{
	public SAM3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent",  // cabin screens: quads with exact texture coordinates
			"CesiumRuntime"  // Cesium for Unreal: georeference, terrain height sampling
		});

		// simcore is a header-only C++17 library that lives at the repo root (../../../../simcore).
		// It is shared with the standalone tests and runs on any platform.
		string SimCoreInclude = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "..", "simcore", "include"));
		PublicIncludePaths.Add(SimCoreInclude);
	}
}
