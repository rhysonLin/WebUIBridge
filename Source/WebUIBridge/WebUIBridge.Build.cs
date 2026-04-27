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
			"WebBrowserWidget",
			"CinematicCamera"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
