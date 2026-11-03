using UnrealBuildTool;
public class ProcessUIEditor : ModuleRules
{
    public ProcessUIEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new [] { "Core", "CoreUObject", "Engine", "UnrealEd", "ProcessUI" });
    }
}
