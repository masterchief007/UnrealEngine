// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataprepEditor : ModuleRules
	{
		public DataprepEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"ApplicationCore",
					"AssetRegistry",
					"AssetTools",
					"BlueprintGraph",
					"Blutility",
					"Core",
					"CoreUObject",
					"DataprepCore",
					"DesktopPlatform",
					"EditorStyle",
					"EditorWidgets",
					"EditorWidgets",
					"Engine",
					"GraphEditor",
					"InputCore",
					"Kismet",
					"KismetCompiler",
					"KismetWidgets",
					"MainFrame",
					"MeshDescription",
					"MeshUtilities",
					"MeshUtilitiesCommon",
					"MessageLog",
					"Projects",
					"PropertyEditor",
					"RHI",
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"StatsViewer",
					"ToolMenus",
					"UnrealEd",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"DataprepCore/Private/Shared",
				});
		}
	}
}
