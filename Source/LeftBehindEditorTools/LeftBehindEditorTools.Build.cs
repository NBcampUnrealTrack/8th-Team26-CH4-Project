using UnrealBuildTool;

public class LeftBehindEditorTools : ModuleRules
{
	public LeftBehindEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"LeftBehind",
			"SlateCore",
			"UnrealEd",
			"Kismet",
			"BlueprintGraph",
			"BlueprintEditorLibrary",
			"UMG",
			"UMGEditor"
		});
	}
}
