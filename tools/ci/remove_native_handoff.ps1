param([Parameter(Mandatory=$true)][string]$Receipt)
$ErrorActionPreference='Stop'
$record=Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json
if(-not $record.install_run_verified -or $record.uninstall_verified) { throw 'Need one verified, not-yet-removed install' }
$workRoot=[IO.Path]::GetFullPath($record.work).TrimEnd('\')
$installRoot=Join-Path $workRoot 'installed'
# All validation finishes before the first removal. Only manifest-owned files
# are removed; user-added files and derived consumer products remain untouched.
foreach($entry in $record.installed_files.PSObject.Properties) {
    $target=[IO.Path]::GetFullPath($entry.Name)
    if(-not $target.StartsWith($installRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Install member outside owned prefix' }
    $item=Get-Item -LiteralPath $target -Force
    if($item.PSIsContainer) { throw 'Install member is not a regular file' }
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Install member is a reparse point' }
    for($part=$item.Directory; $null -ne $part; $part=$part.Parent) {
        if($part.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Install path contains a reparse point' }
    }
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { throw "Modified install member: $target" }
}
foreach($entry in $record.protected_user_files.PSObject.Properties) {
    $target=[IO.Path]::GetFullPath($entry.Name)
    if(-not $target.StartsWith($workRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Protected file outside handoff prefix' }
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { throw 'Protected output changed before removal' }
}
foreach($entry in $record.installed_files.PSObject.Properties) {
    Remove-Item -LiteralPath $entry.Name -Force
}
foreach($entry in $record.installed_files.PSObject.Properties) {
    if(Test-Path -LiteralPath $entry.Name) { throw 'Installed file remains after removal' }
}
foreach($entry in $record.protected_user_files.PSObject.Properties) {
    if((Get-FileHash -LiteralPath $entry.Name -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { throw 'Protected output changed during removal' }
}
$record.state='passed'
$record.uninstall_verified=$true
$record | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $Receipt -Encoding utf8
Write-Output 'Manifest-owned native app/SDK removed; research outputs and in-prefix user notes preserved.'
