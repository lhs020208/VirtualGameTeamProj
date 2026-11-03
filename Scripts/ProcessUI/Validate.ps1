$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $Editor "$ProjectRoot/VirtualGameTeamProj.uproject" -run=ProcessUIValidate -unattended -nop4 -nullrhi -nosound -UTF8Output "-abslog=$ProjectRoot/Saved/ProcessUIValidate.log"
exit $LASTEXITCODE
