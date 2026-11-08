param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$taskBase = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath "$taskBase/dense-pid.txt") { throw 'Preserve prior output; choose a fresh directory' }
New-Item -ItemType Directory -Force -Path $taskBase | Out-Null
$taskRuntime = (Resolve-Path "$taskRoot/../../ci/gridfrontends167-package/Release").Path
$taskPython = (Resolve-Path "$taskRoot/../../../.venv/Scripts/python.exe").Path
$taskBootstrap = "$taskRoot/docs/benchmarks/cell-kernel-optimization/run-native.py"
$taskProbe = "$PSScriptRoot/dense-viewer.py"
$taskFixture = (Resolve-Path "$taskRoot/../../fixtures/large-pyramid.tif").Path
$taskWorker = Start-Process -FilePath $taskPython -ArgumentList @('-I',$taskBootstrap,"$taskRuntime/_native.cp312-win_amd64.pyd",$taskRuntime,$taskProbe,'--fixture',$taskFixture,'--gui','--output',"$taskBase/dense-viewer.json") -WindowStyle Hidden -RedirectStandardOutput "$taskBase/dense-viewer.log" -RedirectStandardError "$taskBase/dense-viewer.stderr.log" -PassThru
$taskDeadline = (Get-Date).AddSeconds(180)
while (-not (Test-Path -LiteralPath "$taskBase/dense-pid.txt")) { if ((Get-Date) -gt $taskDeadline) { throw 'PID timeout' }; Start-Sleep -Milliseconds 100 }
$taskEngineId = [int](Get-Content "$taskBase/dense-pid.txt")
$taskRows = @()
while (-not $taskWorker.HasExited) {
 $taskMemory = Get-Process -Id $taskEngineId -ErrorAction SilentlyContinue
 $taskCounters = (Get-Counter @('\GPU Process Memory(*)\Dedicated Usage','\GPU Process Memory(*)\Shared Usage','\GPU Process Memory(*)\Total Committed') -MaxSamples 1 -ErrorAction SilentlyContinue).CounterSamples | Where-Object { $_.InstanceName -like "pid_${taskEngineId}_*" }
 $taskRows += [pscustomobject]@{time_utc=(Get-Date).ToUniversalTime().ToString('o');engine_pid=$taskEngineId;rss=$taskMemory.WorkingSet64;peak_rss=$taskMemory.PeakWorkingSet64;private=$taskMemory.PrivateMemorySize64;gpu=@($taskCounters | Select-Object Path,InstanceName,CookedValue,Status)}
 if ((Get-Date) -gt $taskDeadline) { throw 'Dense acceptance exceeded180 seconds' }
 Start-Sleep -Milliseconds 200
}
$taskWorker.WaitForExit()
$taskRows | ConvertTo-Json -Depth 6 | Set-Content "$taskBase/dense-resources.json" -Encoding utf8
if ($taskWorker.ExitCode -ne 0) { throw 'Dense viewer failed; inspect preserved logs' }
