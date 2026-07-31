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

		// Json reads Config/CigRecipes.json and writes the dialogue prompt file.
		//
		// HTTP was here for a runtime request to a hosted language model. The
		// shipped game makes no network request, so the dependency is gone: a
		// retail client that cannot reach the internet is a property worth being
		// unable to break by accident.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
