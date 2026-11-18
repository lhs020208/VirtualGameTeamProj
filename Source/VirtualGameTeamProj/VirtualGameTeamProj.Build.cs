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
        string ProjectRoot = System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../.."));
        string CudaLibrary = System.IO.Path.Combine(ProjectRoot, "Intermediate/CUDA/powder_plant_cuda.lib");
        string CudaRuntime = System.IO.Path.Combine(ProjectRoot, "Intermediate/CUDAToolkit/lib/x64/cudart_static.lib");
        if (Target.Platform == UnrealTargetPlatform.Win64 && System.IO.File.Exists(CudaLibrary) && System.IO.File.Exists(CudaRuntime))
        {
            PrivateDefinitions.Add("POWDER_WITH_CUDA=1");
            PublicAdditionalLibraries.Add(CudaLibrary);
            ExternalDependencies.Add(CudaLibrary);
            PublicAdditionalLibraries.Add(CudaRuntime);
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ProjectRoot, "Intermediate/CUDAToolkit/lib/x64/cuda.lib"));
            RuntimeDependencies.Add(System.IO.Path.Combine(ProjectRoot, "SourceAssets/PowderCUDA/plant_sm89.ptx"));
        }
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore", "RenderCore", "DEMPrototype" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Json" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Messaging", "MessagingCommon", "EngineMessages", "SessionMessages" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
