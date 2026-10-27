param([ValidateSet('Format','StopOldTest','BuildTests','Profile','Basic','Candidate','Deposit','Bench','Build','RebuildMesh','Map','Run','Suite','Hardware')][string]$Task='BuildTests',[string]$Extra='')
$ErrorActionPreference='Stop'
$Project='C:\GitHub\VirtualGameTeamProj'
$Plugin=Join-Path $Project 'Plugins\DEMPrototype'
$Engine='C:\Program Files\Epic Games\UE_5.8\Engine'
$Results=Join-Path $Plugin 'Results'
if($Task -eq 'Format'){Get-ChildItem "$Plugin\Source","$Plugin\Tests" -Recurse -Include *.h,*.cpp | ForEach-Object {& 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe' -i $_.FullName}; exit}
if($Task -eq 'StopOldTest'){Get-Process validate -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq (Join-Path $Results 'validate.exe')} | Stop-Process; exit}
if($Task -eq 'Hardware') {
 $info=@{CPU=(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);GPU=(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion);RAM=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory}
 $info | ConvertTo-Json -Depth 4 | Set-Content "$Results\hardware.json"; Get-Content "$Results\hardware.json"; exit
}
if($Task -in @('BuildTests','Profile')) {
 $bat=Join-Path $Results 'compile.bat'
 @('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"',('cd /d "'+$Results+'"'),('cl /nologo /std:c++17 /EHsc /O2 /fp:precise /W4 /I"'+$Plugin+'\Source\DEMPrototype\Public" "'+$Plugin+'\Tests\validate.cpp" /Fe:validate_final.exe')) | Set-Content $bat
 if($Task -eq 'Profile') {(Get-Content $bat -Raw).Replace('validate.cpp','profile.cpp').Replace('validate_final.exe','profile.exe') | Set-Content $bat}
 & cmd.exe /c $bat; if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}; if($Task -eq 'Profile'){& "$Results\profile.exe" | Tee-Object -FilePath "$Results\profile.csv"}; exit $LASTEXITCODE
}
if($Task -in @('Basic','Candidate','Deposit','Bench')) {& "$Results\validate_final.exe" $Task.ToLower() $Results; exit $LASTEXITCODE}
if($Task -eq 'Build') {& "$Engine\Build\BatchFiles\Build.bat" VirtualGameTeamProjEditor Win64 Development "-Project=$Project\VirtualGameTeamProj.uproject" -WaitMutex -NoHotReloadFromIDE; exit $LASTEXITCODE}
if($Task -eq 'Map') {& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project\VirtualGameTeamProj.uproject" -run=DEMSandbox -unattended -nullrhi -nosound "-abslog=$Results\map.log"; exit $LASTEXITCODE}
if($Task -eq 'Run') {
 $opts=@($Extra -split ' ' | Where-Object {$_})
 $process=Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList (@("$Project\VirtualGameTeamProj.uproject",'/DEMPrototype/DEM_Sandbox','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-noraytracing')+$opts+@("-abslog=$Results\runtime.log")) -Wait -PassThru
 exit $process.ExitCode
}

if($Task -eq 'Suite') {
 foreach($n in @(200,1000,5000)) {foreach($mode in @('physics','low','high')) {foreach($repeat in @(0,1,2)) {
  $tag="${mode}_${n}_${repeat}"
  $opts=@('-DEMAuto','-DEMDense',"-DEMCount=$n","-DEMTag=$tag",'-noraytracing','-unattended','-NoSplash')
  if($mode -eq 'physics'){$opts+='-DEMPhysicsOnly'}
  if($mode -eq 'high'){$opts+='-DEMHigh'}
  if(Test-Path "$Results\${tag}_render.csv"){Write-Output "EXISTS $tag";continue}
  Write-Output "START $tag"
  $process=Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList (@("$Project\VirtualGameTeamProj.uproject",'/DEMPrototype/DEM_Sandbox','-game','-windowed','-ResX=1280','-ResY=720','-nosound') + $opts + @("-abslog=$Results\${tag}.log")) -Wait -PassThru
  if($process.ExitCode -ne 0){throw "Standalone failed: $tag"}
  if(!(Test-Path "$Results\${tag}_render.csv")){throw "Missing benchmark: $tag"}
  Write-Output "DONE $tag"
 }}}
}

if($Task -eq 'RebuildMesh') {& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project\VirtualGameTeamProj.uproject" -run=DEMSandbox -DEMMeshOnly -unattended -nullrhi -nosound "-abslog=$Results\mesh_rebuild.log"; exit $LASTEXITCODE}
