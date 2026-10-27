using UnrealBuildTool;
public class DEMPrototypeEditor : ModuleRules {
 public DEMPrototypeEditor(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","UnrealEd","DEMPrototype","MeshDescription","StaticMeshDescription"});
 }
}
