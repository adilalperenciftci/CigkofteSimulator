using UnrealBuildTool;

public class CigkofteSimulator : ModuleRules
{
	public CigkofteSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Subfoldered layout: includes are written relative to the module root
		// (e.g. "Core/CigkofteTypes.h").
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// For moving the tablet UI to UMG (see UI/CigTabletWidget.h). The
			// in-game HUD stays on Canvas, where hand drawing is faster and the
			// layout is fixed anyway.
			"UMG",
			"Slate",
			"SlateCore"
		});

		// The AI customer dialogue path: an async HTTP request to the provider plus
		// JSON parsing. The API key comes from the environment and is never
		// committed to the repo.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json"
		});
	}
}
