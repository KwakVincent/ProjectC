// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class BattleRobot : ModuleRules
{
	public BattleRobot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"BattleRobot",
			"BattleRobot/Variant_Platforming",
			"BattleRobot/Variant_Platforming/Animation",
			"BattleRobot/Variant_Combat",
			"BattleRobot/Variant_Combat/AI",
			"BattleRobot/Variant_Combat/Animation",
			"BattleRobot/Variant_Combat/Gameplay",
			"BattleRobot/Variant_Combat/Interfaces",
			"BattleRobot/Variant_Combat/UI",
			"BattleRobot/Variant_SideScrolling",
			"BattleRobot/Variant_SideScrolling/AI",
			"BattleRobot/Variant_SideScrolling/Gameplay",
			"BattleRobot/Variant_SideScrolling/Interfaces",
			"BattleRobot/Variant_SideScrolling/UI"
		});

		string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty"));

		string WhisperPath = Path.Combine(ThirdPartyPath, "Whisper");
		PublicIncludePaths.Add(Path.Combine(WhisperPath, "Include"));

		string WhisperLibDir = Path.Combine(WhisperPath, "Lib", (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release");
		PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "whisper.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "ggml.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "ggml-base.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "ggml-cpu.lib"));

		string OnnxPath = Path.Combine(ThirdPartyPath, "Onnx");
		PublicIncludePaths.Add(Path.Combine(OnnxPath, "Include"));
		PublicAdditionalLibraries.Add(Path.Combine(OnnxPath, "Lib", "onnxruntime.lib"));
		PublicDelayLoadDLLs.Add("onnxruntime.dll");

		string OnnxDllSource = Path.Combine(OnnxPath, "Lib", "onnxruntime.dll");
		RuntimeDependencies.Add("$(TargetOutputDir)/onnxruntime.dll", OnnxDllSource);
	}
}
