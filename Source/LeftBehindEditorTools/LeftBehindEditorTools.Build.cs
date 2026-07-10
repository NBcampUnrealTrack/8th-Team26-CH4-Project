using UnrealBuildTool;

public class LeftBehindEditorTools : ModuleRules
{
	public LeftBehindEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"LeftBehind",
			"UnrealEd",
			"Kismet",
			"BlueprintGraph",
			"BlueprintEditorLibrary",
			"UMG",
			"UMGEditor"
		});
	}
}
