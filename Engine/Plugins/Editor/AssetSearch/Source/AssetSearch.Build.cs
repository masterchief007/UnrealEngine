// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetSearch : ModuleRules
{
	public AssetSearch(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"SQLiteCore",
				"EditorStyle",
				"InputCore",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"DerivedDataCache",
				"WorkspaceMenuStructure",
				"UnrealEd",
				"Json",
				"AssetRegistry",
				"JsonUtilities",
				"Projects",
				"PropertyPath",
				"UMG",
				"UMGEditor",
				"BlueprintGraph",
				"DeveloperSettings"
			}
		);
	}
}
