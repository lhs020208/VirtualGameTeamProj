using UnrealBuildTool;
public class DEMPrototype : ModuleRules {
 public DEMPrototype(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","InputCore"});
 }
}
