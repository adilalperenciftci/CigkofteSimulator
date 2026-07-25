using UnrealBuildTool;
using System.Collections.Generic;

public class CigkofteSimulatorEditorTarget : TargetRules
{
	public CigkofteSimulatorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CigkofteSimulator");
	}
}
