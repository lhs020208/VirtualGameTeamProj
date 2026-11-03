$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8'
$BuildArgs = @('VirtualGameTeamProjEditor','Win64','Development',"-Project=$ProjectRoot/VirtualGameTeamProj.uproject",'-WaitMutex','-NoHotReloadFromIDE','-NoLiveCoding')
$UseSuffix = @(Get-Process UnrealEditor -ErrorAction SilentlyContinue).Count -gt 0
if ($UseSuffix) {
    $BuildSuffix = Get-Random -Minimum 100000 -Maximum 999999
    $BuildArgs += "-ModuleWithSuffix=ProcessUI,$BuildSuffix"
    $BuildArgs += "-ModuleWithSuffix=ProcessUIEditor,$BuildSuffix"
    $BuildArgs += "-ModuleWithSuffix=VirtualGameTeamProj,$BuildSuffix"
} else {
    $BuildArgs += '-Module=ProcessUI'
    $BuildArgs += '-Module=ProcessUIEditor'
}
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" @BuildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($UseSuffix) {
    $Manifest = Get-Content "$ProjectRoot/Plugins/ProcessUI/Binaries/Win64/UnrealEditor.modules" -Raw | ConvertFrom-Json
    foreach ($Entry in $Manifest.Modules.PSObject.Properties) {
        if (-not (Test-Path "$ProjectRoot/Plugins/ProcessUI/Binaries/Win64/$($Entry.Value)")) { throw "Missing output DLL: $($Entry.Value)" }
    }
    exit 0
}
# A module-only UBT build excludes its metadata action. Scope the UBT-generated
# metadata to this plugin, preserving the engine's actual build ID and main DLL.
$MetadataFile = 'TargetMetadata.json'
$Metadata = Get-Content "$ProjectRoot/Intermediate/Build/Win64/x64/VirtualGameTeamProjEditor/Development/$MetadataFile" -Raw | ConvertFrom-Json
$PluginManifests = @{}
foreach ($Property in $Metadata.FileToManifest.PSObject.Properties) {
    if ($Property.Name -like '*\Plugins\ProcessUI\*') { $PluginManifests[$Property.Name] = $Property.Value }
}
if ($PluginManifests.Count -ne 1) { throw 'Expected exactly one ProcessUI manifest from UBT.' }
$Metadata.Version = $Metadata.Receipt.Version
$Metadata.VersionFile = $null
$Metadata.Receipt = $null
$Metadata.ReceiptFile = $null
$Metadata.FileToManifest = $PluginManifests
$Metadata.FileToLoadOrderManifest = @{}
$MetadataPath = "$ProjectRoot/Intermediate/Build/ProcessUIMetadata.json"
$Metadata | ConvertTo-Json -Depth 50 | Set-Content $MetadataPath -Encoding UTF8
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" -Mode=WriteMetadata "-Input=$MetadataPath" -Version=2
exit $LASTEXITCODE
