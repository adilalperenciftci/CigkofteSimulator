using UnrealBuildTool;
using System.Collections.Generic;

public class CigkofteSimulatorTarget : TargetRules
{
	public CigkofteSimulatorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CigkofteSimulator");
	}
}
