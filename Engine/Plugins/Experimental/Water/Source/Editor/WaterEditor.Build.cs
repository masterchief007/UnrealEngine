// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WaterEditor : ModuleRules
{
	public WaterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PrivateIncludePaths.Add("Editor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"CoreUObject",
                "ComponentVisualizers",
				"DetailCustomizations",
				"EditorStyle",
				"Engine",
				"InputCore",
				"SlateCore",
				"Slate",
				"UnrealEd",
				"Water",
                "Projects",
				"PropertyEditor",
				"Landscape",
				"LandscapeEditorUtilities",
				"Landmass",
				"EditorSubsystem",
				"ComponentVisualizers",
				"DeveloperSettings",
				"RenderCore",
				"RHI"
			});

		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			});
	}
}
