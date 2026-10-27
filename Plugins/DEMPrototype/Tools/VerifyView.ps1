$project='C:\GitHub\VirtualGameTeamProj'
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$project\VirtualGameTeamProj.uproject" -run=pythonscript "-script=$project\Plugins\DEMPrototype\Tools\VerifyIntegrationView.py" -unattended -nullrhi -nosound -nop4 "-abslog=$project\Plugins\DEMPrototype\CommonValidation\view_reload.log"
exit $LASTEXITCODE
