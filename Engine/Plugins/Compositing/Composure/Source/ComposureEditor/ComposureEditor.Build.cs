// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ComposureEditor : ModuleRules
	{
		public ComposureEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
			});

			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
			});

			PrivateDependencyModuleNames.AddRange(new string[] {
				"Composure",
				"Layers",
				"EditorWidgets",
				"MovieScene",
				"MovieSceneTracks",
				"MovieSceneTools",
				"Sequencer",
				"Slate",
				"SlateCore",
				"TimeManagement",
				"DesktopWidgets",
				"EditorStyle",
				"UnrealEd"
			});
		}
	}
}
