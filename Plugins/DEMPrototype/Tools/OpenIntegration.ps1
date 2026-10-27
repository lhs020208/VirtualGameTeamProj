$ErrorActionPreference='Stop'
if(Get-Process UnrealEditor -ErrorAction SilentlyContinue){throw 'Editor already running; preserve current session.'}
Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '"C:\GitHub\VirtualGameTeamProj\VirtualGameTeamProj.uproject" /Game/DEM/DEM_Integration -log'
