$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$contentRoot = (Resolve-Path -LiteralPath (Join-Path $projectRoot 'Content')).Path
$expectedRoot = [IO.Path]::GetFullPath('D:\GitHub\VirtualGameTeamProj\Content')
if ($contentRoot -ne $expectedRoot) { throw "Unexpected cleanup root: $contentRoot" }
$auditPath = Join-Path $projectRoot 'Docs\Demo200kSetup\asset_audit.json'
$audit = Get-Content -LiteralPath $auditPath -Raw | ConvertFrom-Json
if ($audit.unreachable.Count -ne 0) { throw 'Unexpected unreferenced assets; inspect separately.' }
$beforeFiles = @(Get-ChildItem -LiteralPath $contentRoot -File -Recurse -Force | ForEach-Object FullName | Sort-Object)
$removed = [Collections.Generic.List[string]]::new()
$folders = @(Get-ChildItem -LiteralPath $contentRoot -Directory -Recurse -Force | Sort-Object @{Expression={$_.FullName.Length};Descending=$true})
foreach ($folder in $folders) {
    $target = [IO.Path]::GetFullPath($folder.FullName)
    if (-not $target.StartsWith($contentRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Outside Content: $target" }
    if (($folder.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
    if (@(Get-ChildItem -LiteralPath $target -Force).Count -eq 0) {
        # Non-recursive: fail rather than delete if a file appears concurrently.
        [IO.Directory]::Delete($target, $false)
        $removed.Add($target.Substring($contentRoot.Length + 1))
    }
}
$afterFiles = @(Get-ChildItem -LiteralPath $contentRoot -File -Recurse -Force | ForEach-Object FullName | Sort-Object)
if (@(Compare-Object $beforeFiles $afterFiles).Count -ne 0) { throw 'Content file list changed during cleanup.' }
$receipt = [ordered]@{root=$contentRoot;removed_empty_directories=@($removed);removed_count=$removed.Count;asset_files_before=$beforeFiles.Count;asset_files_after=$afterFiles.Count;assets_deleted=0}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'Docs\Demo200kSetup\cleanup.json') -Encoding utf8
"Removed $($removed.Count) empty directories; preserved all $($afterFiles.Count) Content files."
