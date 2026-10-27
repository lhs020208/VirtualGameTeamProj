param([ValidateSet('Build','BuildCore','Core','Create','Verify','Probe')][string]$Task='Build')
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$plugin=Join-Path $project 'Plugins\DEMPrototype'
$engine='C:\Program Files\Epic Games\UE_5.8\Engine'
$results=Join-Path $plugin 'CommonValidation'
New-Item -ItemType Directory -Force $results | Out-Null
if($Task -eq 'Probe') {
 Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Select-Object ProcessId,ExecutablePath,CommandLine | ConvertTo-Json | Tee-Object "$results\editor_process.json"
 $node=Get-ChildItem "$env:LOCALAPPDATA\OpenAI\Codex\runtimes\cua_node\*\bin\node.exe" | Select-Object -First 1 -ExpandProperty FullName
 & $node (Join-Path $PSScriptRoot 'EmitterMCPProbe.mjs') | Tee-Object "$results\mcp.json"
 exit $LASTEXITCODE
}
if($Task -eq 'BuildCore') {
 $bat=Join-Path $results 'compile_core.bat'
 @('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"',('cd /d "'+$results+'"'),('cl /nologo /std:c++17 /EHsc /O2 /fp:precise /W4 /I"'+$plugin+'\Source\DEMPrototype\Public" "'+$plugin+'\Tests\common_core.cpp" /Fe:common_core.exe')) | Set-Content $bat
 & cmd.exe /c $bat 2>&1 | Tee-Object "$results\core_build.log"; exit $LASTEXITCODE
}
if($Task -eq 'Core') { & "$results\common_core.exe" $results | Tee-Object "$results\core_run.log"; exit $LASTEXITCODE }
if($Task -eq 'Build') {
 & "$engine\Build\BatchFiles\Build.bat" VirtualGameTeamProjEditor Win64 Development "-Project=$project\VirtualGameTeamProj.uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object "$results\build.log";exit $LASTEXITCODE
}
$mode=if($Task -eq 'Create'){'-CreateAssets'}else{'-Verify'}
& "$engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$project\VirtualGameTeamProj.uproject" -run=DEMCommon $mode -unattended -nullrhi -nosound -nop4 "-abslog=$results\$($Task.ToLower()).log"
exit $LASTEXITCODE
