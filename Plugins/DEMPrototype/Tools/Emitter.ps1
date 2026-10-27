param([ValidateSet('Probe','Status','Build','Create','Verify')][string]$Task='Probe')
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$engine='C:\Program Files\Epic Games\UE_5.8\Engine'
$results=Join-Path $project 'Plugins\DEMPrototype\EmitterValidation'
$uproject=Join-Path $project 'VirtualGameTeamProj.uproject'
New-Item -ItemType Directory -Force $results | Out-Null
if ($Task -eq 'Status') {
    $running=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -like '*VirtualGameTeamProj.uproject*'} | Select-Object ProcessId,ExecutablePath,CommandLine)
    ConvertTo-Json -InputObject $running | Tee-Object "$results\editor_status.json"
    exit 0
}
if ($Task -eq 'Probe') {
    Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Select-Object ProcessId,ExecutablePath,CommandLine | ConvertTo-Json | Tee-Object "$results\editor_process.json"
    $node=Get-ChildItem "$env:LOCALAPPDATA\OpenAI\Codex\runtimes\cua_node\*\bin\node.exe" | Select-Object -First 1 -ExpandProperty FullName
    & $node (Join-Path $PSScriptRoot 'EmitterMCPProbe.mjs') | Tee-Object "$results\mcp.json"
    exit $LASTEXITCODE
}
if ($Task -eq 'Build') {
    & "$engine\Build\BatchFiles\Build.bat" VirtualGameTeamProjEditor Win64 Development "-Project=$uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object "$results\build.log"
    exit $LASTEXITCODE
}
$mode=if ($Task -eq 'Create') {'-CreateBlueprint'} else {'-Verify'}
& "$engine\Binaries\Win64\UnrealEditor-Cmd.exe" $uproject -run=DEMEmitter $mode -unattended -nullrhi -nosound -nop4 "-abslog=$results\$($Task.ToLower()).log"
exit $LASTEXITCODE
