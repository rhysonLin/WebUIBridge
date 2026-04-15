using UnrealBuildTool;

public class WebUIBridge : ModuleRules
{
	public WebUIBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"WebBrowserWidget"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
