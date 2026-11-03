using UnrealBuildTool;
public class ProcessUI : ModuleRules
{
    public ProcessUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new [] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new [] { "InputCore", "Slate", "SlateCore", "Json" });
    }
}
