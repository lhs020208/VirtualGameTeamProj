param([switch]$Mock, [int]$Port=8765, [int]$IngestPort=8766,
 [ValidateSet('pressure','batch','manual','flowing','paused','stale','unknown','unsupported','error','complete','long','tiny','expanded')][string]$Scenario='manual')
$ErrorActionPreference='Stop'
if(-not (Get-Command node -ErrorAction SilentlyContinue)){throw 'Node.js 20 이상을 설치하고 PowerShell을 다시 여세요.'}
$viewerArgs=@((Join-Path $PSScriptRoot 'server.mjs'),'--port',"$Port",'--ingest-port',"$IngestPort")
if($Mock){$viewerArgs+=@('--mock','--scenario',$Scenario)}
# Does not start PIE, claim control, or open/focus a browser.
& node @viewerArgs
exit $LASTEXITCODE
