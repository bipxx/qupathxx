param([Parameter(Mandatory=$true)][string]$Work)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$workRoot=[IO.Path]::GetFullPath($Work).TrimEnd('\')
if(-not $workRoot.StartsWith((Join-Path $projectRoot 'build')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Work directory outside owned build tree' }
$receiptPath=Join-Path $workRoot 'handoff.json'
$receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
foreach($driver in $receipt.test_driver_files) {
    $resolved=[IO.Path]::GetFullPath($driver)
    if(-not $resolved.StartsWith($workRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Driver outside work directory' }
    Remove-Item -LiteralPath $resolved -Force
}
& $receipt.installed_python -I -m pip uninstall --yes qupathxx
if($LASTEXITCODE -ne 0) { throw 'Wheel uninstall failed' }
& $receipt.installed_python -I -c 'import importlib.util; assert importlib.util.find_spec("qupathxx") is None'
if($LASTEXITCODE -ne 0) { throw 'Package remains importable after uninstall' }
$nativeRoot=[IO.Path]::GetFullPath($receipt.native).TrimEnd('\')
if(-not $nativeRoot.StartsWith($workRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Native folder outside work directory' }
Remove-Item -LiteralPath $nativeRoot -Recurse -Force
if(Test-Path -LiteralPath $nativeRoot) { throw 'Native directory remains' }
foreach($entry in $receipt.protected_user_files.PSObject.Properties) {
    if((Get-FileHash -LiteralPath $entry.Name -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { throw "User output changed: $($entry.Name)" }
}
$receipt.uninstall_verified=$true
$receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $receiptPath
Write-Output 'Wheel and native removal passed; projects/CSVs remain byte-identical.'
