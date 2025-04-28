// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class IWToUE : ModuleRules
{
	public IWToUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		var engineRoot = Path.GetFullPath(Target.RelativeEnginePath);

		string DirectXTexPath = Path.Combine(ModuleDirectory, "ThirdParty", "DirectXTex");

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(DirectXTexPath, "Include")
			}
		);

		string LibPath = Path.Combine(DirectXTexPath, "Lib");
		PublicAdditionalLibraries.Add(Path.Combine(LibPath, "DirectXTex.lib"));
		
		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PhysicsCore",
				"PhysicsUtilities",
				"OodleDataCompression",
				"SQLiteCore",
				"SQLiteSupport",
				"Projects",
				"libOpus",
				"RHI",
				"RenderCore",
				"EditorSubsystem"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"SkeletalMeshUtilitiesCommon",
				"InputCore",
				"ToolMenus",
				"EditorStyle",
				"Json",
				"JsonUtilities",
				"ToolWidgets",
				"ApplicationCore",
				"MessageLog",
				"HTTP",
				"EditorScriptingUtilities"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}