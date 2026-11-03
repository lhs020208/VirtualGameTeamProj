$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
& $Editor "$ProjectRoot/VirtualGameTeamProj.uproject" /Game/ProcessUI/Maps/L_ProcessUI_Test -game -ProcessUISmoke -windowed -ResX=1600 -ResY=900 -nosound -unattended -nop4 -NoSplash "-abslog=$ProjectRoot/Saved/ProcessUIPreview.log"
exit $LASTEXITCODE
