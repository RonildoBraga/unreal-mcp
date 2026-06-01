// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// Use IWYUSupport instead of the deprecated bEnforceIWYU in UE5.5
		IWYUSupport = IWYUSupport.Full;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"RenderCore"   // v0.7.7: FlushRenderingCommands for take_screenshot redraw fix
			}
		);
		// v0.7.8 cleanup: dropped HTTP — no actual HTTP request types referenced anywhere
		// in the module (only a stale `#include "Http.h"` in UnrealMCPBridge.h, also removed).

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"LevelEditor",        // Sprint 1: ULevelEditorSubsystem (open/save level tools)
				"MaterialEditor",     // Sprint 2: UMaterialEditingLibrary (MI tuning + creation)
				"Landscape",          // v0.9.x: ALandscape::Import for create_landscape
				"Slate",
				"SlateCore",
				"UMG",
				"Kismet",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"PythonScriptPlugin"  // v0.8.1: execute_python escape hatch (IPythonScriptPlugin::ExecPythonCommandEx)
			}
		);
		// v0.7.8 cleanup: dropped KismetCompiler — no FKismetCompiler* APIs used anywhere.

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UMGEditor"   // For WidgetBlueprint.h and other UMG editor functionality
				}
			);
			// v0.7.8 cleanup: dropped PropertyEditor / ToolMenus / BlueprintEditorLibrary —
			// not referenced anywhere in the module. UMGEditor is the only remaining
			// editor-only dependency.
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
} 