param([ValidateSet('Build','Create','Verify','BuildIsolated','CreateIsolated','VerifyIsolated','ImportIsolated')][string]$Task='Build')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$stage = Join-Path $project 'Saved\DEMParticleSettingsBuild'
if ($Task -eq 'BuildIsolated') {
    if (Test-Path $stage) { throw "Refusing to overwrite existing isolated folder: $stage" }
    New-Item -ItemType Directory $stage | Out-Null
    Copy-Item (Join-Path $project 'VirtualGameTeamProj.uproject') $stage
    Copy-Item (Join-Path $project 'Source') $stage -Recurse
    $stagePlugin = Join-Path $stage 'Plugins\DEMPrototype'
    New-Item -ItemType Directory $stagePlugin -Force | Out-Null
    Copy-Item (Join-Path $project 'Plugins\DEMPrototype\DEMPrototype.uplugin') $stagePlugin
    Copy-Item (Join-Path $project 'Plugins\DEMPrototype\Source') $stagePlugin -Recurse
    New-Item -ItemType Directory (Join-Path $stage 'Content\DEM') -Force | Out-Null
    Copy-Item (Join-Path $project 'Content\DEM\SM_DEMSphere.uasset') (Join-Path $stage 'Content\DEM')
}
if ($Task -eq 'ImportIsolated') {
    $source = Join-Path $stage 'Content\DEM\Settings\DA_DEMParticle_Default.uasset'
    $destination = Join-Path $project 'Content\DEM\Settings\DA_DEMParticle_Default.uasset'
    $report = Join-Path $stage 'Plugins\DEMPrototype\ParticleSettingsValidation\verification.txt'
    if (!(Test-Path $report) -or !((Get-Content $report -Raw) -match 'TOTAL checks=\d+ failures=0')) { throw 'Isolated verification has not passed' }
    if (Test-Path $destination) { throw 'Refusing to overwrite an existing default asset' }
    New-Item -ItemType Directory (Split-Path $destination) -Force | Out-Null
    Copy-Item $source $destination
    if ((Get-FileHash $source).Hash -ne (Get-FileHash $destination).Hash) { throw 'Copied asset hash mismatch' }
    Write-Output 'Verified asset copied to original project with identical SHA256.'
    exit 0
}
if ($Task.EndsWith('Isolated')) { $project = $stage }
$results = Join-Path $project 'Plugins\DEMPrototype\ParticleSettingsValidation'
$uproject = Join-Path $project 'VirtualGameTeamProj.uproject'
New-Item -ItemType Directory -Force $results | Out-Null
if ($Task -in @('Build','BuildIsolated')) {
    & "$engine\Build\BatchFiles\Build.bat" VirtualGameTeamProjEditor Win64 Development "-Project=$uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object (Join-Path $results 'build.log')
    exit $LASTEXITCODE
}
$mode = if ($Task -in @('Create','CreateIsolated')) { '-CreateDefault' } else { '-Verify' }
& "$engine\Binaries\Win64\UnrealEditor-Cmd.exe" $uproject -run=DEMParticleSettings $mode -unattended -nullrhi -nosound -nop4 "-abslog=$results\$($Task.ToLower()).log"
exit $LASTEXITCODE
