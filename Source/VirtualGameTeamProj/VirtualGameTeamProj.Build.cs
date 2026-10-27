// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualGameTeamProj : ModuleRules
{
	public VirtualGameTeamProj(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false; // Core translation units contain private helper namespaces.
        bEnableExceptions = true; // Solver rejects invalid steps without committing state.
        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../../Physics/PowderCore/third_party/OpenCL"));
        PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../../Physics/PowderCore/include"));
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore", "RenderCore", "DEMPrototype" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
